#include "oal/driver_api.h"

// Canon EDSDK 13.20.x ships Linux headers that still use the MSVC-only
// `__int64` spelling and the Windows `WCHAR` alias even when linking the
// vendor-provided ELF ARM/x86_64 libraries.  Provide translation-unit-local
// compatibility shims instead of modifying the vendor SDK in-place.
#if defined(__linux__) && !defined(_WIN32)
#include <cwchar>
#ifndef __int64
#define __int64 long long
#define OAL_CANON_EDSDK_UNDEF_INT64_AFTER_INCLUDE 1
#endif
#ifndef WCHAR
using WCHAR = wchar_t;
#endif
#endif

#include <EDSDK.h>

#ifdef OAL_CANON_EDSDK_UNDEF_INT64_AFTER_INCLUDE
#undef __int64
#undef OAL_CANON_EDSDK_UNDEF_INT64_AFTER_INCLUDE
#endif

#include <QImage>
#include <QByteArray>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
OalDriverHostV2 host{};

struct CameraState {
    std::string id;
    std::string name;
    int index{-1};
    EdsCameraRef camera{nullptr};
    std::atomic_bool connected{false};
    std::atomic_bool abortRequested{false};
    std::atomic_bool transportLost{false};
    std::mutex opMutex;
    std::mutex eventMutex;
    std::condition_variable eventCv;
    std::vector<EdsDirectoryItemRef> pendingItems;
};

struct DriverState {
    std::mutex mutex;
    std::unordered_map<std::string, std::unique_ptr<CameraState>> cameras;
    std::filesystem::path spoolDir;
    std::atomic_bool eventPumpStop{false};
    std::atomic_bool cameraAddedEvent{false};
    std::thread eventPumpThread;
    bool sdkInitialized{false};
} state;

char *copyString(const std::string &s) {
    auto *p = static_cast<char *>(host.allocate(host.hostContext, s.size() + 1));
    if (p) std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}
std::string quote(const std::string &s) {
    std::string o="\"";
    for (unsigned char c : s) { if (c=='\\') o+="\\\\"; else if(c=='\"') o+="\\\""; else if(c>=0x20) o+=char(c); }
    return o+'"';
}
const char *ok(const std::string &d="{}") { return copyString("{\"ok\":true,\"data\":"+d+"}"); }
const char *fail(const std::string &c,const std::string&m){return copyString("{\"ok\":false,\"error\":{\"code\":"+quote(c)+",\"message\":"+quote(m)+"}}");}
void emitEvent(const std::string&dev,const std::string&type,const std::string&payload="{}"){
    if(!host.emitEvent) return;
    const auto e="{\"type\":"+quote(type)+",\"payload\":"+payload+"}";
    host.emitEvent(host.hostContext,"oal.canon",dev.c_str(),e.c_str());
}
const char *edsErrorName(EdsError e){
    switch(e){
#ifdef EDS_ERR_TAKE_PICTURE_AF_NG
    case EDS_ERR_TAKE_PICTURE_AF_NG:return "TAKE_PICTURE_AF_NG";
#endif
#ifdef EDS_ERR_DEVICE_BUSY
    case EDS_ERR_DEVICE_BUSY:return "DEVICE_BUSY";
#endif
#ifdef EDS_ERR_INVALID_PARAMETER
    case EDS_ERR_INVALID_PARAMETER:return "INVALID_PARAMETER";
#endif
#ifdef EDS_ERR_NOT_SUPPORTED
    case EDS_ERR_NOT_SUPPORTED:return "NOT_SUPPORTED";
#endif
#ifdef EDS_ERR_OPERATION_REFUSED
    case EDS_ERR_OPERATION_REFUSED:return "OPERATION_REFUSED";
#endif
    default:return nullptr;
    }
}
std::string edsError(const char *what,EdsError e){std::ostringstream s;s<<what<<" failed (EDSDK 0x"<<std::hex<<e;if(const char*n=edsErrorName(e))s<<" "<<n;s<<")";return s.str();}
void log(int level,const std::string&m){if(host.log)host.log(host.hostContext,level,"oal.canon",m.c_str());}
std::string stringValue(const std::string&j,const std::string&k,const std::string&f={}){auto p=j.find("\""+k+"\"");if(p==std::string::npos)return f;auto c=j.find(':',p);auto q=j.find('"',c+1);if(c==std::string::npos||q==std::string::npos)return f;auto e=j.find('"',q+1);return e==std::string::npos?f:j.substr(q+1,e-q-1);}
double number(const std::string&j,const std::string&k,double f){auto p=j.find("\""+k+"\"");if(p==std::string::npos)return f;auto c=j.find(':',p);if(c==std::string::npos)return f;char*e=nullptr;double v=std::strtod(j.c_str()+c+1,&e);return e==j.c_str()+c+1?f:v;}

std::filesystem::path defaultSpoolDir(){
#ifdef _WIN32
    if(const char *p=std::getenv("USERPROFILE")) return std::filesystem::path(p)/"Pictures"/"OpenAstroLink"/"Canon";
#endif
    if(const char *p=std::getenv("HOME")) return std::filesystem::path(p)/"Pictures"/"OpenAstroLink"/"Canon";
    return std::filesystem::temp_directory_path()/"OpenAstroLink"/"Canon";
}

EdsError EDSCALLBACK cameraAdded(EdsVoid *){
    state.cameraAddedEvent=true;
    log(1,"Canon EDSDK camera-added event received; requesting automatic native rediscovery");
    emitEvent("", "device.discoveryHint", "{\"reason\":\"camera-added\"}");
    return EDS_ERR_OK;
}

EdsError EDSCALLBACK stateEvent(EdsStateEvent event,EdsUInt32,EdsVoid *context){
    auto *c=static_cast<CameraState*>(context);
    if(!c)return EDS_ERR_OK;
    if(event==kEdsStateEvent_Shutdown){
        const bool wasConnected=c->connected.exchange(false);
        c->transportLost=true;
        c->abortRequested=true;
        c->eventCv.notify_all();
        if(wasConnected){
            log(1,"Canon EDSDK shutdown/transport-loss event received for "+c->name);
            emitEvent(c->id,"device.disconnected","{\"reason\":\"edsdk-shutdown\",\"physical\":true}");
        }
    }
    return EDS_ERR_OK;
}

EdsError EDSCALLBACK objectEvent(EdsObjectEvent event,EdsBaseRef object,EdsVoid *context){
    auto *c=static_cast<CameraState*>(context);
    if(!c){ if(object) EdsRelease(object); return EDS_ERR_OK; }
    if(event==kEdsObjectEvent_DirItemRequestTransfer && object){
        {
            std::lock_guard<std::mutex> lk(c->eventMutex);
            c->pendingItems.push_back(static_cast<EdsDirectoryItemRef>(object));
        }
        c->eventCv.notify_all();
        return EDS_ERR_OK; // ownership retained until the capture path downloads/releases it
    }
    if(object) EdsRelease(object);
    return EDS_ERR_OK;
}

bool refreshEnumeration(std::string &error){
    // EDSDK can deliver kEdsStateEvent_CameraAdded slightly before the new body
    // becomes visible through EdsGetCameraList().  An explicit user Refresh made
    // immediately after the callback used to consume that edge and publish zero
    // devices forever.  If a camera-added edge is pending, give the SDK a short
    // bounded settle/retry window.  There is still no periodic background scan.
    EdsCameraListRef list=nullptr;
    EdsUInt32 count=0;
    EdsError rc=EDS_ERR_OK;
    const int attempts=state.cameraAddedEvent.load()?12:1;
    for(int attempt=0;attempt<attempts;++attempt){
        if(list){EdsRelease(list);list=nullptr;}
        rc=EdsGetCameraList(&list);
        if(rc!=EDS_ERR_OK){error=edsError("EdsGetCameraList",rc);if(list)EdsRelease(list);return false;}
        rc=EdsGetChildCount(list,&count);
        if(rc!=EDS_ERR_OK){EdsRelease(list);error=edsError("EdsGetChildCount",rc);return false;}
        if(count>0||attempt+1==attempts)break;
        // The regular event thread is already pumping EDSDK.  Only wait here;
        // avoid a second concurrent EdsGetEvent() call from the discovery thread.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::unordered_set<std::string> seen;
    std::lock_guard<std::mutex> lk(state.mutex);
    for(EdsUInt32 i=0;i<count;++i){
        EdsCameraRef cam=nullptr; if(EdsGetChildAtIndex(list,EdsInt32(i),&cam)!=EDS_ERR_OK||!cam)continue;
        EdsDeviceInfo info{}; if(EdsGetDeviceInfo(cam,&info)!=EDS_ERR_OK){EdsRelease(cam);continue;}
        std::string name=info.szDeviceDescription; if(name.empty())name="Canon EOS";
        std::string id="canon-edsdk:"+std::to_string(i)+":"+name;
        seen.insert(id);
        auto it=state.cameras.find(id);
        if(it==state.cameras.end()){
            auto c=std::make_unique<CameraState>(); c->id=id;c->name=name;c->index=int(i);state.cameras.emplace(id,std::move(c));
        }else it->second->index=int(i);
        EdsRelease(cam);
    }
    for(auto it=state.cameras.begin();it!=state.cameras.end();){
        if(!it->second->connected && !seen.count(it->first))it=state.cameras.erase(it); else ++it;
    }
    EdsRelease(list);
    if(count>0)state.cameraAddedEvent=false; // preserve a not-yet-enumerable attach edge
    return true;
}

bool openCamera(CameraState &c,std::string &error){
    if(c.connected)return true;
    EdsCameraListRef list=nullptr; EdsError rc=EdsGetCameraList(&list); if(rc!=EDS_ERR_OK){error=edsError("EdsGetCameraList",rc);return false;}
    EdsBaseRef child=nullptr; rc=EdsGetChildAtIndex(list,c.index,&child); EdsRelease(list);
    if(rc!=EDS_ERR_OK||!child){error=edsError("EdsGetChildAtIndex",rc);return false;}
    c.camera=static_cast<EdsCameraRef>(child);
    rc=EdsOpenSession(c.camera); if(rc!=EDS_ERR_OK){EdsRelease(c.camera);c.camera=nullptr;error=edsError("EdsOpenSession",rc);return false;}
    const auto failOpen=[&](const char *what,EdsError e){
        error=edsError(what,e);
        EdsCloseSession(c.camera);EdsRelease(c.camera);c.camera=nullptr;
        return false;
    };
    rc=EdsSetObjectEventHandler(c.camera,kEdsObjectEvent_All,&objectEvent,&c);
    if(rc!=EDS_ERR_OK)return failOpen("EdsSetObjectEventHandler",rc);
    rc=EdsSetCameraStateEventHandler(c.camera,kEdsStateEvent_All,&stateEvent,&c);
    if(rc!=EDS_ERR_OK)return failOpen("EdsSetCameraStateEventHandler",rc);
    EdsUInt32 saveTo=kEdsSaveTo_Host;
    rc=EdsSetPropertyData(c.camera,kEdsPropID_SaveTo,0,sizeof(saveTo),&saveTo);
    if(rc!=EDS_ERR_OK)return failOpen("EdsSetPropertyData(SaveTo=Host)",rc);
    EdsCapacity capacity{}; capacity.numberOfFreeClusters=0x7fffffff; capacity.bytesPerSector=0x1000; capacity.reset=1;
    rc=EdsSetCapacity(c.camera,capacity);
    if(rc!=EDS_ERR_OK)return failOpen("EdsSetCapacity",rc);
    log(1,"Canon EDSDK session ready: object/state handlers registered; SaveTo=Host; host capacity published");
    c.abortRequested=false;c.transportLost=false;c.connected=true; emitEvent(c.id,"device.connected"); return true;
}
void closeCamera(CameraState &c){
    const bool wasConnected=c.connected.exchange(false);
    c.abortRequested=true;
    {
        std::lock_guard<std::mutex> lk(c.eventMutex);
        for(auto *item:c.pendingItems) if(item) EdsRelease(item);
        c.pendingItems.clear();
    }
    if(c.camera){EdsCloseSession(c.camera);EdsRelease(c.camera);c.camera=nullptr;}
    if(wasConnected)emitEvent(c.id,"device.disconnected");
}

EdsDirectoryItemRef waitForItem(CameraState &c,std::chrono::milliseconds timeout,const OalDriverCallV2 *call){
    const auto deadline=std::chrono::steady_clock::now()+timeout;
    for(;;){
        {
            std::lock_guard<std::mutex> lk(c.eventMutex);
            if(!c.pendingItems.empty()){auto *item=c.pendingItems.front();c.pendingItems.erase(c.pendingItems.begin());return item;}
        }
        if(c.abortRequested || (call&&call->operationIdUtf8&&host.isCancellationRequested&&host.isCancellationRequested(host.hostContext,call->operationIdUtf8))) return nullptr;
        if(std::chrono::steady_clock::now()>=deadline)return nullptr;
        // The driver's global EdsGetEvent pump dispatches transfer callbacks.
        std::unique_lock<std::mutex> lk(c.eventMutex);
        c.eventCv.wait_for(lk,std::chrono::milliseconds(30));
    }
}

struct TvChoice { EdsUInt32 value; double seconds; };

struct IsoChoice { EdsUInt32 value; double iso; };

const std::vector<IsoChoice> &knownIsoChoices(){
    // Canon EDSDK kEdsPropID_ISOSpeed values. The body descriptor remains the
    // authority: this table only translates advertised EDSDK codes to the OAL
    // numeric gain semantic (ISO) and lets us choose the nearest supported ISO.
    static const std::vector<IsoChoice> v={
        {0x28,6},{0x30,12},{0x38,25},{0x40,50},{0x48,100},{0x4B,125},{0x4D,160},
        {0x50,200},{0x53,250},{0x55,320},{0x58,400},{0x5B,500},{0x5D,640},{0x60,800},
        {0x63,1000},{0x65,1250},{0x68,1600},{0x6B,2000},{0x6D,2500},{0x70,3200},
        {0x73,4000},{0x75,5000},{0x78,6400},{0x7B,8000},{0x7D,10000},{0x80,12800},
        {0x88,25600},{0x90,51200},{0x98,102400}
    };
    return v;
}

bool isoForCode(EdsUInt32 code,double &iso){
    for(const auto &x:knownIsoChoices())if(x.value==code){iso=x.iso;return true;}
    return false;
}

std::vector<IsoChoice> advertisedIsoChoices(CameraState &c){
    EdsPropertyDesc desc{};
    if(EdsGetPropertyDesc(c.camera,kEdsPropID_ISOSpeed,&desc)!=EDS_ERR_OK)return {};
    std::vector<IsoChoice> out;
    for(EdsInt32 i=0;i<desc.numElements;++i){
        const EdsUInt32 code=EdsUInt32(desc.propDesc[i]);double iso=0.0;
        if(isoForCode(code,iso))out.push_back({code,iso});
    }
    std::sort(out.begin(),out.end(),[](const IsoChoice&a,const IsoChoice&b){return a.iso<b.iso;});
    out.erase(std::unique(out.begin(),out.end(),[](const IsoChoice&a,const IsoChoice&b){return a.value==b.value;}),out.end());
    return out;
}

bool setIsoForGain(CameraState &c,double requested,double &actual,std::string &error){
    if(requested<=0){actual=0.0;return true;} // 0 means preserve the camera's current ISO.
    const auto choices=advertisedIsoChoices(c);
    if(choices.empty()){
        error="Canon reports no writable ISO choices in the current camera mode";
        return false;
    }
    const IsoChoice *best=&choices.front();double bestScore=1e100;
    for(const auto &x:choices){
        const double score=std::abs(std::log(x.iso/requested));
        if(score<bestScore){bestScore=score;best=&x;}
    }
    EdsUInt32 value=best->value;
    const EdsError rc=EdsSetPropertyData(c.camera,kEdsPropID_ISOSpeed,0,sizeof(value),&value);
    if(rc!=EDS_ERR_OK){error=edsError("EdsSetPropertyData(ISOSpeed)",rc);return false;}
    actual=best->iso;
    log(1,"Canon ISO requested="+std::to_string(requested)+" applied="+std::to_string(actual));
    return true;
}

const std::vector<TvChoice> &knownTvChoices(){
    static const std::vector<TvChoice> v={
        {0x10,30.0},{0x13,25.0},{0x14,20.0},{0x15,20.0},{0x18,15.0},{0x1B,13.0},{0x1C,10.0},{0x1D,10.0},
        {0x20,8.0},{0x23,6.0},{0x24,6.0},{0x25,5.0},{0x28,4.0},{0x2B,3.2},{0x2C,3.0},{0x2D,2.5},
        {0x30,2.0},{0x33,1.6},{0x34,1.5},{0x35,1.3},{0x38,1.0},{0x3B,0.8},{0x3C,0.7},{0x3D,0.6},
        {0x40,0.5},{0x43,0.4},{0x44,0.3},{0x45,0.3},{0x48,0.25},{0x4B,0.2},{0x4C,1.0/6.0},{0x4D,1.0/6.0},
        {0x50,0.125},{0x53,0.1},{0x54,0.1},{0x55,1.0/13.0},{0x58,1.0/15.0},{0x5B,1.0/20.0},{0x5C,1.0/20.0},
        {0x5D,1.0/25.0},{0x60,1.0/30.0},{0x63,1.0/40.0},{0x64,1.0/45.0},{0x65,1.0/50.0},{0x68,1.0/60.0},
        {0x6B,1.0/80.0},{0x6C,1.0/90.0},{0x6D,1.0/100.0},{0x70,1.0/125.0},{0x73,1.0/160.0},{0x74,1.0/180.0},
        {0x75,1.0/200.0},{0x78,1.0/250.0},{0x7B,1.0/320.0},{0x7C,1.0/350.0},{0x7D,1.0/400.0},{0x80,1.0/500.0},
        {0x83,1.0/640.0},{0x84,1.0/750.0},{0x85,1.0/800.0},{0x88,1.0/1000.0},{0x8B,1.0/1250.0},{0x8C,1.0/1500.0},
        {0x8D,1.0/1600.0},{0x90,1.0/2000.0},{0x93,1.0/2500.0},{0x94,1.0/3000.0},{0x95,1.0/3200.0},
        {0x98,1.0/4000.0},{0x9B,1.0/5000.0},{0x9C,1.0/6000.0},{0x9D,1.0/6400.0},{0xA0,1.0/8000.0}
    };return v;
}

bool setTvForExposure(CameraState &c,double requested,double &actual,std::string &error){
    EdsPropertyDesc desc{};EdsError rc=EdsGetPropertyDesc(c.camera,kEdsPropID_Tv,&desc);
    if(rc!=EDS_ERR_OK){error=edsError("EdsGetPropertyDesc(Tv)",rc)+"; put the EOS in Manual (M) mode";return false;}
    const auto &known=knownTvChoices();bool found=false;TvChoice best{};double bestScore=1e100;
    for(EdsInt32 i=0;i<desc.numElements;++i){
        const EdsUInt32 code=EdsUInt32(desc.propDesc[i]);
        for(const auto&t:known)if(t.value==code){const double score=std::abs(std::log(t.seconds/requested));if(score<bestScore){bestScore=score;best=t;found=true;}break;}
    }
    if(!found){error="Canon reports no usable Tv values; put the EOS in Manual (M) mode";return false;}
    EdsUInt32 tv=best.value;rc=EdsSetPropertyData(c.camera,kEdsPropID_Tv,0,sizeof(tv),&tv);
    if(rc!=EDS_ERR_OK){error=edsError("EdsSetPropertyData(Tv)",rc)+"; put the EOS in Manual (M) mode";return false;}
    actual=best.seconds;return true;
}

bool setBulbTv(CameraState &c,std::string &error){
    EdsPropertyDesc desc{};EdsError rc=EdsGetPropertyDesc(c.camera,kEdsPropID_Tv,&desc);
    if(rc!=EDS_ERR_OK){error=edsError("EdsGetPropertyDesc(Tv)",rc);return false;}
    bool advertised=false;for(EdsInt32 i=0;i<desc.numElements;++i)if(EdsUInt32(desc.propDesc[i])==0x0C){advertised=true;break;}
    // Older EOS bodies/firmware occasionally omit Bulb from the transient Tv
    // descriptor even though setting Tv=Bulb is accepted in Manual mode.  Try the
    // documented value once before declaring it unavailable.
    EdsUInt32 tv=0x0C;rc=EdsSetPropertyData(c.camera,kEdsPropID_Tv,0,sizeof(tv),&tv);
    if(rc!=EDS_ERR_OK){
        error=(advertised?edsError("EdsSetPropertyData(Tv=Bulb)",rc):
               std::string("Bulb is not available in the current EOS mode (Tv descriptor did not advertise Bulb; ")+edsError("direct Tv=Bulb",rc)+")")+
              "; switch the camera to Manual (M/Bulb-capable) mode";
        return false;
    }
    if(!advertised)log(1,"Canon accepted Tv=Bulb although the current Tv descriptor did not advertise it");
    return true;
}

void releaseShutter(CameraState &c){
    if(!c.camera)return;
    EdsSendCommand(c.camera,kEdsCameraCommand_PressShutterButton,kEdsCameraCommand_ShutterButton_OFF);
}

bool triggerNonAfStill(CameraState &c,std::string &error){
    // TakePicture follows the camera/lens AF policy and returns 0x8D01 when AF
    // cannot lock. Astronomy capture must never depend on lens autofocus.
    EdsError rc=EdsSendCommand(c.camera,kEdsCameraCommand_PressShutterButton,
                               kEdsCameraCommand_ShutterButton_Completely_NonAF);
    if(rc!=EDS_ERR_OK){releaseShutter(c);error=edsError("EdsSendCommand(PressShutterButton NonAF)",rc);return false;}
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    rc=EdsSendCommand(c.camera,kEdsCameraCommand_PressShutterButton,kEdsCameraCommand_ShutterButton_OFF);
    if(rc!=EDS_ERR_OK){error=edsError("EdsSendCommand(ShutterButton OFF)",rc);return false;}
    return true;
}

std::filesystem::path targetPathFor(const EdsDirectoryItemInfo &info, const std::string &requested) {
    std::filesystem::path target = requested.empty() ? state.spoolDir / std::filesystem::path(info.szFileName)
                                                     : std::filesystem::path(requested);
    if (!requested.empty() && !target.has_extension()) target /= info.szFileName;
    if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path());
    return target;
}

bool downloadThumbnailFile(EdsDirectoryItemRef item, std::vector<std::uint8_t> &bytes) {
    try {
        const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto tmp = std::filesystem::temp_directory_path() / ("oal-canon-thumb-" + std::to_string(stamp) + ".jpg");
        EdsStreamRef stream = nullptr;
        EdsError rc = EdsCreateFileStream(tmp.string().c_str(), kEdsFileCreateDisposition_CreateAlways,
                                          kEdsAccess_ReadWrite, &stream);
        if (rc != EDS_ERR_OK || !stream) return false;
        rc = EdsDownloadThumbnail(item, stream);
        EdsRelease(stream);
        if (rc != EDS_ERR_OK) { std::error_code ec; std::filesystem::remove(tmp, ec); return false; }
        std::ifstream f(tmp, std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        std::error_code ec; std::filesystem::remove(tmp, ec);
        return !bytes.empty();
    } catch (...) { return false; }
}

bool downloadOriginalFile(EdsDirectoryItemRef item, const EdsDirectoryItemInfo &info,
                          const std::filesystem::path &target, std::string &error) {
    EdsStreamRef stream = nullptr;
    EdsError rc = EdsCreateFileStream(target.string().c_str(), kEdsFileCreateDisposition_CreateAlways,
                                      kEdsAccess_ReadWrite, &stream);
    if (rc != EDS_ERR_OK || !stream) { error = edsError("EdsCreateFileStream", rc); return false; }
    rc = EdsDownload(item, info.size, stream);
    if (rc == EDS_ERR_OK) rc = EdsDownloadComplete(item);
    EdsRelease(stream);
    if (rc != EDS_ERR_OK) { error = edsError("EdsDownload", rc); return false; }
    return true;
}

QImage previewFromStoredOriginal(const std::filesystem::path &path, std::string &source) {
    std::ifstream f(path,std::ios::binary);
    if(!f)return {};
    std::vector<char> bytes((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
    if(bytes.size()<4)return {};

    // JPEG originals decode directly.  For RAW/CR2 this normally fails and we
    // continue with extraction of the embedded JPEG preview below.
    const QByteArray whole(bytes.data(),qsizetype(bytes.size()));
    QImage direct=QImage::fromData(whole);
    if(!direct.isNull()){
        source="original-image";
        return direct.convertToFormat(QImage::Format_RGB888);
    }

    // CR2 is TIFF-based and normally contains one or more embedded JPEG previews.
    // Qt does not need a RAW plugin if we extract the largest decodable JPEG
    // segment.  This keeps the science-grade CR2 untouched while providing a
    // useful GUI/plate-solving preview even when EdsDownloadThumbnail() returns
    // no data (observed on EOS 550D).
    QImage best; std::int64_t bestPixels=0; std::size_t pos=0; int candidates=0;
    while(pos+3<bytes.size()&&candidates<64){
        std::size_t soi=pos;
        while(soi+1<bytes.size()&&!(static_cast<unsigned char>(bytes[soi])==0xFF&&static_cast<unsigned char>(bytes[soi+1])==0xD8))++soi;
        if(soi+1>=bytes.size())break;
        std::size_t eoi=soi+2;
        while(eoi+1<bytes.size()&&!(static_cast<unsigned char>(bytes[eoi])==0xFF&&static_cast<unsigned char>(bytes[eoi+1])==0xD9))++eoi;
        if(eoi+1>=bytes.size())break;
        const auto len=eoi+2-soi;
        if(len>1024){
            const QByteArray candidate(bytes.data()+soi,qsizetype(len));
            QImage image=QImage::fromData(candidate);
            if(!image.isNull()){
                const std::int64_t pixels=std::int64_t(image.width())*std::int64_t(image.height());
                if(pixels>bestPixels){bestPixels=pixels;best=image;}
            }
        }
        ++candidates;pos=soi+2;
    }
    if(best.isNull())return {};
    source="embedded-jpeg";
    return best.convertToFormat(QImage::Format_RGB888);
}

const char *manifest(void*){return copyString(R"json({"driverId":"oal.canon","name":"OpenAstroLink native Canon EOS driver (EDSDK)","version":"0.2.10.32","abiVersion":2,"threadModel":"per-device-serial","transport":"Canon EDSDK"})json");}
bool start(void*,const char*cfg){
    const EdsError initRc=EdsInitializeSDK();
    if(initRc!=EDS_ERR_OK){log(2,edsError("EdsInitializeSDK",initRc));return false;}
    state.sdkInitialized=true;
    log(1,"Canon EDSDK initialized");
    state.spoolDir=defaultSpoolDir();
    if(cfg){auto s=stringValue(cfg,"spoolDir");if(!s.empty())state.spoolDir=s;}
    const EdsError addRc=EdsSetCameraAddedHandler(&cameraAdded,nullptr);
    if(addRc!=EDS_ERR_OK)log(2,edsError("EdsSetCameraAddedHandler",addRc));
    state.eventPumpStop=false;
    state.eventPumpThread=std::thread([](){
        while(!state.eventPumpStop){
            const EdsError rc=EdsGetEvent();
            if(rc!=EDS_ERR_OK)log(2,edsError("EdsGetEvent",rc));
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
    std::string e;
    const bool enumOk=refreshEnumeration(e);
    std::size_t count=0;{std::lock_guard<std::mutex>lk(state.mutex);count=state.cameras.size();}
    if(!enumOk)log(2,"Canon EDSDK initial enumeration failed: "+e);
    else log(1,"Canon EDSDK discovered "+std::to_string(count)+" camera(s)");
    return true;
}
void stop(void*){
    state.eventPumpStop=true;if(state.eventPumpThread.joinable())state.eventPumpThread.join();
    {std::lock_guard<std::mutex>lk(state.mutex);for(auto &kv:state.cameras)closeCamera(*kv.second);state.cameras.clear();}
    if(state.sdkInitialized){EdsTerminateSDK();state.sdkInitialized=false;}
}
const char *devices(void*){
    std::string e;const bool enumOk=refreshEnumeration(e);std::lock_guard<std::mutex>lk(state.mutex);
    if(!enumOk)log(2,"Canon EDSDK enumeration failed: "+e);
    else log(1,"Canon EDSDK discovery scan sees "+std::to_string(state.cameras.size())+" camera(s)");
    std::string out="[";bool first=true;
    for(auto &kv:state.cameras){auto &c=*kv.second;if(!first)out+=',';first=false;out+="{\"id\":"+quote(c.id)+",\"name\":"+quote(c.name)+",\"type\":\"camera\",\"transport\":\"edsdk\",\"backend\":\"native\"}";}
    return copyString(out+"]");
}
const char *caps(void*,const char*id){
    std::string gain="{\"supported\":true,\"semantic\":\"ISO\",\"zeroMeans\":\"preserve-current\"}";
    {
        std::lock_guard<std::mutex>lk(state.mutex);
        auto it=state.cameras.find(id?id:"");
        if(it!=state.cameras.end()&&it->second->connected&&it->second->camera){
            const auto choices=advertisedIsoChoices(*it->second);
            if(!choices.empty()){
                std::ostringstream g;g<<"{\"supported\":true,\"semantic\":\"ISO\",\"zeroMeans\":\"preserve-current\",\"choices\":[";
                for(std::size_t i=0;i<choices.size();++i){if(i)g<<',';g<<choices[i].iso;}
                g<<"]}";gain=g.str();
            }
        }
    }
    return copyString(std::string("{\"camera\":{\"exposure\":{\"supported\":true,\"minSec\":0.000125,\"maxSec\":3600,\"abortSupported\":true,\"mode\":\"bulb-timed-or-camera-release\"},\"raw\":{\"supported\":true,\"preserveOriginal\":true},\"preview\":{\"source\":\"EDSDK-thumbnail-or-embedded-JPEG\",\"scienceGrade\":false},\"gain\":")+gain+",\"roi\":{\"supported\":false},\"binning\":{\"supported\":false}},\"transport\":{\"kind\":\"usb\",\"implementation\":\"Canon EDSDK\"}}");
}
const char *health(void*,const char*id){std::lock_guard<std::mutex>lk(state.mutex);auto it=state.cameras.find(id?id:"");bool c=it!=state.cameras.end()&&it->second->connected;return copyString(std::string("{\"status\":\"")+(c?"ok":"disconnected")+"\",\"transport\":\"edsdk\"}");}

const char *invoke(void*,const char*id,const char*method,const char*req,const OalDriverCallV2*call){
    CameraState *c=nullptr;{std::lock_guard<std::mutex>lk(state.mutex);auto it=state.cameras.find(id?id:"");if(it==state.cameras.end())return fail("device-not-found","Canon EOS device not found");c=it->second.get();}
    const std::string m=method?method:"",j=req?req:"{}";std::string error;
    if(m=="device.connect")return openCamera(*c,error)?ok():fail("connect-failed",error);
    if(m=="device.disconnect"){closeCamera(*c);return ok();}
    if(!c->connected)return fail("not-connected","Canon EOS is not connected");
    if(m=="camera.abortExposure"){c->abortRequested=true;if(c->camera){EdsSendCommand(c->camera,kEdsCameraCommand_BulbEnd,0);releaseShutter(*c);}c->eventCv.notify_all();return ok();}
    if(m=="camera.status")return ok(std::string("{\"connected\":true,\"transport\":\"edsdk\",\"name\":")+quote(c->name)+"}");
    if(m!="camera.capture")return fail("unsupported-method","Unsupported Canon method: "+m);

    std::lock_guard<std::mutex> oplock(c->opMutex);c->abortRequested=false;
    const double requestedSec=std::clamp(number(j,"exposureSec",1.0),0.000125,3600.0);const std::string save=stringValue(j,"savePath");
    const double requestedGain=number(j,"gain",0.0);
    double actualSec=requestedSec,actualGain=0.0;
    if(!setIsoForGain(*c,requestedGain,actualGain,error))return fail("gain-configuration-failed",error);
    emitEvent(c->id,"camera.exposure.started","{\"requestedExposureSec\":"+std::to_string(requestedSec)+",\"requestedGain\":"+std::to_string(requestedGain)+",\"actualIso\":"+std::to_string(actualGain)+"}");
    // <=30 s uses the nearest Tv value reported by the body and a non-AF
    // shutter press. This avoids EDS_ERR_TAKE_PICTURE_AF_NG (0x8D01), which
    // can wedge older EOS bodies after an unsuccessful remote autofocus.
    if(requestedSec<=30.0){
        if(!setTvForExposure(*c,requestedSec,actualSec,error))return fail("exposure-configuration-failed",error);
        if(!triggerNonAfStill(*c,error))return fail("capture-start-failed",error);
    }else{
        if(!setBulbTv(*c,error))return fail("exposure-configuration-failed",error);
        // EOS 550D accepts PressShutterButton/NonAF for ordinary exposures but
        // returns EDS_ERR_INVALID_PARAMETER (0x60) for BulbStart.  Holding the
        // same non-AF shutter command while Tv=Bulb is also how mature EDSDK
        // clients support older bodies, and avoids the model-specific BulbStart
        // command path.
        releaseShutter(*c);
        EdsError startRc=EdsSendCommand(c->camera,kEdsCameraCommand_PressShutterButton,
                                       kEdsCameraCommand_ShutterButton_Completely_NonAF);
        bool commandBulb=false; EdsError lockRc=EDS_ERR_OK;
        if(startRc!=EDS_ERR_OK){
            log(2,edsError("EdsSendCommand(PressShutterButton Bulb NonAF)",startRc)+"; trying BulbStart compatibility fallback");
            lockRc=EdsSendStatusCommand(c->camera,kEdsCameraStatusCommand_UILock,0);
            if(lockRc!=EDS_ERR_OK)log(2,edsError("EdsSendStatusCommand(UILock)",lockRc));
            startRc=EdsSendCommand(c->camera,kEdsCameraCommand_BulbStart,0);
            commandBulb=startRc==EDS_ERR_OK;
        }
        if(startRc!=EDS_ERR_OK){
            if(lockRc==EDS_ERR_OK)EdsSendStatusCommand(c->camera,kEdsCameraStatusCommand_UIUnLock,0);
            releaseShutter(*c);
            return fail("capture-start-failed",edsError(commandBulb?"EdsSendCommand(BulbStart)":"Canon Bulb start",startRc));
        }
        const auto started=std::chrono::steady_clock::now();
        while(std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<requestedSec){
            if(c->abortRequested||(call&&call->operationIdUtf8&&host.isCancellationRequested&&host.isCancellationRequested(host.hostContext,call->operationIdUtf8))){
                if(commandBulb)EdsSendCommand(c->camera,kEdsCameraCommand_BulbEnd,0);else releaseShutter(*c);
                if(commandBulb&&lockRc==EDS_ERR_OK)EdsSendStatusCommand(c->camera,kEdsCameraStatusCommand_UIUnLock,0);
                releaseShutter(*c);return fail("cancelled","Canon exposure cancelled");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        EdsError endRc=EDS_ERR_OK;
        if(commandBulb){endRc=EdsSendCommand(c->camera,kEdsCameraCommand_BulbEnd,0);if(lockRc==EDS_ERR_OK)EdsSendStatusCommand(c->camera,kEdsCameraStatusCommand_UIUnLock,0);}
        else {endRc=EdsSendCommand(c->camera,kEdsCameraCommand_PressShutterButton,kEdsCameraCommand_ShutterButton_OFF);}
        if(endRc!=EDS_ERR_OK)return fail("capture-end-failed",edsError(commandBulb?"EdsSendCommand(BulbEnd)":"EdsSendCommand(ShutterButton OFF)",endRc));
        actualSec=requestedSec;
        log(1,std::string("Canon Bulb transport=")+(commandBulb?"BulbStart/BulbEnd":"held-non-af-shutter"));
    }
    log(1,"Canon capture requested="+std::to_string(requestedSec)+" s actual="+std::to_string(actualSec)+" s ISO="+(actualGain>0?std::to_string(actualGain):std::string("camera-current"))+" focusPolicy=non-af");
    auto *item=waitForItem(*c,std::chrono::milliseconds(std::max(15000,int(actualSec*1000)+15000)),call);
    if(!item){
        if(c->transportLost||!c->connected)return fail("DEVICE_DISCONNECTED","Canon camera was switched off or USB transport was lost during exposure");
        if(c->abortRequested)return fail("cancelled","Canon exposure cancelled");
        return fail("capture-timeout","Canon did not publish a transfer object before timeout");
    }
    EdsDirectoryItemInfo info{};
    EdsError infoRc = EdsGetDirectoryItemInfo(item, &info);
    if (infoRc != EDS_ERR_OK) { EdsRelease(item); return fail("download-failed", edsError("EdsGetDirectoryItemInfo", infoRc)); }
    std::vector<std::uint8_t> thumb;
    downloadThumbnailFile(item, thumb); // optional: several EOS/RAW combinations expose none here
    const auto stored = targetPathFor(info, save);
    if (!downloadOriginalFile(item, info, stored, error)) { EdsRelease(item); return fail("download-failed", error); }
    EdsRelease(item);
    QImage image; std::string previewSource;
    if(!thumb.empty()){
        const QByteArray encoded(reinterpret_cast<const char *>(thumb.data()), qsizetype(thumb.size()));
        image=QImage::fromData(encoded);
        if(!image.isNull())previewSource="edsdk-thumbnail";
    }
    if(image.isNull())image=previewFromStoredOriginal(stored,previewSource);
    if(image.isNull())return fail("preview-decode-failed","Canon original was stored at "+stored.string()+", but neither EDSDK thumbnail nor an embedded/original JPEG preview could be decoded");
    image=image.convertToFormat(QImage::Format_RGB888);
    log(1,"Canon preview source="+previewSource+" size="+std::to_string(image.width())+"x"+std::to_string(image.height())+" original="+stored.string());
    const std::string frameId="canon-"+std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    const std::string meta="{\"originalPath\":"+quote(stored.string())+",\"originalName\":"+quote(info.szFileName)+",\"previewSource\":"+quote(previewSource)+",\"requestedExposureSec\":"+std::to_string(requestedSec)+",\"actualExposureSec\":"+std::to_string(actualSec)+",\"requestedGain\":"+std::to_string(requestedGain)+",\"actualIso\":"+std::to_string(actualGain)+",\"focusPolicy\":\"non-af\"}";
    OalFrameDescriptorV2 f{};f.structSize=sizeof(f);f.frameIdUtf8=frameId.c_str();f.width=std::uint32_t(image.width());f.height=std::uint32_t(image.height());f.strideBytes=std::uint32_t(image.bytesPerLine());f.pixelFormat=OAL_PIXEL_RGB8;f.bitsPerSample=8;f.channels=3;f.capturedUnixNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();f.exposureSec=actualSec;f.gain=actualGain;f.data=image.constBits();f.dataBytes=std::uint64_t(image.sizeInBytes());f.metadataJsonUtf8=meta.c_str();
    const auto token=host.publishFrame?host.publishFrame(host.hostContext,"oal.canon",c->id.c_str(),&f):0;
    emitEvent(c->id,"camera.frame.ready","{\"frameToken\":"+std::to_string(token)+"}");
    return token?ok("{\"frameToken\":"+std::to_string(token)+",\"savedPath\":"+quote(stored.string())+"}"):fail("publish-failed","Host rejected Canon preview frame");
}
bool cancel(void*,const char*id,const char*){std::lock_guard<std::mutex>lk(state.mutex);auto it=state.cameras.find(id?id:"");if(it==state.cameras.end())return false;it->second->abortRequested=true;if(it->second->camera){EdsSendCommand(it->second->camera,kEdsCameraCommand_BulbEnd,0);releaseShutter(*it->second);}it->second->eventCv.notify_all();return true;}
void releaseString(void*,const char*p){if(p)host.deallocate(host.hostContext,const_cast<char*>(p));}
OalDriverV2 api{OAL_DRIVER_ABI_V2,sizeof(OalDriverV2),OAL_DRIVER_FEATURE_EVENTS|OAL_DRIVER_FEATURE_FRAME_PUBLISH|OAL_DRIVER_FEATURE_CANCELLATION|OAL_DRIVER_FEATURE_HEALTH,"oal.canon","OpenAstroLink native Canon EOS driver (EDSDK)","0.2.10.32",nullptr,&manifest,&start,&stop,&devices,&caps,&health,&invoke,&cancel,&releaseString};
}
extern "C" OAL_DRIVER_EXPORT const OalDriverV2 *oalCreateDriverV2(const OalDriverHostV2*h){if(!h||h->abiVersion!=OAL_DRIVER_ABI_V2||!h->allocate||!h->deallocate)return nullptr;host=*h;return &api;}
