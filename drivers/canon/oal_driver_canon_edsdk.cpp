#include "oal/driver_api.h"
#include <EDSDK.h>
#include <QImage>
#include <QByteArray>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
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
    std::mutex opMutex;
    std::mutex eventMutex;
    std::condition_variable eventCv;
    std::vector<EdsDirectoryItemRef> pendingItems;
};

struct DriverState {
    std::mutex mutex;
    std::unordered_map<std::string, std::unique_ptr<CameraState>> cameras;
    std::filesystem::path spoolDir;
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
std::string edsError(const char *what,EdsError e){std::ostringstream s;s<<what<<" failed (EDSDK 0x"<<std::hex<<e<<")";return s.str();}
std::string stringValue(const std::string&j,const std::string&k,const std::string&f={}){auto p=j.find("\""+k+"\"");if(p==std::string::npos)return f;auto c=j.find(':',p);auto q=j.find('"',c+1);if(c==std::string::npos||q==std::string::npos)return f;auto e=j.find('"',q+1);return e==std::string::npos?f:j.substr(q+1,e-q-1);}
double number(const std::string&j,const std::string&k,double f){auto p=j.find("\""+k+"\"");if(p==std::string::npos)return f;auto c=j.find(':',p);if(c==std::string::npos)return f;char*e=nullptr;double v=std::strtod(j.c_str()+c+1,&e);return e==j.c_str()+c+1?f:v;}

std::filesystem::path defaultSpoolDir(){
#ifdef _WIN32
    if(const char *p=std::getenv("USERPROFILE")) return std::filesystem::path(p)/"Pictures"/"OpenAstroLink"/"Canon";
#endif
    if(const char *p=std::getenv("HOME")) return std::filesystem::path(p)/"Pictures"/"OpenAstroLink"/"Canon";
    return std::filesystem::temp_directory_path()/"OpenAstroLink"/"Canon";
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
    EdsCameraListRef list=nullptr; EdsError rc=EdsGetCameraList(&list); if(rc!=EDS_ERR_OK){error=edsError("EdsGetCameraList",rc);return false;}
    EdsUInt32 count=0; rc=EdsGetChildCount(list,&count); if(rc!=EDS_ERR_OK){EdsRelease(list);error=edsError("EdsGetChildCount",rc);return false;}
    std::lock_guard<std::mutex> lk(state.mutex);
    for(EdsUInt32 i=0;i<count;++i){
        EdsCameraRef cam=nullptr; if(EdsGetChildAtIndex(list,EdsInt32(i),&cam)!=EDS_ERR_OK||!cam)continue;
        EdsDeviceInfo info{}; if(EdsGetDeviceInfo(cam,&info)!=EDS_ERR_OK){EdsRelease(cam);continue;}
        std::string name=info.szDeviceDescription; if(name.empty())name="Canon EOS";
        std::string id="canon-edsdk:"+std::to_string(i)+":"+name;
        auto it=state.cameras.find(id);
        if(it==state.cameras.end()){
            auto c=std::make_unique<CameraState>(); c->id=id;c->name=name;c->index=int(i);state.cameras.emplace(id,std::move(c));
        }
        EdsRelease(cam);
    }
    EdsRelease(list); return true;
}

bool openCamera(CameraState &c,std::string &error){
    if(c.connected)return true;
    EdsCameraListRef list=nullptr; EdsError rc=EdsGetCameraList(&list); if(rc!=EDS_ERR_OK){error=edsError("EdsGetCameraList",rc);return false;}
    EdsBaseRef child=nullptr; rc=EdsGetChildAtIndex(list,c.index,&child); EdsRelease(list);
    if(rc!=EDS_ERR_OK||!child){error=edsError("EdsGetChildAtIndex",rc);return false;}
    c.camera=static_cast<EdsCameraRef>(child);
    rc=EdsOpenSession(c.camera); if(rc!=EDS_ERR_OK){EdsRelease(c.camera);c.camera=nullptr;error=edsError("EdsOpenSession",rc);return false;}
    EdsSetObjectEventHandler(c.camera,kEdsObjectEvent_All,&objectEvent,&c);
    EdsUInt32 saveTo=kEdsSaveTo_Host; EdsSetPropertyData(c.camera,kEdsPropID_SaveTo,0,sizeof(saveTo),&saveTo);
    EdsCapacity capacity{}; capacity.numberOfFreeClusters=0x7fffffff; capacity.bytesPerSector=0x1000; capacity.reset=1; EdsSetCapacity(c.camera,capacity);
    c.abortRequested=false;c.connected=true; emitEvent(c.id,"device.connected"); return true;
}
void closeCamera(CameraState &c){
    c.abortRequested=true;
    {
        std::lock_guard<std::mutex> lk(c.eventMutex);
        for(auto *item:c.pendingItems) if(item) EdsRelease(item);
        c.pendingItems.clear();
    }
    if(c.camera){EdsCloseSession(c.camera);EdsRelease(c.camera);c.camera=nullptr;}
    c.connected=false; emitEvent(c.id,"device.disconnected");
}

EdsDirectoryItemRef waitForItem(CameraState &c,std::chrono::milliseconds timeout,const OalDriverCallV2 *call){
    const auto deadline=std::chrono::steady_clock::now()+timeout;
    std::unique_lock<std::mutex> lk(c.eventMutex);
    while(c.pendingItems.empty()){
        if(c.abortRequested || (call&&call->operationIdUtf8&&host.isCancellationRequested&&host.isCancellationRequested(host.hostContext,call->operationIdUtf8))) return nullptr;
        if(c.eventCv.wait_until(lk,deadline)==std::cv_status::timeout)return nullptr;
    }
    auto *item=c.pendingItems.front();c.pendingItems.erase(c.pendingItems.begin());return item;
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

const char *manifest(void*){return copyString(R"json({"driverId":"oal.canon","name":"OpenAstroLink native Canon EOS driver (EDSDK)","version":"0.2.10.10","abiVersion":2,"threadModel":"per-device-serial","transport":"Canon EDSDK"})json");}
bool start(void*,const char*cfg){
    if(EdsInitializeSDK()!=EDS_ERR_OK) return false;
    state.sdkInitialized=true;
    state.spoolDir=defaultSpoolDir();
    if(cfg){auto s=stringValue(cfg,"spoolDir");if(!s.empty())state.spoolDir=s;}
    std::string e;refreshEnumeration(e);return true;
}
void stop(void*){
    {std::lock_guard<std::mutex>lk(state.mutex);for(auto &kv:state.cameras)closeCamera(*kv.second);state.cameras.clear();}
    if(state.sdkInitialized){EdsTerminateSDK();state.sdkInitialized=false;}
}
const char *devices(void*){
    std::string e;refreshEnumeration(e);std::lock_guard<std::mutex>lk(state.mutex);std::string out="[";bool first=true;
    for(auto &kv:state.cameras){auto &c=*kv.second;if(!first)out+=',';first=false;out+="{\"id\":"+quote(c.id)+",\"name\":"+quote(c.name)+",\"type\":\"camera\",\"transport\":\"edsdk\",\"backend\":\"native\"}";}
    return copyString(out+"]");
}
const char *caps(void*,const char*){
    return copyString(R"({"camera":{"exposure":{"supported":true,"minSec":0.05,"maxSec":3600,"abortSupported":true,"mode":"bulb-timed-or-camera-release"},"raw":{"supported":true,"preserveOriginal":true},"preview":{"source":"EDSDK thumbnail","scienceGrade":false},"gain":{"supported":false},"roi":{"supported":false},"binning":{"supported":false}},"transport":{"kind":"usb","implementation":"Canon EDSDK"}})");
}
const char *health(void*,const char*id){std::lock_guard<std::mutex>lk(state.mutex);auto it=state.cameras.find(id?id:"");bool c=it!=state.cameras.end()&&it->second->connected;return copyString(std::string("{\"status\":\"")+(c?"ok":"disconnected")+"\",\"transport\":\"edsdk\"}");}

const char *invoke(void*,const char*id,const char*method,const char*req,const OalDriverCallV2*call){
    CameraState *c=nullptr;{std::lock_guard<std::mutex>lk(state.mutex);auto it=state.cameras.find(id?id:"");if(it==state.cameras.end())return fail("device-not-found","Canon EOS device not found");c=it->second.get();}
    const std::string m=method?method:"",j=req?req:"{}";std::string error;
    if(m=="device.connect")return openCamera(*c,error)?ok():fail("connect-failed",error);
    if(m=="device.disconnect"){closeCamera(*c);return ok();}
    if(!c->connected)return fail("not-connected","Canon EOS is not connected");
    if(m=="camera.abortExposure"){c->abortRequested=true;if(c->camera)EdsSendCommand(c->camera,kEdsCameraCommand_BulbEnd,0);c->eventCv.notify_all();return ok();}
    if(m=="camera.status")return ok(std::string("{\"connected\":true,\"transport\":\"edsdk\",\"name\":")+quote(c->name)+"}");
    if(m!="camera.capture")return fail("unsupported-method","Unsupported Canon method: "+m);

    std::lock_guard<std::mutex> oplock(c->opMutex);c->abortRequested=false;
    const double sec=std::clamp(number(j,"exposureSec",1.0),0.05,3600.0);const std::string save=stringValue(j,"savePath");
    emitEvent(c->id,"camera.exposure.started","{\"exposureSec\":"+std::to_string(sec)+"}");
    EdsError rc=EdsSendCommand(c->camera,kEdsCameraCommand_BulbStart,0);
    if(rc!=EDS_ERR_OK){ // Some bodies/modes reject BulbStart; fall back to the camera's current Tv.
        rc=EdsSendCommand(c->camera,kEdsCameraCommand_TakePicture,0);
        if(rc!=EDS_ERR_OK)return fail("capture-start-failed",edsError("EdsSendCommand",rc));
    } else {
        const auto started=std::chrono::steady_clock::now();
        while(std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<sec){
            if(c->abortRequested||(call&&call->operationIdUtf8&&host.isCancellationRequested&&host.isCancellationRequested(host.hostContext,call->operationIdUtf8))){EdsSendCommand(c->camera,kEdsCameraCommand_BulbEnd,0);return fail("cancelled","Canon exposure cancelled");}
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        EdsSendCommand(c->camera,kEdsCameraCommand_BulbEnd,0);
    }
    auto *item=waitForItem(*c,std::chrono::milliseconds(std::max(15000,int(sec*1000)+15000)),call);
    if(!item)return fail("capture-timeout","Canon did not publish a transfer object before timeout");
    EdsDirectoryItemInfo info{};
    EdsError infoRc = EdsGetDirectoryItemInfo(item, &info);
    if (infoRc != EDS_ERR_OK) { EdsRelease(item); return fail("download-failed", edsError("EdsGetDirectoryItemInfo", infoRc)); }
    std::vector<std::uint8_t> thumb;
    downloadThumbnailFile(item, thumb); // best-effort operational preview before completing the original transfer
    const auto stored = targetPathFor(info, save);
    if (!downloadOriginalFile(item, info, stored, error)) { EdsRelease(item); return fail("download-failed", error); }
    EdsRelease(item);
    if (thumb.empty()) return fail("preview-unavailable", "Canon original was stored, but EDSDK returned no thumbnail preview");
    const QByteArray encoded(reinterpret_cast<const char *>(thumb.data()), qsizetype(thumb.size()));
    QImage image = QImage::fromData(encoded).convertToFormat(QImage::Format_RGB888);
    if (image.isNull()) return fail("preview-decode-failed","Canon original was stored, but EDSDK preview could not be decoded");
    const std::string frameId="canon-"+std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    const std::string meta="{\"originalPath\":"+quote(stored.string())+",\"originalName\":"+quote(info.szFileName)+",\"previewSource\":\"edsdk-thumbnail\"}";
    OalFrameDescriptorV2 f{};f.structSize=sizeof(f);f.frameIdUtf8=frameId.c_str();f.width=std::uint32_t(image.width());f.height=std::uint32_t(image.height());f.strideBytes=std::uint32_t(image.bytesPerLine());f.pixelFormat=OAL_PIXEL_RGB8;f.bitsPerSample=8;f.channels=3;f.capturedUnixNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();f.exposureSec=sec;f.data=image.constBits();f.dataBytes=std::uint64_t(image.sizeInBytes());f.metadataJsonUtf8=meta.c_str();
    const auto token=host.publishFrame?host.publishFrame(host.hostContext,"oal.canon",c->id.c_str(),&f):0;
    emitEvent(c->id,"camera.frame.ready","{\"frameToken\":"+std::to_string(token)+"}");
    return token?ok("{\"frameToken\":"+std::to_string(token)+",\"savedPath\":"+quote(stored.string())+"}"):fail("publish-failed","Host rejected Canon preview frame");
}
bool cancel(void*,const char*id,const char*){std::lock_guard<std::mutex>lk(state.mutex);auto it=state.cameras.find(id?id:"");if(it==state.cameras.end())return false;it->second->abortRequested=true;if(it->second->camera)EdsSendCommand(it->second->camera,kEdsCameraCommand_BulbEnd,0);it->second->eventCv.notify_all();return true;}
void releaseString(void*,const char*p){if(p)host.deallocate(host.hostContext,const_cast<char*>(p));}
OalDriverV2 api{OAL_DRIVER_ABI_V2,sizeof(OalDriverV2),OAL_DRIVER_FEATURE_EVENTS|OAL_DRIVER_FEATURE_FRAME_PUBLISH|OAL_DRIVER_FEATURE_CANCELLATION|OAL_DRIVER_FEATURE_HEALTH,"oal.canon","OpenAstroLink native Canon EOS driver (EDSDK)","0.2.10.10",nullptr,&manifest,&start,&stop,&devices,&caps,&health,&invoke,&cancel,&releaseString};
}
extern "C" OAL_DRIVER_EXPORT const OalDriverV2 *oalCreateDriverV2(const OalDriverHostV2*h){if(!h||h->abiVersion!=OAL_DRIVER_ABI_V2||!h->allocate||!h->deallocate)return nullptr;host=*h;return &api;}
