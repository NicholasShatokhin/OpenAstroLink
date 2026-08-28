#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <atomic>
#include <memory>
#include <vector>

namespace oas {

struct NativeDriverFrame {
    quint64 token{0};
    QString driverId;
    QString deviceId;
    QString frameId;
    quint32 width{0};
    quint32 height{0};
    quint32 strideBytes{0};
    quint32 pixelFormat{0};
    quint32 bitsPerSample{0};
    quint32 channels{0};
    qint64 capturedUnixNs{0};
    double exposureSec{0.0};
    double gain{0.0};
    QByteArray bytes;
    QJsonObject metadata;
};

class OalDriverPluginLoader final : public QObject {
    Q_OBJECT
public:
    explicit OalDriverPluginLoader(QObject *parent=nullptr);
    ~OalDriverPluginLoader() override;

    // Scan one directory. ABI-v2 *.manifest.json entries are preferred. Any
    // unclaimed shared libraries are then offered the legacy ABI-v1 loader.
    int scan(const QString &directory, QStringList *errors=nullptr, bool append=false);
    int scanDefaultPaths(QStringList *errors=nullptr, bool discoverDevices=true);
    void clear();

    QJsonArray drivers() const;
    // Return the most recently discovered native devices without touching hardware.
    // Discovery is intentionally cached so ordinary metadata/state HTTP requests
    // can never block on USB/serial probes.
    QJsonArray devices() const;
    // Explicitly rescan all loaded native drivers and update the device cache.
    QJsonArray refreshDevices(QStringList *errors=nullptr);
    // Rescan only the requested driver IDs and merge their results into the cached device list.
    // This avoids touching unrelated vendor SDKs/serial ports during reconnect of one missing device.
    QJsonArray refreshDevices(const QStringList &driverIds, QStringList *errors=nullptr);
    QJsonObject capabilities(const QString &driverId, const QString &deviceId,
                             QString *error=nullptr) const;
    QJsonObject health(const QString &driverId, const QString &deviceId,
                       QString *error=nullptr) const;

    QJsonObject invoke(const QString &driverId, const QString &deviceId,
                       const QString &method, const QJsonObject &request,
                       QString *error=nullptr, const QString &operationId={},
                       quint64 deadlineMonotonicNs=0);
    bool cancel(const QString &driverId, const QString &deviceId,
                const QString &operationId, QString *error=nullptr);

    bool takePublishedFrame(quint64 token, NativeDriverFrame &out, QString *error=nullptr);

    // ABI callback bridges. Public only so plain C function pointers can call
    // them; they are not part of the application-facing API.
    quint64 publishFrameFromDriver(const char *driverIdUtf8, const char *deviceIdUtf8,
                                   const void *descriptor);
    void emitEventFromDriver(const char *driverIdUtf8, const char *deviceIdUtf8,
                             const char *eventJsonUtf8);
    void logFromDriver(int level, const char *driverIdUtf8, const char *messageUtf8);

signals:
    void driverEvent(const QString &driverId, const QString &deviceId,
                     const QJsonObject &event);
    void driverLog(const QString &driverId, int level, const QString &message);

private:
    struct Loaded;
    int scanDirectory(const QString &directory, QStringList *errors, bool append);
    bool loadManifest(const QString &manifestPath, QStringList *errors,
                      QStringList *claimedLibraries);
    bool loadLibrary(const QString &libraryPath, const QJsonObject &manifest,
                     const QString &manifestPath, QStringList *errors);
    Loaded *find(const QString &driverId);
    const Loaded *find(const QString &driverId) const;
    QMutex *callMutex(const Loaded *driver, const QString &deviceId) const;

    mutable QMutex deviceCacheMutex_;
    QJsonArray deviceCache_;
    mutable QMutex frameMutex_;
    QHash<quint64, NativeDriverFrame> frames_;
    std::atomic<quint64> nextFrameToken_{1};
    std::vector<std::unique_ptr<Loaded>> loaded_;
};

} // namespace oas
