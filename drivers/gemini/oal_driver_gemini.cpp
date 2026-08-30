#include "oal/driver_api.h"
#include "myfocuserpro2_protocol.h"
#include "../common_blocking_serial_session.h"

#include <QByteArray>
#include <QElapsedTimer>
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
    int maxPosition{100000};
    bool temperatureSupported{false};
    bool connected{false};
    std::shared_ptr<OalBlockingSerialSession> session;
};

OalDriverHostV2 gHost{};
QMutex gMutex;
QHash<QString, Device> gDevices;
QStringList gConfiguredPorts;
int gProbeTimeoutMs{900};
int gCommandTimeoutMs{2500};
int gMoveTimeoutMs{120000};
int gOpenSettleMs{2200};
int gResetRecoveryMs{1200};

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
    gHost.emitEvent(gHost.hostContext, "oal.gemini", id.constData(), bytes.constData());
}

QString normalizedPort(QString port) { return port.trimmed(); }

QStringList candidatePorts() {
    QStringList out = gConfiguredPorts;
    const QString env = qEnvironmentVariable("OAL_GEMINI_PORT");
    if (!env.isEmpty()) out.prepend(env);
    if (out.isEmpty()) {
        for (const auto &info : QSerialPortInfo::availablePorts()) out << info.portName();
    }
    out.removeDuplicates();
    return out;
}

QString idForPort(const QString &port) {
    for (const auto &info : QSerialPortInfo::availablePorts()) {
        if (info.portName() == port || info.systemLocation() == port) {
            const QString serial = info.serialNumber().trimmed();
            if (!serial.isEmpty()) return "gemini-eaf:" + serial;
        }
    }
    QString safe = port;
    safe.replace('/', '_').replace('\\', '_').replace(':', '_');
    return "gemini-eaf:" + safe;
}

void fillPortMetadata(Device &device) {
    device.description = QStringLiteral("Gemini EAF (native OAL)");
    for (const auto &info : QSerialPortInfo::availablePorts()) {
        if (info.portName() != device.port && info.systemLocation() != device.port) continue;
        device.serial = info.serialNumber();
        if (!info.description().isEmpty())
            device.description = info.description() + QStringLiteral(" / Gemini EAF");
        break;
    }
}


void driverLog(int level, const QString &message) {
    if (!gHost.log) return;
    const QByteArray m = message.toUtf8();
    gHost.log(gHost.hostContext, level, "oal.gemini", m.constData());
}

bool rawExchange(const std::shared_ptr<OalBlockingSerialSession> &session,
                 const QByteArray &command, QByteArray *reply, int timeoutMs,
                 bool expectReply = true) {
    return session && session->exchange(command, reply, timeoutMs, expectReply);
}

std::optional<int> queryInt(const Device &device, const QByteArray &command, char prefix) {
    QByteArray response;
    if (!rawExchange(device.session, command, &response, gCommandTimeoutMs)) return std::nullopt;
    return oal::gemini::parsePrefixedInt(response.trimmed().toStdString(), prefix);
}

std::optional<double> queryDouble(const Device &device, const QByteArray &command,
                                  char prefix) {
    QByteArray response;
    if (!rawExchange(device.session, command, &response, gCommandTimeoutMs)) return std::nullopt;
    return oal::gemini::parsePrefixedDouble(response.trimmed().toStdString(), prefix);
}

std::optional<bool> queryMoving(const Device &device) {
    QByteArray response;
    if (!rawExchange(device.session,
                     QByteArray::fromStdString(oal::gemini::movingCommand()),
                     &response, gCommandTimeoutMs))
        return std::nullopt;
    return oal::gemini::parseMoving(response.trimmed().toStdString());
}

bool populateIdentity(Device &device, QByteArray *probeReply = nullptr) {
    QByteArray response;
    const bool exchangeOk = rawExchange(device.session,
                                        QByteArray::fromStdString(oal::gemini::probeCommand()),
                                        &response, gProbeTimeoutMs);
    if (probeReply) *probeReply = response;
    if (!exchangeOk ||
        !oal::gemini::isProbeResponse(response.trimmed().toStdString()))
        return false;

    QByteArray firmware;
    if (rawExchange(device.session,
                    QByteArray::fromStdString(oal::gemini::firmwareCommand()),
                    &firmware, gCommandTimeoutMs)) {
        const auto text = firmware.trimmed();
        if (text.startsWith('F') && text.endsWith('#'))
            device.firmware = QString::fromUtf8(text.mid(1, text.size() - 2));
    }

    // Historic MyFocuserPro2 revisions have appeared in the field with both
    // :08# and :8# spellings for MaxSteps. Prefer the documented/current form
    // but tolerate the older form so Gemini firmware variants remain usable.
    auto maxPosition = queryInt(device,
                                QByteArray::fromStdString(oal::gemini::maxPositionCommand()),
                                'M');
    if (!maxPosition)
        maxPosition = queryInt(device, QByteArrayLiteral(":8#"), 'M');
    if (maxPosition) device.maxPosition = std::max(1, *maxPosition);

    device.temperatureSupported =
        queryDouble(device,
                    QByteArray::fromStdString(oal::gemini::temperatureCommand()),
                    'Z')
            .has_value();
    return true;
}

bool probePort(const QString &port, Device &device, bool keepOpen) {
    Device result;
    result.port = normalizedPort(port);
    result.id = idForPort(result.port);
    fillPortMetadata(result);
    result.session = std::make_shared<OalBlockingSerialSession>();
    const bool explicitPort = !qEnvironmentVariable("OAL_GEMINI_PORT").trimmed().isEmpty() ||
                              gConfiguredPorts.contains(result.port);
    if (explicitPort) driverLog(1, QString("probing %1 at 9600 baud").arg(result.port));
    QString error;
    if (!result.session->open(result.port, 9600, gOpenSettleMs, &error)) {
        if (explicitPort) driverLog(2, QString("%1: open failed: %2").arg(result.port, error));
        return false;
    }

    // Windows HIL established that this CH340/Gemini controller needs a quiet
    // boot window after Open() before the first :02# byte.  The driver manifest
    // is the authoritative runtime configuration, so v0.2.10.25 keeps its
    // openSettleMs in lock-step with the C++ default (2200 ms).  If the first
    // post-settle exchange still fails, keep the same port open and retry only
    // after an additional recovery delay.  Reopening would restart the device.
    QByteArray probeReply;
    bool identityOk = populateIdentity(result, &probeReply);
    if (explicitPort)
        driverLog(identityOk ? 1 : 2,
                  QString("%1: first :02# exchange: %2; reply='%3' hex=%4")
                      .arg(result.port, result.session->lastExchangeDiagnostic(),
                           QString::fromLatin1(probeReply),
                           QString::fromLatin1(probeReply.toHex(' '))));
    if (!identityOk && gResetRecoveryMs > 0) {
        const int remainingRecoveryMs = gResetRecoveryMs;
        if (explicitPort)
            driverLog(1, QString("%1: no reply after %2 ms quiet-open settle; waiting an additional %3 ms before retry")
                             .arg(result.port)
                             .arg(gOpenSettleMs)
                             .arg(remainingRecoveryMs));
        if (remainingRecoveryMs > 0) QThread::msleep(unsigned(remainingRecoveryMs));
        probeReply.clear();
        identityOk = populateIdentity(result, &probeReply);
        if (explicitPort)
            driverLog(identityOk ? 1 : 2,
                      QString("%1: recovery :02# exchange: %2; reply='%3' hex=%4")
                          .arg(result.port, result.session->lastExchangeDiagnostic(),
                               QString::fromLatin1(probeReply),
                               QString::fromLatin1(probeReply.toHex(' '))));
    }
    if (!identityOk) {
        if (explicitPort)
            driverLog(2, QString("%1: MyFocuserPro2/Gemini handshake failed (:02# -> EOK# expected) after reset-aware retry")
                             .arg(result.port));
        result.session->close();
        return false;
    }
    if (explicitPort) driverLog(1, QString("%1: Gemini EAF discovered (firmware %2)").arg(result.port, result.firmware.isEmpty()?QString("unknown"):result.firmware));
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
        if (device.connected && normalizedPort(device.port) == normalizedPort(port) &&
            device.session && device.session->isOpen())
            return device;
    }
    return std::nullopt;
}

void refreshDevices() {
    QHash<QString, Device> found;
    for (const QString &candidate : candidatePorts()) {
        const QString port = normalizedPort(candidate);
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
    gMoveTimeoutMs = std::clamp(object.value("moveTimeoutMs").toInt(gMoveTimeoutMs),
                                1000, 600000);
    gOpenSettleMs = std::clamp(object.value("openSettleMs").toInt(gOpenSettleMs),
                               0, 3000);
    gResetRecoveryMs =
        std::clamp(object.value("resetRecoveryMs").toInt(gResetRecoveryMs), 0, 10000);
    driverLog(1, QString("serial config: probeTimeoutMs=%1 commandTimeoutMs=%2 openSettleMs=%3 resetRecoveryMs=%4")
                     .arg(gProbeTimeoutMs)
                     .arg(gCommandTimeoutMs)
                     .arg(gOpenSettleMs)
                     .arg(gResetRecoveryMs));
    // Hardware discovery is owned by the OAL host refresh cycle.  Keep driver
    // start side-effect-light so startup does not probe serial ports twice.
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
    return json(QJsonObject{{"driverId", "oal.gemini"},
                            {"name", "OpenAstroLink native Gemini EAF driver"},
                            {"version", "0.2.10.32"},
                            {"abiVersion", 2},
                            {"threadModel", "per-device-serial"},
                            {"protocol", "MyFocuserPro2 serial"},
                            {"baudRate", 9600}});
}

const char *devices(void *) {
    refreshDevices();
    QJsonArray array;
    QMutexLocker lock(&gMutex);
    for (const auto &device : gDevices) {
        array.append(QJsonObject{
            {"id", device.id}, {"type", "focuser"}, {"name", device.description},
            {"serialNumber", device.serial}, {"firmware", device.firmware},
            {"transport", QJsonObject{{"kind", "serial"}, {"port", device.port},
                                      {"baudRate", 9600},
                                      {"persistentSession", device.connected}}}});
    }
    return json(array);
}

const char *capabilities(void *, const char *deviceId) {
    const auto device = getDevice(QString::fromUtf8(deviceId ? deviceId : ""));
    if (!device) return json(QJsonObject{});
    return json(QJsonObject{
        {"schemaVersion", "1.0"},
        {"focuser",
         QJsonObject{
             {"absolute", QJsonObject{{"supported", true}, {"minPosition", 0},
                                      {"maxPosition", device->maxPosition}, {"step", 1}}},
             {"relative", QJsonObject{{"supported", true}}},
             {"halt", QJsonObject{{"supported", false},
                                  {"reason", "No verified Gemini/MyFocuserPro2 halt command is enabled in this driver revision"}}},
             {"temperature", QJsonObject{{"supported", device->temperatureSupported}}},
             {"positionConfidence",
              QJsonObject{{"model", "controller-reported"},
                          {"powerCycleMayInvalidateMechanicalReference", true}}},
             {"firmware", device->firmware},
             {"protocol", "MyFocuserPro2-serial"}}}});
}

const char *health(void *, const char *deviceId) {
    const auto original = getDevice(QString::fromUtf8(deviceId ? deviceId : ""));
    if (!original) return json(QJsonObject{{"state", "missing"}, {"connected", false}});
    Device device=*original;
    if(!device.connected||!device.session||!device.session->isOpen())
        return json(QJsonObject{{"state","disconnected"},{"connected",false},{"port",device.port}});
    // A position read is a non-moving liveness probe. The loader serializes it
    // with normal focuser operations, so unplugging USB becomes an actual
    // device.disconnected event instead of a stale GUI connection.
    const auto position=queryInt(device,QByteArray::fromStdString(oal::gemini::positionCommand()),'P');
    if(!position){
        if(device.session)device.session->close();device.session.reset();device.connected=false;updateDevice(device);
        emitEvent(device,"device.disconnected",QJsonObject{{"reason","Gemini health probe failed"}});
        return json(QJsonObject{{"state","disconnected"},{"connected",false},{"port",device.port},{"reason","health-probe-failed"}});
    }
    return json(QJsonObject{{"state","ok"},{"connected",true},{"port",device.port},{"firmware",device.firmware},{"position",*position},{"serialSessionOpen",true}});
}

const char *invoke(void *, const char *deviceId, const char *method,
                   const char *requestJson, const OalDriverCallV2 *) {
    const QString id = QString::fromUtf8(deviceId ? deviceId : "");
    const QString methodName = QString::fromUtf8(method ? method : "");
    auto optionalDevice = getDevice(id);
    if (!optionalDevice) return fail("DEVICE_NOT_FOUND", "Gemini EAF is no longer present");
    Device device = *optionalDevice;
    const auto request =
        QJsonDocument::fromJson(QByteArray(requestJson ? requestJson : "{}")).object();

    if (methodName == "device.connect") {
        Device connected;
        if (!probePort(device.port, connected, true))
            return fail("CONNECT_FAILED", "Gemini EAF handshake failed on " + device.port);
        connected.connected = true;
        updateDevice(connected);
        emitEvent(connected, "device.connected");
        return ok(QJsonObject{{"port", connected.port}, {"firmware", connected.firmware},
                              {"maxPosition", connected.maxPosition}});
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
        return fail("DEVICE_DISCONNECTED", "Gemini EAF is not connected");

    if (methodName == "focuser.status") {
        const auto position = queryInt(
            device, QByteArray::fromStdString(oal::gemini::positionCommand()), 'P');
        const auto moving = queryMoving(device);
        if (!position || !moving)
            return fail("TRANSPORT_ERROR", "Could not read Gemini EAF position/moving state");
        QJsonObject data{{"position", *position}, {"moving", *moving},
                         {"positionConfidence", "controller-reported"}};
        if (device.temperatureSupported) {
            if (auto temperature = queryDouble(
                    device, QByteArray::fromStdString(oal::gemini::temperatureCommand()), 'Z'))
                data["temperatureC"] = *temperature;
        }
        return ok(data);
    }

    if (methodName == "focuser.moveAbsolute" || methodName == "focuser.moveRelative") {
        int target = request.value("position").toInt(-1);
        if (methodName == "focuser.moveRelative") {
            const auto position = queryInt(
                device, QByteArray::fromStdString(oal::gemini::positionCommand()), 'P');
            if (!position) return fail("TRANSPORT_ERROR", "Could not read Gemini EAF position");
            target = *position + request.value("delta").toInt();
        }
        if (target < 0 || target > device.maxPosition)
            return fail("LIMIT_VIOLATION",
                        QString("Requested focuser position %1 is outside 0..%2")
                            .arg(target)
                            .arg(device.maxPosition));

        const QByteArray command =
            QByteArray::fromStdString(oal::gemini::moveAbsoluteCommand(target));
        if (!rawExchange(device.session, command, nullptr, gCommandTimeoutMs, false))
            return fail("TRANSPORT_ERROR", "Failed to send Gemini EAF move command");
        emitEvent(device, "focuser.moveStarted", QJsonObject{{"targetPosition", target}});
        // Move commands are asynchronous at the OAL driver boundary. Returning
        // immediately keeps the node responsive and allows focuser.status to
        // report controller position + Moving=YES during the physical motion.
        // Higher-level workflows (autofocus) explicitly wait for idle before
        // taking an image at the requested focus position.
        return ok(QJsonObject{{"accepted", true}, {"targetPosition", target}});
    }

    if (methodName == "focuser.halt")
        return fail("NOT_SUPPORTED",
                    "Native Gemini halt is disabled until the exact command is verified against the target firmware");
    return fail("NOT_SUPPORTED", "Unsupported Gemini EAF method: " + methodName);
}

bool cancel(void *, const char *, const char *) {
    // The exact hardware stop command is intentionally not guessed. OAL reports
    // halt/cancel unsupported instead of claiming a safety feature it cannot
    // verify on the target Gemini firmware.
    return false;
}

void release(void *, const char *value) {
    if (value) gHost.deallocate(gHost.hostContext, const_cast<char *>(value));
}

OalDriverV2 api{OAL_DRIVER_ABI_V2,
                sizeof(OalDriverV2),
                OAL_DRIVER_FEATURE_EVENTS | OAL_DRIVER_FEATURE_HEALTH,
                "oal.gemini",
                "OpenAstroLink native Gemini EAF driver",
                "0.2.10.25",
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
