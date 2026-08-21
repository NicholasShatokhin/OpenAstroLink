#include "oal/driver_api.h"
#include "synscan_protocol.h"
#include "motor_controller_protocol.h"
#include "../common_blocking_serial_session.h"

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSerialPortInfo>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>

namespace {

struct Device {
    QString id;
    QString port;
    QString serial;
    QString description;
    QString firmware;
    int model{-1};
    bool aligned{false};
    bool pierSideSupported{false};
    bool connected{false};
    std::shared_ptr<OalBlockingSerialSession> session;
};

OalDriverHostV2 gHost{};
QMutex gMutex;
QHash<QString, Device> gDevices;
QStringList gConfiguredPorts;
int gProbeTimeoutMs{850};
int gCommandTimeoutMs{3000};
int gOpenSettleMs{60};

char *copyString(const QByteArray &bytes) {
    auto *p = static_cast<char *>(gHost.allocate(gHost.hostContext,
                                                  std::size_t(bytes.size() + 1)));
    if (!p) return nullptr;
    std::memcpy(p, bytes.constData(), std::size_t(bytes.size()));
    p[bytes.size()] = '\0';
    return p;
}
const char *json(const QJsonObject &object) {
    return copyString(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
const char *json(const QJsonArray &array) {
    return copyString(QJsonDocument(array).toJson(QJsonDocument::Compact));
}
const char *ok(const QJsonObject &data = {}) {
    return json(QJsonObject{{"ok", true}, {"data", data}});
}
const char *fail(const QString &code, const QString &message) {
    return json(QJsonObject{{"ok", false},
                            {"error", QJsonObject{{"code", code},
                                                  {"message", message}}}});
}

void emitEvent(const Device &device, const QString &type,
               const QJsonObject &payload = {}) {
    if (!gHost.emitEvent) return;
    const auto bytes = QJsonDocument(QJsonObject{{"type", type}, {"payload", payload}})
                           .toJson(QJsonDocument::Compact);
    const QByteArray id = device.id.toUtf8();
    gHost.emitEvent(gHost.hostContext, "oal.skywatcher", id.constData(),
                    bytes.constData());
}

QStringList candidatePorts() {
    QStringList ports = gConfiguredPorts;
    const QString env = qEnvironmentVariable("OAL_SKYWATCHER_PORT");
    if (!env.isEmpty()) ports.prepend(env);
    if (ports.isEmpty()) {
        for (const auto &info : QSerialPortInfo::availablePorts()) ports << info.portName();
    }
    ports.removeDuplicates();
    return ports;
}

QString makeId(const QString &port) {
    for (const auto &info : QSerialPortInfo::availablePorts()) {
        if (info.portName() == port || info.systemLocation() == port) {
            const QString serial = info.serialNumber().trimmed();
            if (!serial.isEmpty()) return "skywatcher-synscan:" + serial;
        }
    }
    QString safe = port;
    safe.replace('/', '_').replace('\\', '_').replace(':', '_');
    return "skywatcher-synscan:" + safe;
}

QString modelName(int model) {
    switch (model) {
    case 0: return QStringLiteral("EQ6 GOTO Series");
    case 1: return QStringLiteral("HEQ5 GOTO Series");
    case 2: return QStringLiteral("EQ5 GOTO Series");
    case 3: return QStringLiteral("EQ3 GOTO Series");
    case 4: return QStringLiteral("EQ8 GOTO Series");
    case 5: return QStringLiteral("AZ-EQ6 GOTO Series");
    case 6: return QStringLiteral("AZ-EQ5 GOTO Series");
    case 160: return QStringLiteral("AllView GOTO Series");
    default:
        if (model >= 128 && model <= 143) return QStringLiteral("AZ GOTO Series");
        if (model >= 144 && model <= 159) return QStringLiteral("DOB GOTO Series");
        return QString("SynScan model %1").arg(model);
    }
}

bool equatorialModel(int model) { return model >= 0 && model < 128; }

QString parseFirmwareVersion(const QByteArray &response) {
    const QByteArray text = response.trimmed();
    if (!text.endsWith('#') || text.size() != 7) return QString::fromLatin1(text);
    bool okMajor = false, okMinor = false, okPatch = false;
    const int major = text.mid(0, 2).toInt(&okMajor, 16);
    const int minor = text.mid(2, 2).toInt(&okMinor, 16);
    const int patch = text.mid(4, 2).toInt(&okPatch, 16);
    if (!okMajor || !okMinor || !okPatch) return QString::fromLatin1(text.left(6));
    return QString("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

bool exchange(const Device &device, const QByteArray &command, QByteArray *reply,
              int timeoutMs, bool expectReply = true) {
    return device.session &&
           device.session->exchange(command, reply, timeoutMs, expectReply);
}

std::optional<int> queryByte(const Device &device, const QByteArray &command) {
    QByteArray response;
    if (!exchange(device, command, &response, gCommandTimeoutMs) || response.size() < 2)
        return std::nullopt;
    return int(static_cast<unsigned char>(response[0]));
}

std::optional<bool> alignment(const Device &device) {
    const auto value = queryByte(device, QByteArrayLiteral("J"));
    if (!value) return std::nullopt;
    return *value != 0;
}

std::optional<bool> gotoBusy(const Device &device) {
    QByteArray response;
    if (!exchange(device, QByteArrayLiteral("L"), &response, gCommandTimeoutMs))
        return std::nullopt;
    if (response.startsWith('0')) return false;
    if (response.startsWith('1')) return true;
    return std::nullopt;
}

std::optional<int> trackingMode(const Device &device) {
    return queryByte(device, QByteArrayLiteral("t"));
}

std::optional<QString> pierSide(const Device &device) {
    QByteArray response;
    if (!exchange(device, QByteArrayLiteral("p"), &response, gCommandTimeoutMs))
        return std::nullopt;
    if (response.startsWith('E')) return QStringLiteral("east");
    if (response.startsWith('W')) return QStringLiteral("west");
    return std::nullopt;
}

bool position(const Device &device, double &raDeg, double &decDeg) {
    QByteArray response;
    if (!exchange(device, QByteArrayLiteral("e"), &response, gCommandTimeoutMs))
        return false;
    return oal::synscan::parsePreciseRaDec(response.trimmed().toStdString(),
                                           raDeg, decDeg);
}

void fillPortMetadata(Device &device) {
    device.description = QStringLiteral("Sky-Watcher SynScan mount (native OAL)");
    for (const auto &info : QSerialPortInfo::availablePorts()) {
        if (info.portName() != device.port && info.systemLocation() != device.port) continue;
        device.serial = info.serialNumber();
        if (!info.description().isEmpty())
            device.description = info.description() + QStringLiteral(" / Sky-Watcher SynScan");
        break;
    }
}

bool populateIdentity(Device &device) {
    QByteArray response;
    const QByteArray echo = QByteArray::fromStdString(oal::synscan::echoProbe('O'));
    if (!exchange(device, echo, &response, gProbeTimeoutMs) ||
        response != QByteArrayLiteral("O#"))
        return false;

    if (const auto model = queryByte(device, QByteArrayLiteral("m"))) device.model = *model;
    if (const auto aligned = alignment(device)) device.aligned = *aligned;
    QByteArray version;
    if (exchange(device, QByteArrayLiteral("V"), &version, gCommandTimeoutMs))
        device.firmware = parseFirmwareVersion(version);
    device.pierSideSupported = equatorialModel(device.model) && pierSide(device).has_value();
    return true;
}

bool probePort(const QString &port, Device &device, bool keepOpen) {
    Device result;
    result.port = port.trimmed();
    result.id = makeId(result.port);
    fillPortMetadata(result);
    result.session = std::make_shared<OalBlockingSerialSession>();
    QString error;
    if (!result.session->open(result.port, 9600, gOpenSettleMs, &error)) return false;
    if (!populateIdentity(result)) {
        result.session->close();
        return false;
    }
    if (!keepOpen) {
        result.session->close();
        result.session.reset();
    }
    device = std::move(result);
    return true;
}

std::optional<Device> connectedDeviceForPort(const QString &port) {
    QMutexLocker lock(&gMutex);
    for (const auto &device : gDevices) {
        if (device.connected && device.port.trimmed() == port.trimmed() && device.session &&
            device.session->isOpen())
            return device;
    }
    return std::nullopt;
}

void refreshDevices() {
    QHash<QString, Device> found;
    for (const auto &candidate : candidatePorts()) {
        const QString port = candidate.trimmed();
        if (auto existing = connectedDeviceForPort(port)) {
            found.insert(existing->id, *existing);
            continue;
        }
        Device discovered;
        if (probePort(port, discovered, false)) {
            QMutexLocker lock(&gMutex);
            if (gDevices.contains(discovered.id))
                discovered.connected = gDevices.value(discovered.id).connected;
            found.insert(discovered.id, discovered);
        }
    }
    QMutexLocker lock(&gMutex);
    for (auto it = gDevices.begin(); it != gDevices.end(); ++it) {
        if (it.value().connected && !found.contains(it.key())) found.insert(it.key(), it.value());
    }
    gDevices = found;
}

std::optional<Device> getDevice(const QString &id) {
    QMutexLocker lock(&gMutex);
    const auto it = gDevices.find(id);
    if (it == gDevices.end()) return std::nullopt;
    return it.value();
}

void updateDevice(const Device &device) {
    QMutexLocker lock(&gMutex);
    gDevices[device.id] = device;
}

bool fixedRate(const Device &device, bool raAxis, bool positive, int rate) {
    QByteArray command;
    command.append('P');
    command.append(char(2));
    command.append(char(raAxis ? 16 : 17));
    command.append(char(positive ? 36 : 37));
    command.append(char(std::clamp(rate, 0, 9)));
    command.append(char(0));
    command.append(char(0));
    command.append(char(0));
    QByteArray response;
    return exchange(device, command, &response, gCommandTimeoutMs) && response.endsWith('#');
}

bool stopManualAxes(const Device &device) {
    const bool ra = fixedRate(device, true, true, 0);
    const bool dec = fixedRate(device, false, true, 0);
    return ra && dec;
}

bool start(void *, const char *configJson) {
    const auto object = QJsonDocument::fromJson(QByteArray(configJson ? configJson : "{}"))
                            .object();
    for (const auto &value : object.value("ports").toArray())
        if (value.isString()) gConfiguredPorts << value.toString();
    gConfiguredPorts.removeDuplicates();
    gProbeTimeoutMs = std::clamp(object.value("probeTimeoutMs").toInt(gProbeTimeoutMs),
                                 100, 10000);
    gCommandTimeoutMs = std::clamp(object.value("commandTimeoutMs").toInt(gCommandTimeoutMs),
                                   250, 15000);
    gOpenSettleMs = std::clamp(object.value("openSettleMs").toInt(gOpenSettleMs), 0, 3000);
    refreshDevices();
    return true;
}

void stop(void *) {
    QMutexLocker lock(&gMutex);
    for (auto it = gDevices.begin(); it != gDevices.end(); ++it) {
        if (it.value().session) it.value().session->close();
        it.value().session.reset();
        it.value().connected = false;
    }
}

const char *manifest(void *) {
    return json(QJsonObject{{"driverId", "oal.skywatcher"},
                            {"name", "OpenAstroLink native Sky-Watcher/SynScan driver"},
                            {"version", "0.2.9"},
                            {"abiVersion", 2},
                            {"threadModel", "per-device-serial"},
                            {"protocol", "SynScan Serial Communication Protocol 3.3"},
                            {"baudRate", 9600}});
}

const char *devices(void *) {
    refreshDevices();
    QJsonArray array;
    QMutexLocker lock(&gMutex);
    for (const auto &device : gDevices) {
        array.append(QJsonObject{
            {"id", device.id}, {"type", "mount"}, {"name", device.description},
            {"serialNumber", device.serial}, {"firmware", device.firmware},
            {"model", modelName(device.model)},
            {"transport", QJsonObject{{"kind", "serial"},
                                      {"protocol", "synscan-hand-controller-v3.3"},
                                      {"port", device.port}, {"baudRate", 9600},
                                      {"persistentSession", device.connected}}}});
    }
    return json(array);
}

const char *capabilities(void *, const char *deviceId) {
    const auto device = getDevice(QString::fromUtf8(deviceId ? deviceId : ""));
    if (!device) return json(QJsonObject{});
    const bool equatorial = equatorialModel(device->model);
    QJsonArray trackingModes{QStringLiteral("off")};
    trackingModes.append(equatorial ? QStringLiteral("equatorial")
                                    : QStringLiteral("altaz"));
    return json(QJsonObject{
        {"schemaVersion", "1.0"},
        {"mount",
         QJsonObject{
             {"position", QJsonObject{{"supported", true}, {"frame", "J2000"},
                                      {"precisionArcsec", 0.08}}},
             {"slew", QJsonObject{{"supported", true}, {"abortSupported", true},
                                  {"requiresAlignment", true},
                                  {"coordinatePrecisionArcsec", 0.08}}},
             {"sync", QJsonObject{{"supported", true}, {"frame", "J2000"}}},
             {"tracking", QJsonObject{{"supported", true}, {"modes", trackingModes}}},
             {"park", QJsonObject{{"supported", false},
                                  {"reason", "SynScan serial protocol 3.3 exposes no normative park command"}}},
             {"pulseGuide", QJsonObject{{"supported", equatorial},
                                        {"implementation", "fixed-rate-1"},
                                        {"maxDurationMs", 5000}}},
             {"pierSide", QJsonObject{{"supported", device->pierSideSupported}}},
             {"alignment", QJsonObject{{"querySupported", true},
                                       {"requiredForRaDecGoto", true}}},
             {"model", modelName(device->model)}, {"modelCode", device->model},
             {"firmware", device->firmware},
             {"protocol", "SynScan Serial Communication Protocol 3.3"},
             {"directMotorController",
              QJsonObject{{"status", "experimental-codec-only"},
                          {"serialSupportedByProtocol", true},
                          {"wifiUdpPort", 11880},
                          {"reason", "Direct axis commands require a calibrated OAL alignment/coordinate model before RA/DEC GOTO is exposed"}}}}}});
}

const char *health(void *, const char *deviceId) {
    const auto device = getDevice(QString::fromUtf8(deviceId ? deviceId : ""));
    if (!device)
        return json(QJsonObject{{"state", "missing"}, {"connected", false}});
    return json(QJsonObject{{"state", device->connected ? "ok" : "disconnected"},
                            {"connected", device->connected}, {"port", device->port},
                            {"model", modelName(device->model)}, {"firmware", device->firmware},
                            {"aligned", device->aligned},
                            {"serialSessionOpen", device->session && device->session->isOpen()}});
}

const char *invoke(void *, const char *deviceId, const char *method,
                   const char *requestJson, const OalDriverCallV2 *) {
    const QString id = QString::fromUtf8(deviceId ? deviceId : "");
    const QString methodName = QString::fromUtf8(method ? method : "");
    auto optionalDevice = getDevice(id);
    if (!optionalDevice)
        return fail("DEVICE_NOT_FOUND", "Sky-Watcher SynScan mount is no longer present");
    Device device = *optionalDevice;
    const auto request =
        QJsonDocument::fromJson(QByteArray(requestJson ? requestJson : "{}")).object();

    if (methodName == "device.connect") {
        Device connected;
        if (!probePort(device.port, connected, true))
            return fail("CONNECT_FAILED", "SynScan echo handshake failed on " + device.port);
        connected.connected = true;
        updateDevice(connected);
        emitEvent(connected, "device.connected");
        return ok(QJsonObject{{"port", connected.port},
                              {"model", modelName(connected.model)},
                              {"firmware", connected.firmware},
                              {"aligned", connected.aligned}});
    }
    if (methodName == "device.disconnect") {
        if (device.session) device.session->close();
        device.session.reset();
        device.connected = false;
        updateDevice(device);
        emitEvent(device, "device.disconnected");
        return ok();
    }
    if (!device.connected || !device.session || !device.session->isOpen())
        return fail("DEVICE_DISCONNECTED", "Sky-Watcher mount is not connected");

    if (methodName == "mount.status") {
        double raDeg = 0.0, decDeg = 0.0;
        if (!position(device, raDeg, decDeg))
            return fail("TRANSPORT_ERROR", "Could not read precise RA/DEC from SynScan");
        const auto tracking = trackingMode(device);
        const auto busy = gotoBusy(device);
        const auto side = device.pierSideSupported ? pierSide(device) : std::nullopt;
        const auto aligned = alignment(device);
        if (aligned) {
            device.aligned = *aligned;
            updateDevice(device);
        }
        return ok(QJsonObject{{"raDeg", raDeg}, {"decDeg", decDeg},
                              {"coordinateFrame", "J2000"},
                              {"tracking", tracking && *tracking != 0},
                              {"trackingMode", tracking.value_or(0)},
                              {"slewing", busy.value_or(false)},
                              {"parked", false}, {"parkSupported", false},
                              {"pierSide", side.value_or(QStringLiteral("unknown"))},
                              {"aligned", aligned.value_or(device.aligned)}});
    }

    if (methodName == "mount.slew") {
        const auto aligned = alignment(device);
        if (!aligned || !*aligned)
            return fail("MOUNT_NOT_ALIGNED",
                        "SynScan RA/DEC GOTO requires a completed hand-controller alignment");
        const double raDeg = request.value("raDeg").toDouble(-1.0);
        const double decDeg = request.value("decDeg").toDouble(999.0);
        if (raDeg < 0.0 || raDeg >= 360.0 || decDeg < -90.0 || decDeg > 90.0)
            return fail("INVALID_COORDINATE", "RA/DEC outside valid range");
        QByteArray response;
        const QByteArray command =
            QByteArray::fromStdString(oal::synscan::gotoRaDec(raDeg, decDeg));
        if (!exchange(device, command, &response, gCommandTimeoutMs) ||
            !response.endsWith('#'))
            return fail("TRANSPORT_ERROR", "SynScan rejected/failed precise GOTO command");
        // OAL's operation manager owns the long-running slew lifecycle and polls
        // mount.status. The driver must return after the command is accepted so
        // cancellation/ABORT remains responsive.
        emitEvent(device, "mount.slewStarted",
                  QJsonObject{{"raDeg", raDeg}, {"decDeg", decDeg}});
        return ok(QJsonObject{{"accepted", true}, {"raDeg", raDeg}, {"decDeg", decDeg}});
    }

    if (methodName == "mount.abort") {
        QByteArray response;
        const bool gotoAbort =
            exchange(device, QByteArrayLiteral("M"), &response, gCommandTimeoutMs) &&
            response.endsWith('#');
        const bool manualStop = stopManualAxes(device);
        if (!gotoAbort && !manualStop)
            return fail("TRANSPORT_ERROR", "Could not abort SynScan motion");
        emitEvent(device, "mount.motionAborted");
        return ok();
    }

    if (methodName == "mount.sync") {
        const double raDeg = request.value("raDeg").toDouble(-1.0);
        const double decDeg = request.value("decDeg").toDouble(999.0);
        if (raDeg < 0.0 || raDeg >= 360.0 || decDeg < -90.0 || decDeg > 90.0)
            return fail("INVALID_COORDINATE", "RA/DEC outside valid range");
        QByteArray response;
        if (!exchange(device,
                      QByteArray::fromStdString(oal::synscan::syncRaDec(raDeg, decDeg)),
                      &response, gCommandTimeoutMs) ||
            !response.endsWith('#'))
            return fail("TRANSPORT_ERROR", "SynScan sync failed");
        emitEvent(device, "mount.synced", QJsonObject{{"raDeg", raDeg}, {"decDeg", decDeg}});
        return ok();
    }

    if (methodName == "mount.setTracking") {
        const bool enabled = request.value("enabled").toBool();
        QByteArray command;
        command.append('T');
        command.append(char(enabled ? (equatorialModel(device.model) ? 2 : 1) : 0));
        QByteArray response;
        if (!exchange(device, command, &response, gCommandTimeoutMs) ||
            !response.endsWith('#'))
            return fail("TRANSPORT_ERROR", "SynScan tracking command failed");
        emitEvent(device, "mount.tracking", QJsonObject{{"enabled", enabled}});
        return ok();
    }

    if (methodName == "mount.park")
        return fail("NOT_SUPPORTED",
                    "SynScan serial protocol 3.3 does not define a normative park command; use an OAL higher-level park workflow or a compatibility driver if park is required now");

    if (methodName == "mount.pulseGuide") {
        if (!equatorialModel(device.model))
            return fail("NOT_SUPPORTED",
                        "Native fixed-rate pulse guiding is enabled only for equatorial SynScan models");
        const QString direction = request.value("direction").toString();
        const int durationMs = std::clamp(request.value("durationMs").toInt(), 1, 5000);
        const bool raAxis = direction == "east" || direction == "west";
        const bool positive = direction == "east" || direction == "north";
        if (!(raAxis || direction == "north" || direction == "south"))
            return fail("INVALID_ARGUMENT", "Unknown guide direction");
        if (!fixedRate(device, raAxis, positive, 1))
            return fail("TRANSPORT_ERROR", "Could not start SynScan guide pulse");
        QThread::msleep(unsigned(durationMs));
        if (!fixedRate(device, raAxis, positive, 0))
            return fail("TRANSPORT_ERROR", "Could not stop SynScan guide pulse");
        return ok();
    }

    return fail("NOT_SUPPORTED", "Unsupported Sky-Watcher method: " + methodName);
}

bool cancel(void *, const char *deviceId, const char *) {
    const auto device = getDevice(QString::fromUtf8(deviceId ? deviceId : ""));
    if (!device || !device->connected || !device->session) return false;
    QByteArray response;
    const bool gotoAbort =
        exchange(*device, QByteArrayLiteral("M"), &response, gCommandTimeoutMs);
    stopManualAxes(*device);
    return gotoAbort;
}

void release(void *, const char *value) {
    if (value) gHost.deallocate(gHost.hostContext, const_cast<char *>(value));
}

OalDriverV2 api{OAL_DRIVER_ABI_V2,
                sizeof(OalDriverV2),
                OAL_DRIVER_FEATURE_EVENTS | OAL_DRIVER_FEATURE_CANCELLATION |
                    OAL_DRIVER_FEATURE_HEALTH,
                "oal.skywatcher",
                "OpenAstroLink native Sky-Watcher/SynScan driver",
                "0.2.9",
                nullptr,
                &manifest,
                &start,
                &stop,
                &devices,
                &capabilities,
                &health,
                &invoke,
                &cancel,
                &release};

} // namespace

extern "C" OAL_DRIVER_EXPORT const OalDriverV2 *
oalCreateDriverV2(const OalDriverHostV2 *host) {
    if (!host || host->abiVersion != OAL_DRIVER_ABI_V2 || !host->allocate ||
        !host->deallocate)
        return nullptr;
    gHost = *host;
    return &api;
}
