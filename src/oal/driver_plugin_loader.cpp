#include "oal/driver_plugin_loader.h"
#include "oal/driver_api.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLibrary>
#include <QMetaObject>
#include <QMutexLocker>
#include <QProcessEnvironment>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <chrono>

namespace oas {

struct OalDriverPluginLoader::Loaded {
    std::unique_ptr<QLibrary> library;
    const OalDriverV1 *api1{};
    const OalDriverV2 *api2{};
    QString id;
    QString name;
    QString version;
    int abiVersion{0};
    QString manifestPath;
    QJsonObject manifest;
    QString threadModel{"serial"};
    mutable QMutex serialMutex;
    mutable QMutex deviceMutexMapGuard;
    mutable QHash<QString, std::shared_ptr<QMutex>> deviceMutexes;
};

namespace {
void v1HostLog(int, const char *) {}
void *v1HostAlloc(std::size_t n) { return ::operator new(n); }
void v1HostFree(void *p) { ::operator delete(p); }
const OalDriverHostV1 v1Host{1, &v1HostLog, &v1HostAlloc, &v1HostFree};

void v2HostLog(void *ctx, int level, const char *driver, const char *message) {
    static_cast<OalDriverPluginLoader *>(ctx)->logFromDriver(level, driver, message);
}
void *v2HostAlloc(void *, std::size_t n) { return ::operator new(n); }
void v2HostFree(void *, void *p) { ::operator delete(p); }
void v2HostEvent(void *ctx, const char *driver, const char *device, const char *json) {
    static_cast<OalDriverPluginLoader *>(ctx)->emitEventFromDriver(driver, device, json);
}
std::uint64_t v2HostFrame(void *ctx, const char *driver, const char *device,
                          const OalFrameDescriptorV2 *frame) {
    return static_cast<OalDriverPluginLoader *>(ctx)->publishFrameFromDriver(driver, device, frame);
}
std::uint64_t v2HostMonotonic(void *) {
    using namespace std::chrono;
    return std::uint64_t(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}
bool v2HostCancelled(void *, const char *) {
    // Host-level operation cancellation is currently delivered through
    // OalDriverV2::cancel() / device abort calls. This callback is reserved for
    // the next operation-context integration and safely defaults to false.
    return false;
}

QString canonicalLibraryPath(const QString &path) {
    QFileInfo fi(path);
    const QString canonical = fi.canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(fi.absoluteFilePath()) : canonical;
}

QJsonObject parseObject(const char *value) {
    if (!value) return {};
    const auto doc = QJsonDocument::fromJson(QByteArray(value));
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QJsonArray parseArray(const char *value) {
    if (!value) return {};
    const auto doc = QJsonDocument::fromJson(QByteArray(value));
    return doc.isArray() ? doc.array() : QJsonArray{};
}
} // namespace

OalDriverPluginLoader::OalDriverPluginLoader(QObject *p) : QObject(p) {}
OalDriverPluginLoader::~OalDriverPluginLoader() { clear(); }

void OalDriverPluginLoader::clear() {
    for (auto &x : loaded_) {
        if (x->api2 && x->api2->stop) x->api2->stop(x->api2->context);
        else if (x->api1 && x->api1->stop) x->api1->stop(x->api1->context);
    }
    loaded_.clear();
    { QMutexLocker deviceLock(&deviceCacheMutex_); deviceCache_ = QJsonArray{}; }
    QMutexLocker lock(&frameMutex_);
    frames_.clear();
}

int OalDriverPluginLoader::scan(const QString &directory, QStringList *errors, bool append) {
    const int count = scanDirectory(directory, errors, append);
    refreshDevices(errors);
    return count;
}

int OalDriverPluginLoader::scanDefaultPaths(QStringList *errors, bool discoverDevices) {
    clear();
    QStringList dirs;
    const QString env = qEnvironmentVariable("OAL_DRIVER_PATH");
    if (!env.isEmpty()) dirs << env.split(QDir::listSeparator(), Qt::SkipEmptyParts);

    const QString app = QCoreApplication::applicationDirPath();
    dirs << QDir(app).filePath("drivers")
         << QDir(app).filePath("plugins")
         << QDir(app).filePath("../lib/openastrolink/drivers")
         << QDir(app).filePath("../share/OpenAstroSuite/drivers");

#ifdef Q_OS_UNIX
    dirs << "/usr/local/lib/openastrolink/drivers"
         << "/usr/lib/openastrolink/drivers";
#endif

    QSet<QString> seen;
    bool first = true;
    for (const auto &d : dirs) {
        const QString clean = QDir::cleanPath(d);
        if (clean.isEmpty() || seen.contains(clean) || !QDir(clean).exists()) continue;
        seen.insert(clean);
        scanDirectory(clean, errors, !first);
        first = false;
    }
    // Hardware enumeration can take seconds on serial devices (Gemini reset
    // recovery, EQDrive baud probing, vendor camera SDK enumeration).  The
    // headless node therefore loads driver code first and performs discovery
    // asynchronously after HTTP/WebSocket listeners are already available.
    // CLI tools keep the historical synchronous behavior by leaving the
    // discoverDevices argument at its default true value.
    if (discoverDevices) refreshDevices(errors);
    return int(loaded_.size());
}

int OalDriverPluginLoader::scanDirectory(const QString &directory, QStringList *errors, bool append) {
    if (!append) clear();
    QDir d(directory);
    if (!d.exists()) return int(loaded_.size());

    QStringList claimed;
    for (const auto &manifestFile : d.entryList({"*.manifest.json"}, QDir::Files, QDir::Name))
        loadManifest(d.filePath(manifestFile), errors, &claimed);

    // ABI-v1 compatibility: load unclaimed libraries directly. ABI-v2 drivers
    // should always ship a manifest so permissions/isolation metadata are known
    // before code is loaded.
    QStringList filters;
#ifdef Q_OS_WIN
    filters << "*.dll";
#elif defined(Q_OS_MACOS)
    filters << "*.dylib" << "*.so";
#else
    filters << "*.so";
#endif
    for (const auto &file : d.entryList(filters, QDir::Files, QDir::Name)) {
        // Only OpenAstroLink driver libraries are eligible for the legacy
        // manifest-less ABI-v1 compatibility path. Vendor runtime DLLs (QHY,
        // ZWO, Canon, USB bridges, TBB, etc.) may intentionally live beside
        // drivers so the OS loader can resolve dependencies, but they are not
        // plugins and must never be dlopen/QLibrary-probed as OAL drivers.
        const QString base = QFileInfo(file).completeBaseName();
        if (!base.startsWith(QStringLiteral("oal_driver_"), Qt::CaseInsensitive))
            continue;
        const QString full = canonicalLibraryPath(d.filePath(file));
        if (claimed.contains(full)) continue;
        loadLibrary(d.filePath(file), {}, {}, errors);
    }
    return int(loaded_.size());
}

bool OalDriverPluginLoader::loadManifest(const QString &manifestPath, QStringList *errors,
                                         QStringList *claimedLibraries) {
    QFile f(manifestPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errors) errors->append(manifestPath + ": cannot read manifest");
        return false;
    }
    QJsonParseError pe{};
    const auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (!doc.isObject()) {
        if (errors) errors->append(manifestPath + ": invalid JSON: " + pe.errorString());
        return false;
    }
    const QJsonObject m = doc.object();
    if (m.value("schema").toString() != "org.openastrolink.driver-manifest/v2" ||
        m.value("abiVersion").toInt() != 2 || m.value("driverId").toString().isEmpty() ||
        !m.value("deviceClasses").isArray() || !m.value("permissions").isArray()) {
        if (errors) errors->append(manifestPath + ": manifest does not satisfy required ABI-v2 identity/permissions fields");
        return false;
    }
    const QString isolation = m.value("isolation").toString();
    if (isolation != "in-process" && isolation != "in-process-reference" && isolation != "out-of-process") {
        if (errors) errors->append(manifestPath + ": invalid isolation mode");
        return false;
    }
    if (isolation == "out-of-process") {
        if (errors) errors->append(manifestPath + ": requests out-of-process isolation; driver host is not implemented in v0.2.6");
        return false;
    }
    const QString library = m.value("library").toString();
    if (library.isEmpty()) {
        if (errors) errors->append(manifestPath + ": missing library");
        return false;
    }
    const QString path = QDir(QFileInfo(manifestPath).absolutePath()).filePath(library);
    QLibrary probe(path);
    QString resolved = path;
    // canonicalFilePath() cannot resolve extension-less QLibrary names. Try the
    // platform extension explicitly so duplicate detection and loading use the
    // same canonical library path.
#ifdef Q_OS_WIN
    if (!QFileInfo(resolved).exists() && QFileInfo(resolved + ".dll").exists()) resolved += ".dll";
#elif defined(Q_OS_MACOS)
    if (!QFileInfo(resolved).exists() && QFileInfo(resolved + ".dylib").exists()) resolved += ".dylib";
#else
    if (!QFileInfo(resolved).exists() && QFileInfo(resolved + ".so").exists()) resolved += ".so";
#endif
    if (claimedLibraries) claimedLibraries->append(canonicalLibraryPath(resolved));
    return loadLibrary(resolved, m, manifestPath, errors);
}

bool OalDriverPluginLoader::loadLibrary(const QString &libraryPath, const QJsonObject &manifest,
                                        const QString &manifestPath, QStringList *errors) {
    auto lib = std::make_unique<QLibrary>(libraryPath);
    if (!lib->load()) {
        if (errors) errors->append(libraryPath + ": " + lib->errorString());
        return false;
    }

    // Prefer v2. A v2 driver without a manifest is refused because v2 security
    // and permissions metadata are part of the native driver contract.
    auto factory2 = reinterpret_cast<const OalDriverV2 *(*)(const OalDriverHostV2 *)>(
        lib->resolve("oalCreateDriverV2"));
    if (factory2) {
        if (manifest.isEmpty()) {
            if (errors) errors->append(libraryPath + ": ABI v2 driver has no *.manifest.json; refused");
            return false;
        }
        OalDriverHostV2 host{};
        host.abiVersion = OAL_DRIVER_ABI_V2;
        host.structSize = sizeof(host);
        host.hostContext = this;
        host.log = &v2HostLog;
        host.allocate = &v2HostAlloc;
        host.deallocate = &v2HostFree;
        host.emitEvent = &v2HostEvent;
        host.publishFrame = &v2HostFrame;
        host.monotonicTimeNs = &v2HostMonotonic;
        host.isCancellationRequested = &v2HostCancelled;

        const auto *api = factory2(&host);
        if (!api || api->abiVersion != OAL_DRIVER_ABI_V2 ||
            api->structSize < sizeof(OalDriverV2) || !api->driverId) {
            if (errors) errors->append(libraryPath + ": incompatible OAL ABI v2");
            return false;
        }
        const QString expected = manifest.value("driverId").toString();
        const QString exportedId = QString::fromUtf8(api->driverId);
        if (!expected.isEmpty() && expected != exportedId) {
            if (errors) errors->append(libraryPath + ": manifest driverId does not match exported driverId");
            return false;
        }
        if (find(exportedId)) {
            if (errors) errors->append(libraryPath + ": duplicate native driverId already loaded: " + exportedId);
            return false;
        }
        const QByteArray cfg = QJsonDocument(manifest.value("config").toObject()).toJson(QJsonDocument::Compact);
        if (api->start && !api->start(api->context, cfg.constData())) {
            if (errors) errors->append(libraryPath + ": driver start failed");
            return false;
        }
        auto x = std::make_unique<Loaded>();
        x->id = QString::fromUtf8(api->driverId);
        x->name = QString::fromUtf8(api->driverName ? api->driverName : api->driverId);
        x->version = QString::fromUtf8(api->driverVersion ? api->driverVersion : "");
        x->abiVersion = 2;
        x->api2 = api;
        x->library = std::move(lib);
        x->manifestPath = manifestPath;
        x->manifest = manifest;
        x->threadModel = manifest.value("threadModel").toString("serial");
        loaded_.push_back(std::move(x));
        return true;
    }

    auto factory1 = reinterpret_cast<const OalDriverV1 *(*)(const OalDriverHostV1 *)>(
        lib->resolve("oalCreateDriverV1"));
    if (!factory1) {
        if (errors) errors->append(libraryPath + ": missing oalCreateDriverV2/oalCreateDriverV1");
        return false;
    }
    const auto *api = factory1(&v1Host);
    if (!api || api->abiVersion != 1 || !api->driverId) {
        if (errors) errors->append(libraryPath + ": incompatible OAL ABI v1");
        return false;
    }
    const QString legacyId = QString::fromUtf8(api->driverId);
    if (find(legacyId)) {
        if (errors) errors->append(libraryPath + ": duplicate driverId already loaded: " + legacyId);
        return false;
    }
    if (api->start && !api->start(api->context, "{}")) {
        if (errors) errors->append(libraryPath + ": driver start failed");
        return false;
    }
    auto x = std::make_unique<Loaded>();
    x->id = QString::fromUtf8(api->driverId);
    x->name = QString::fromUtf8(api->driverName ? api->driverName : api->driverId);
    x->version = QString::fromUtf8(api->driverVersion ? api->driverVersion : "");
    x->abiVersion = 1;
    x->api1 = api;
    x->library = std::move(lib);
    x->threadModel = "serial";
    loaded_.push_back(std::move(x));
    return true;
}

OalDriverPluginLoader::Loaded *OalDriverPluginLoader::find(const QString &driverId) {
    for (auto &x : loaded_) if (x->id == driverId) return x.get();
    return nullptr;
}
const OalDriverPluginLoader::Loaded *OalDriverPluginLoader::find(const QString &driverId) const {
    for (const auto &x : loaded_) if (x->id == driverId) return x.get();
    return nullptr;
}

QMutex *OalDriverPluginLoader::callMutex(const Loaded *driver, const QString &deviceId) const {
    if (!driver || driver->threadModel == "concurrent") return nullptr;
    if (driver->threadModel != "per-device-serial" || deviceId.isEmpty())
        return &driver->serialMutex;
    QMutexLocker guard(&driver->deviceMutexMapGuard);
    auto it = driver->deviceMutexes.find(deviceId);
    if (it == driver->deviceMutexes.end())
        it = driver->deviceMutexes.insert(deviceId, std::make_shared<QMutex>());
    return it.value().get();
}

namespace {
class OptionalMutexLocker {
public:
    explicit OptionalMutexLocker(QMutex *mutex) : mutex_(mutex) { if (mutex_) mutex_->lock(); }
    ~OptionalMutexLocker() { if (mutex_) mutex_->unlock(); }
    OptionalMutexLocker(const OptionalMutexLocker &) = delete;
    OptionalMutexLocker &operator=(const OptionalMutexLocker &) = delete;
private:
    QMutex *mutex_{};
};
}

QJsonArray OalDriverPluginLoader::drivers() const {
    QJsonArray out;
    for (const auto &x : loaded_) {
        QJsonObject o{{"driverId", x->id}, {"name", x->name}, {"version", x->version},
                      {"abiVersion", x->abiVersion}, {"native", x->abiVersion == 2}};
        if (!x->manifestPath.isEmpty()) o["manifestPath"] = x->manifestPath;
        if (!x->manifest.isEmpty()) {
            o["isolation"] = x->manifest.value("isolation");
            o["permissions"] = x->manifest.value("permissions");
            o["deviceClasses"] = x->manifest.value("deviceClasses");
        }
        out.append(o);
    }
    return out;
}

QJsonArray OalDriverPluginLoader::devices() const {
    QMutexLocker lock(&deviceCacheMutex_);
    return deviceCache_;
}

QJsonArray OalDriverPluginLoader::refreshDevices(QStringList *errors) {
    return refreshDevices(QStringList{}, errors);
}

QJsonArray OalDriverPluginLoader::refreshDevices(const QStringList &driverIds, QStringList *errors) {
    const bool filtered = !driverIds.isEmpty();
    QSet<QString> wanted; for (const auto &id : driverIds) wanted.insert(id);

    QJsonArray refreshed;
    for (const auto &x : loaded_) {
        if (filtered && !wanted.contains(x->id)) continue;
        const char *p = nullptr;
        // Device enumeration is driver-wide hardware I/O. Serialize it with the
        // normal driver call mutex so it cannot race a connect/capture/move.
        OptionalMutexLocker callLock(callMutex(x.get(), QString()));
        if (x->api2 && x->api2->enumerateDevicesJson)
            p = x->api2->enumerateDevicesJson(x->api2->context);
        else if (x->api1 && x->api1->enumerateDevicesJson)
            p = x->api1->enumerateDevicesJson(x->api1->context);
        const auto arr = parseArray(p);
        if (p && arr.isEmpty() && QByteArray(p).trimmed() != "[]" && errors)
            errors->append(x->id + ": device enumeration returned invalid JSON");
        for (auto v : arr) {
            auto o = v.toObject();
            o["driverId"] = x->id;
            o["driverName"] = x->name;
            o["driverVersion"] = x->version;
            o["abiVersion"] = x->abiVersion;
            refreshed.append(o);
        }
        if (p) {
            if (x->api2 && x->api2->releaseString) x->api2->releaseString(x->api2->context, p);
            else if (x->api1 && x->api1->releaseString) x->api1->releaseString(x->api1->context, p);
        }
    }

    QMutexLocker lock(&deviceCacheMutex_);
    if (!filtered) {
        deviceCache_ = refreshed;
        return deviceCache_;
    }

    // Keep cached devices owned by drivers that were not rescanned and replace
    // only the selected drivers. This is critical for a connected QHY camera or
    // ASCOM-owned serial mount: reconnecting a missing Gemini must not enumerate
    // every hardware backend in the process.
    QJsonArray merged;
    for (const auto &v : deviceCache_) {
        const auto o = v.toObject();
        if (!wanted.contains(o.value("driverId").toString())) merged.append(o);
    }
    for (const auto &v : refreshed) merged.append(v);
    deviceCache_ = merged;
    return deviceCache_;
}

QJsonObject OalDriverPluginLoader::capabilities(const QString &driverId, const QString &deviceId,
                                                 QString *error) const {
    const auto *x = find(driverId);
    if (!x) { if (error) *error = "Native driver not found: " + driverId; return {}; }
    if (!x->api2 || !x->api2->capabilitiesJson) {
        if (error) *error = "Driver does not expose ABI-v2 capabilities";
        return {};
    }
    const QByteArray d = deviceId.toUtf8();
    OptionalMutexLocker callLock(callMutex(x, deviceId));
    const char *p = x->api2->capabilitiesJson(x->api2->context, d.constData());
    const auto o = parseObject(p);
    if (p && x->api2->releaseString) x->api2->releaseString(x->api2->context, p);
    if (o.isEmpty() && error) *error = "Driver returned invalid/empty capability document";
    return o;
}

QJsonObject OalDriverPluginLoader::health(const QString &driverId, const QString &deviceId,
                                           QString *error) const {
    const auto *x = find(driverId);
    if (!x) { if (error) *error = "Native driver not found: " + driverId; return {}; }
    if (!x->api2 || !x->api2->healthJson) return {{"state", "unknown"}};
    const QByteArray d = deviceId.toUtf8();
    OptionalMutexLocker callLock(callMutex(x, deviceId));
    const char *p = x->api2->healthJson(x->api2->context, d.constData());
    const auto o = parseObject(p);
    if (p && x->api2->releaseString) x->api2->releaseString(x->api2->context, p);
    if (o.isEmpty() && error) *error = "Driver returned invalid health document";
    return o;
}

QJsonObject OalDriverPluginLoader::invoke(const QString &driverId, const QString &deviceId,
                                           const QString &method, const QJsonObject &request,
                                           QString *error, const QString &operationId,
                                           quint64 deadlineMonotonicNs) {
    auto *x = find(driverId);
    if (!x) { if (error) *error = "Native driver not found: " + driverId; return {}; }
    const QByteArray dev = deviceId.toUtf8();
    const QByteArray meth = method.toUtf8();
    const QByteArray req = QJsonDocument(request).toJson(QJsonDocument::Compact);
    // Abort/HALT are explicit safety/cancellation paths and must be able to
    // interrupt a long per-device-serial call currently holding its normal
    // invocation lock. Drivers are required to make these methods thread-safe.
    const bool bypassSerialLock = method == "camera.abortExposure" ||
                                  method == "mount.abort" ||
                                  method == "focuser.halt";
    OptionalMutexLocker callLock(bypassSerialLock ? nullptr : callMutex(x, deviceId));
    const char *p = nullptr;
    if (x->api2 && x->api2->invokeJson) {
        const QByteArray op = operationId.toUtf8();
        const QByteArray rid = (QString("call-") + QString::number(QDateTime::currentMSecsSinceEpoch())).toUtf8();
        OalDriverCallV2 call{sizeof(OalDriverCallV2), rid.constData(),
                            op.isEmpty() ? nullptr : op.constData(), deadlineMonotonicNs};
        p = x->api2->invokeJson(x->api2->context, dev.constData(), meth.constData(),
                                req.constData(), &call);
    } else if (x->api1 && x->api1->invokeJson) {
        p = x->api1->invokeJson(x->api1->context, dev.constData(), meth.constData(), req.constData());
    }
    if (!p) { if (error) *error = "Driver returned no response"; return {}; }
    const auto doc = QJsonDocument::fromJson(QByteArray(p));
    if (x->api2 && x->api2->releaseString) x->api2->releaseString(x->api2->context, p);
    else if (x->api1 && x->api1->releaseString) x->api1->releaseString(x->api1->context, p);
    if (!doc.isObject()) { if (error) *error = "Driver returned invalid JSON"; return {}; }
    const auto o = doc.object();
    if (!o.value("ok").toBool(true) && error) {
        const auto e = o.value("error").toObject();
        *error = e.value("message").toString("Native driver call failed");
    }
    return o;
}

bool OalDriverPluginLoader::cancel(const QString &driverId, const QString &deviceId,
                                    const QString &operationId, QString *error) {
    auto *x = find(driverId);
    if (!x || !x->api2 || !x->api2->cancel) {
        if (error) *error = "Native driver does not support ABI-v2 cancellation";
        return false;
    }
    const QByteArray d = deviceId.toUtf8();
    const QByteArray op = operationId.toUtf8();
    if (!x->api2->cancel(x->api2->context, d.constData(), op.constData())) {
        if (error) *error = "Native driver rejected cancellation";
        return false;
    }
    return true;
}

quint64 OalDriverPluginLoader::publishFrameFromDriver(const char *driverIdUtf8,
                                                       const char *deviceIdUtf8,
                                                       const void *descriptor) {
    const auto *f = static_cast<const OalFrameDescriptorV2 *>(descriptor);
    if (!f || f->structSize < sizeof(OalFrameDescriptorV2) || !f->data || f->dataBytes == 0)
        return 0;
    NativeDriverFrame out;
    out.token = nextFrameToken_.fetch_add(1);
    out.driverId = QString::fromUtf8(driverIdUtf8 ? driverIdUtf8 : "");
    out.deviceId = QString::fromUtf8(deviceIdUtf8 ? deviceIdUtf8 : "");
    out.frameId = QString::fromUtf8(f->frameIdUtf8 ? f->frameIdUtf8 : "");
    out.width = f->width;
    out.height = f->height;
    out.strideBytes = f->strideBytes;
    out.pixelFormat = f->pixelFormat;
    out.bitsPerSample = f->bitsPerSample;
    out.channels = f->channels;
    out.capturedUnixNs = f->capturedUnixNs;
    out.exposureSec = f->exposureSec;
    out.gain = f->gain;
    out.bytes = QByteArray(reinterpret_cast<const char *>(f->data), qsizetype(f->dataBytes));
    if (f->metadataJsonUtf8) out.metadata = parseObject(f->metadataJsonUtf8);
    QMutexLocker lock(&frameMutex_);
    frames_.insert(out.token, std::move(out));
    return out.token;
}

bool OalDriverPluginLoader::takePublishedFrame(quint64 token, NativeDriverFrame &out,
                                                QString *error) {
    QMutexLocker lock(&frameMutex_);
    const auto it = frames_.find(token);
    if (it == frames_.end()) {
        if (error) *error = QString("Native frame token %1 not found").arg(token);
        return false;
    }
    out = std::move(it.value());
    frames_.erase(it);
    return true;
}

void OalDriverPluginLoader::emitEventFromDriver(const char *driverIdUtf8,
                                                 const char *deviceIdUtf8,
                                                 const char *eventJsonUtf8) {
    const QString driver = QString::fromUtf8(driverIdUtf8 ? driverIdUtf8 : "");
    const QString device = QString::fromUtf8(deviceIdUtf8 ? deviceIdUtf8 : "");
    const QJsonObject event = parseObject(eventJsonUtf8);
    QMetaObject::invokeMethod(this, [this, driver, device, event]() {
        emit driverEvent(driver, device, event);
    }, Qt::QueuedConnection);
}

void OalDriverPluginLoader::logFromDriver(int level, const char *driverIdUtf8,
                                           const char *messageUtf8) {
    const QString driver = QString::fromUtf8(driverIdUtf8 ? driverIdUtf8 : "");
    const QString message = QString::fromUtf8(messageUtf8 ? messageUtf8 : "");

    // Command-line tools such as oal-hardware-probe intentionally do not enter
    // QCoreApplication::exec(). If the driver calls us from the loader's own
    // thread, a queued delivery would therefore never be processed and the most
    // useful diagnostics (for example "COM4: open failed: Access is denied")
    // would disappear. Emit synchronously on the owning thread; retain queued
    // delivery only for callbacks arriving from a driver worker thread.
    if (QThread::currentThread() == thread()) {
        emit driverLog(driver, level, message);
        return;
    }
    QMetaObject::invokeMethod(this, [this, driver, level, message]() {
        emit driverLog(driver, level, message);
    }, Qt::QueuedConnection);
}

} // namespace oas
