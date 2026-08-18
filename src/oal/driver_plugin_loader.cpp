#include "oal/driver_plugin_loader.h"
#include "oal/driver_api.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>

namespace oas {
struct OalDriverPluginLoader::Loaded {std::unique_ptr<QLibrary> library;const OalDriverV1 *api{};QString id;};
static void hostLog(int,const char*){}static void *hostAlloc(std::size_t n){return ::operator new(n);}static void hostFree(void*p){::operator delete(p);}static const OalDriverHostV1 host{1,&hostLog,&hostAlloc,&hostFree};
OalDriverPluginLoader::OalDriverPluginLoader(QObject*p):QObject(p){}OalDriverPluginLoader::~OalDriverPluginLoader(){for(auto&x:loaded_)if(x->api&&x->api->stop)x->api->stop(x->api->context);}
int OalDriverPluginLoader::scan(const QString&dir,QStringList*errors){loaded_.clear();QDir d(dir);QStringList filters;
#ifdef Q_OS_WIN
filters<<"*.dll";
#elif defined(Q_OS_MACOS)
filters<<"*.dylib"<<"*.so";
#else
filters<<"*.so";
#endif
for(const auto&file:d.entryList(filters,QDir::Files)){auto lib=std::make_unique<QLibrary>(d.filePath(file));if(!lib->load()){if(errors)errors->append(file+": "+lib->errorString());continue;}auto factory=reinterpret_cast<const OalDriverV1*(*)(const OalDriverHostV1*)>(lib->resolve("oalCreateDriverV1"));if(!factory){if(errors)errors->append(file+": missing oalCreateDriverV1");continue;}const auto*api=factory(&host);if(!api||api->abiVersion!=1||!api->driverId){if(errors)errors->append(file+": incompatible OAL ABI");continue;}if(api->start&&!api->start(api->context,"{}")){if(errors)errors->append(file+": driver start failed");continue;}auto x=std::make_unique<Loaded>();x->id=QString::fromUtf8(api->driverId);x->api=api;x->library=std::move(lib);loaded_.push_back(std::move(x));}return int(loaded_.size());}
QJsonArray OalDriverPluginLoader::devices()const{QJsonArray out;for(const auto&x:loaded_)if(x->api->enumerateDevicesJson){const char*p=x->api->enumerateDevicesJson(x->api->context);auto doc=QJsonDocument::fromJson(QByteArray(p?p:"[]"));if(doc.isArray())for(auto v:doc.array()){auto o=v.toObject();o["driverId"]=x->id;out.append(o);}if(p&&x->api->releaseString)x->api->releaseString(x->api->context,p);}return out;}
QJsonObject OalDriverPluginLoader::invoke(const QString&driver,const QString&device,const QString&method,const QJsonObject&request,QString*error){for(const auto&x:loaded_)if(x->id==driver&&x->api->invokeJson){QByteArray req=QJsonDocument(request).toJson(QJsonDocument::Compact);const char*p=x->api->invokeJson(x->api->context,device.toUtf8().constData(),method.toUtf8().constData(),req.constData());auto doc=QJsonDocument::fromJson(QByteArray(p?p:"{}"));if(p&&x->api->releaseString)x->api->releaseString(x->api->context,p);if(doc.isObject())return doc.object();if(error)*error="Driver returned invalid JSON";return{};}if(error)*error="Driver or method not found";return{};}
}
