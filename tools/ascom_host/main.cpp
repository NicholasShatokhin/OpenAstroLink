#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <oleauto.h>
#endif

#include <cstdio>
#include <string>
#include <vector>
#include <utility>

namespace {
QJsonObject ok(const QJsonObject &data = {}) { return {{"ok", true}, {"data", data}}; }
QJsonObject errorJson(const QString &code, const QString &message) {
    return {{"ok", false}, {"error", QJsonObject{{"code", code}, {"message", message}}}};
}
void writeJson(const QJsonObject &o) {
    const QByteArray line = QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(line.constData(), 1, size_t(line.size()), stdout);
    std::fflush(stdout);
}

#ifdef Q_OS_WIN
QString hresultText(HRESULT hr) {
    wchar_t *buffer = nullptr;
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                       FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr, DWORD(hr), 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    QString text = n && buffer ? QString::fromWCharArray(buffer, int(n)).trimmed()
                               : QStringLiteral("HRESULT 0x%1").arg(quint32(hr), 8, 16, QLatin1Char('0'));
    if (buffer) LocalFree(buffer);
    return text;
}

struct AutoVariant {
    VARIANT value;
    AutoVariant() { VariantInit(&value); }
    ~AutoVariant() { VariantClear(&value); }
    AutoVariant(const AutoVariant &) = delete;
    AutoVariant &operator=(const AutoVariant &) = delete;
};

VARIANT vBool(bool x) { VARIANT v; VariantInit(&v); v.vt=VT_BOOL; v.boolVal=x?VARIANT_TRUE:VARIANT_FALSE; return v; }
VARIANT vInt(int x) { VARIANT v; VariantInit(&v); v.vt=VT_I4; v.lVal=x; return v; }
VARIANT vDouble(double x) { VARIANT v; VariantInit(&v); v.vt=VT_R8; v.dblVal=x; return v; }
VARIANT vString(const QString &x) { VARIANT v; VariantInit(&v); v.vt=VT_BSTR; v.bstrVal=SysAllocString(reinterpret_cast<const OLECHAR*>(x.utf16())); return v; }
void clearArgs(std::vector<VARIANT> &args) { for (auto &v : args) VariantClear(&v); }

class Dispatch {
public:
    ~Dispatch() { reset(); }
    Dispatch(const Dispatch &) = delete;
    Dispatch &operator=(const Dispatch &) = delete;
    Dispatch() = default;

    bool create(const QString &progId, QString *error) {
        reset();
        CLSID clsid{};
        const HRESULT cr = CLSIDFromProgID(reinterpret_cast<LPCOLESTR>(progId.utf16()), &clsid);
        if (FAILED(cr)) { if (error) *error = "COM ProgID is not registered: " + progId + " (" + hresultText(cr) + ")"; return false; }
        const HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                                            IID_IDispatch, reinterpret_cast<void **>(&ptr_));
        if (FAILED(hr) || !ptr_) { if (error) *error = "COM activation failed for " + progId + ": " + hresultText(hr); return false; }
        return true;
    }
    void reset() { if (ptr_) { ptr_->Release(); ptr_=nullptr; } }
    bool valid() const { return ptr_ != nullptr; }

    bool get(const wchar_t *name, VARIANT *result, QString *error) const {
        return invoke(name, DISPATCH_PROPERTYGET, {}, result, error);
    }
    bool put(const wchar_t *name, VARIANT value, QString *error) const {
        std::vector<VARIANT> args{value};
        const bool ok = invoke(name, DISPATCH_PROPERTYPUT, args, nullptr, error, true);
        // ownership of BSTR/other allocated content moved into args copy
        clearArgs(args);
        VariantInit(&value);
        return ok;
    }
    bool call(const wchar_t *name, std::vector<VARIANT> args, VARIANT *result, QString *error) const {
        const bool ok = invoke(name, DISPATCH_METHOD, args, result, error);
        clearArgs(args);
        return ok;
    }

private:
    bool invoke(const wchar_t *name, WORD flags, const std::vector<VARIANT> &naturalArgs,
                VARIANT *result, QString *error, bool propertyPut=false) const {
        if (!ptr_) { if(error)*error="COM object is not active"; return false; }
        LPOLESTR mutableName = const_cast<LPOLESTR>(name);
        DISPID id{};
        HRESULT hr = ptr_->GetIDsOfNames(IID_NULL, &mutableName, 1, LOCALE_USER_DEFAULT, &id);
        if (FAILED(hr)) { if(error)*error=QString("ASCOM member %1 not found: %2").arg(QString::fromWCharArray(name),hresultText(hr)); return false; }
        std::vector<VARIANTARG> reversed(naturalArgs.size());
        for (size_t i=0;i<naturalArgs.size();++i) reversed[i]=naturalArgs[naturalArgs.size()-1-i];
        DISPPARAMS dp{}; dp.cArgs=UINT(reversed.size()); dp.rgvarg=reversed.empty()?nullptr:reversed.data();
        DISPID putId=DISPID_PROPERTYPUT; if(propertyPut){dp.cNamedArgs=1;dp.rgdispidNamedArgs=&putId;}
        EXCEPINFO ex{}; UINT argErr=0; AutoVariant temporary;
        VARIANT *out = result ? result : &temporary.value;
        VariantInit(out);
        hr = ptr_->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, out, &ex, &argErr);
        if (FAILED(hr)) {
            QString detail;
            if (ex.bstrDescription) detail=QString::fromWCharArray(ex.bstrDescription);
            if(ex.bstrSource)SysFreeString(ex.bstrSource); if(ex.bstrDescription)SysFreeString(ex.bstrDescription); if(ex.bstrHelpFile)SysFreeString(ex.bstrHelpFile);
            if(error)*error=QString("ASCOM %1 failed: %2%3").arg(QString::fromWCharArray(name),hresultText(hr),detail.isEmpty()?QString():" — "+detail);
            return false;
        }
        return true;
    }
    IDispatch *ptr_{};
};

QString asString(const VARIANT &v, const QString &fallback={}) {
    VARIANT tmp; VariantInit(&tmp);
    if (SUCCEEDED(VariantChangeType(&tmp, const_cast<VARIANT*>(&v), 0, VT_BSTR)) && tmp.bstrVal) {
        const QString s=QString::fromWCharArray(tmp.bstrVal); VariantClear(&tmp); return s;
    }
    VariantClear(&tmp); return fallback;
}
double asDouble(const VARIANT &v, double fallback=0) { VARIANT t;VariantInit(&t);if(SUCCEEDED(VariantChangeType(&t,const_cast<VARIANT*>(&v),0,VT_R8))){double x=t.dblVal;VariantClear(&t);return x;}VariantClear(&t);return fallback; }
int asInt(const VARIANT &v, int fallback=-1) { VARIANT t;VariantInit(&t);if(SUCCEEDED(VariantChangeType(&t,const_cast<VARIANT*>(&v),0,VT_I4))){int x=int(t.lVal);VariantClear(&t);return x;}VariantClear(&t);return fallback; }
bool asBool(const VARIANT &v, bool fallback=false) { VARIANT t;VariantInit(&t);if(SUCCEEDED(VariantChangeType(&t,const_cast<VARIANT*>(&v),0,VT_BOOL))){bool x=t.boolVal==VARIANT_TRUE;VariantClear(&t);return x;}VariantClear(&t);return fallback; }

template<class F, class T> T safeGet(F &&fn, T fallback) { try { return fn(); } catch (...) { return fallback; } }

class AscomSession {
public:
    QJsonObject choose(const QString &current) {
        Dispatch chooser; QString e; if(!chooser.create("ASCOM.Utilities.Chooser",&e))return errorJson("ASCOM_NOT_INSTALLED",e);
        if(!chooser.put(L"DeviceType",vString("Telescope"),&e))return errorJson("CHOOSER_FAILED",e);
        AutoVariant out; std::vector<VARIANT> args; if(!current.trimmed().isEmpty())args.push_back(vString(current));
        if(!chooser.call(L"Choose",std::move(args),&out.value,&e))return errorJson("CHOOSER_FAILED",e);
        return ok({{"progId",asString(out.value)}});
    }
    QJsonObject connect(const QString &id) {
        if(id.trimmed().isEmpty())return errorJson("INVALID_PROGID","ASCOM Telescope ProgID is empty");
        disconnect(); QString e; if(!telescope_.create(id,&e))return errorJson("DRIVER_NOT_FOUND",e);
        if(!telescope_.put(L"Connected",vBool(true),&e)){telescope_.reset();return errorJson("CONNECT_FAILED",e);} progId_=id;
        return ok({{"progId",id},{"name",getString(L"Name",id)},{"description",getString(L"Description",id)}});
    }
    QJsonObject disconnect() { if(telescope_.valid()){QString e;telescope_.put(L"Connected",vBool(false),&e);telescope_.reset();}progId_.clear();return ok(); }
    QJsonObject setup(const QString &id) { Dispatch d;QString e;if(!d.create(id,&e))return errorJson("DRIVER_NOT_FOUND",e);if(!d.call(L"SetupDialog",{},nullptr,&e))return errorJson("SETUP_FAILED",e);return ok(); }
    QJsonObject status() {
        if(!telescope_.valid())return errorJson("NOT_CONNECTED","ASCOM telescope is not connected");
        return ok({{"raHours",getDouble(L"RightAscension",0.0)},{"decDeg",getDouble(L"Declination",0.0)},
                   {"tracking",getBool(L"Tracking",false)},{"slewing",getBool(L"Slewing",false)},
                   {"parked",getBool(L"AtPark",false)},{"sideOfPier",getInt(L"SideOfPier",-1)},{"progId",progId_}});
    }
    QJsonObject slew(double ra,double dec){if(!telescope_.valid())return notConnected();QString e;const bool async=getBool(L"CanSlewAsync",false);const wchar_t*name=async?L"SlewToCoordinatesAsync":L"SlewToCoordinates";if(!telescope_.call(name,{vDouble(ra),vDouble(dec)},nullptr,&e))return errorJson("SLEW_FAILED",e);return ok({{"async",async}});}
    QJsonObject sync(double ra,double dec){return method2(L"SyncToCoordinates",vDouble(ra),vDouble(dec),"SYNC_FAILED");}
    QJsonObject tracking(bool enabled){if(!telescope_.valid())return notConnected();QString e;if(!telescope_.put(L"Tracking",vBool(enabled),&e))return errorJson("TRACKING_FAILED",e);return ok();}
    QJsonObject abort(){return method0(L"AbortSlew","ABORT_FAILED");}
    QJsonObject park(bool parked){return method0(parked?L"Park":L"Unpark",parked?"PARK_FAILED":"UNPARK_FAILED");}
    QJsonObject pulseGuide(int direction,int ms){return method2(L"PulseGuide",vInt(direction),vInt(ms),"PULSE_GUIDE_FAILED");}
private:
    QJsonObject notConnected(){return errorJson("NOT_CONNECTED","ASCOM telescope is not connected");}
    QJsonObject method0(const wchar_t*n,const QString&code){if(!telescope_.valid())return notConnected();QString e;if(!telescope_.call(n,{},nullptr,&e))return errorJson(code,e);return ok();}
    QJsonObject method2(const wchar_t*n,VARIANT a,VARIANT b,const QString&code){if(!telescope_.valid()){VariantClear(&a);VariantClear(&b);return notConnected();}QString e;if(!telescope_.call(n,{a,b},nullptr,&e))return errorJson(code,e);return ok();}
    QString getString(const wchar_t*n,const QString&f={}){AutoVariant v;QString e;return telescope_.get(n,&v.value,&e)?asString(v.value,f):f;}
    double getDouble(const wchar_t*n,double f){AutoVariant v;QString e;return telescope_.get(n,&v.value,&e)?asDouble(v.value,f):f;}
    bool getBool(const wchar_t*n,bool f){AutoVariant v;QString e;return telescope_.get(n,&v.value,&e)?asBool(v.value,f):f;}
    int getInt(const wchar_t*n,int f){AutoVariant v;QString e;return telescope_.get(n,&v.value,&e)?asInt(v.value,f):f;}
    Dispatch telescope_; QString progId_;
};
#endif
}

int main(int argc,char **argv){
    QCoreApplication app(argc,argv); QCoreApplication::setApplicationName("oas-ascom-host");
#ifndef Q_OS_WIN
    writeJson(errorJson("PLATFORM_UNSUPPORTED","Classic ASCOM is Windows-only")); return 2;
#else
    const HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); if(FAILED(init)){writeJson(errorJson("COM_INIT_FAILED",hresultText(init)));return 2;}
    AscomSession s;
    const QStringList args=app.arguments();
    if(args.size()>1&&args[1]=="--choose"){writeJson(s.choose(args.size()>2?args[2]:QString()));CoUninitialize();return 0;}
    if(args.size()>1&&args[1]=="--setup"){writeJson(args.size()>2?s.setup(args[2]):errorJson("INVALID_PROGID","ProgID is required"));CoUninitialize();return 0;}
    std::string line;
    while(true){
        int ch; line.clear(); while((ch=std::getc(stdin))!=EOF&&ch!='\n')line.push_back(char(ch)); if(line.empty()&&ch==EOF)break;
        QJsonParseError pe{};const auto doc=QJsonDocument::fromJson(QByteArray::fromStdString(line),&pe);QJsonObject r;
        if(pe.error!=QJsonParseError::NoError||!doc.isObject())r=errorJson("INVALID_JSON",pe.errorString());
        else {const auto q=doc.object();const QString cmd=q.value("cmd").toString();
            if(cmd=="hello")r=ok({{"platform","windows-com"},{"ascomChooser",true}});
            else if(cmd=="choose")r=s.choose(q.value("current").toString());
            else if(cmd=="setup")r=s.setup(q.value("progId").toString());
            else if(cmd=="connect")r=s.connect(q.value("progId").toString());
            else if(cmd=="disconnect")r=s.disconnect();
            else if(cmd=="status")r=s.status();
            else if(cmd=="slew")r=s.slew(q.value("raHours").toDouble(),q.value("decDeg").toDouble());
            else if(cmd=="abort")r=s.abort();
            else if(cmd=="sync")r=s.sync(q.value("raHours").toDouble(),q.value("decDeg").toDouble());
            else if(cmd=="tracking")r=s.tracking(q.value("enabled").toBool());
            else if(cmd=="park")r=s.park(q.value("parked").toBool());
            else if(cmd=="pulseGuide")r=s.pulseGuide(q.value("direction").toInt(),q.value("durationMs").toInt());
            else if(cmd=="quit"){r=ok();r["quit"]=true;}
            else r=errorJson("UNKNOWN_COMMAND","Unknown command: "+cmd);
        }
        writeJson(r); if(r.value("quit").toBool())break;
    }
    s.disconnect(); CoUninitialize(); return 0;
#endif
}
