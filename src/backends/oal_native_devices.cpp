#include "backends/oal_native_devices.h"
#include "oal/driver_api.h"
#include "oal/driver_plugin_loader.h"

#include <QDateTime>
#include <QJsonArray>
#include <QUrl>
#include <opencv2/core.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace oas {

QString nativeBackendKey(const QString &driverId, const QString &deviceId) {
    return "native:" + driverId + "/" + QString::fromUtf8(QUrl::toPercentEncoding(deviceId));
}

bool parseNativeBackendKey(const QString &key, QString &driverId, QString &deviceId) {
    if (!key.startsWith("native:")) return false;
    const QString rest = key.mid(7);
    const int slash = rest.indexOf('/');
    if (slash <= 0 || slash == rest.size() - 1) return false;
    driverId = rest.left(slash);
    deviceId = QUrl::fromPercentEncoding(rest.mid(slash + 1).toUtf8());
    return !driverId.isEmpty() && !deviceId.isEmpty();
}

NativeOalDeviceBase::NativeOalDeviceBase(std::shared_ptr<OalDriverPluginLoader> loader,
                                         QJsonObject descriptor)
    : loader_(std::move(loader)),
      driverId_(descriptor.value("driverId").toString()),
      deviceId_(descriptor.value("id").toString()),
      name_(descriptor.value("name").toString(deviceId_)),
      descriptor_(std::move(descriptor)) {}

bool NativeOalDeviceBase::invokeOk(const QString &method, const QJsonObject &request,
                                   QJsonObject *data, QString *error,
                                   const QString &operationId) const {
    if (!loader_) { if (error) *error = "Native driver registry unavailable"; return false; }
    QString e;
    const auto response = loader_->invoke(driverId_, deviceId_, method, request, &e, operationId);
    if (response.isEmpty()) { if (error) *error = e; return false; }
    if (!response.value("ok").toBool(true)) {
        const auto problem = response.value("error").toObject();
        if (error) *error = problem.value("message").toString(e.isEmpty() ? "Native driver call failed" : e);
        return false;
    }
    if (data) *data = response.value("data").toObject();
    return true;
}

QJsonObject NativeOalDeviceBase::capabilities(QString *error) const {
    return loader_ ? loader_->capabilities(driverId_, deviceId_, error) : QJsonObject{};
}

NativeOalCamera::NativeOalCamera(std::shared_ptr<OalDriverPluginLoader> l, const QJsonObject &d)
    : NativeOalDeviceBase(std::move(l), d) {}

bool NativeOalCamera::connectDevice(QString *error) {
    state_ = ConnectionState::Connecting;
    if (!invokeOk("device.connect", {}, nullptr, error)) { state_ = ConnectionState::Error; return false; }
    state_ = ConnectionState::Connected;
    return true;
}
void NativeOalCamera::disconnectDevice() {
    if (state_ == ConnectionState::Disconnected) return;
    invokeOk("device.disconnect", {}, nullptr, nullptr);
    state_ = ConnectionState::Disconnected;
}

bool NativeOalCamera::capture(const ExposureRequest &r, CameraFrame &frame, QString *error) {
    QJsonObject req{{"exposureSec", r.exposureSec}, {"gain", r.gain}, {"offset", r.offset},
                    {"binX", r.binX}, {"binY", r.binY}, {"saveRaw", r.saveRaw}};
    if (!r.savePath.isEmpty()) req["savePath"] = r.savePath;
    if (r.roi.width > 0 && r.roi.height > 0)
        req["roi"] = QJsonObject{{"x", r.roi.x}, {"y", r.roi.y},
                                 {"width", r.roi.width}, {"height", r.roi.height}};
    QJsonObject data;
    if (!invokeOk("camera.capture", req, &data, error)) return false;
    const quint64 token = data.value("frameToken").toVariant().toULongLong();
    if (!token) { if (error) *error = "Native camera returned no frameToken"; return false; }
    NativeDriverFrame native;
    if (!loader_->takePublishedFrame(token, native, error)) return false;

    const int w = int(native.width), h = int(native.height);
    if (w <= 0 || h <= 0) { if (error) *error = "Native camera returned invalid frame dimensions"; return false; }
    int cvType = -1;
    switch (native.pixelFormat) {
    case OAL_PIXEL_MONO8:
    case OAL_PIXEL_BAYER8: cvType = CV_8UC1; break;
    case OAL_PIXEL_MONO16:
    case OAL_PIXEL_BAYER16: cvType = CV_16UC1; break;
    case OAL_PIXEL_RGB8: cvType = CV_8UC3; break;
    case OAL_PIXEL_RGB16: cvType = CV_16UC3; break;
    default:
        if (native.channels == 1 && native.bitsPerSample <= 8) cvType = CV_8UC1;
        else if (native.channels == 1 && native.bitsPerSample <= 16) cvType = CV_16UC1;
        else if (native.channels == 3 && native.bitsPerSample <= 8) cvType = CV_8UC3;
        else if (native.channels == 3 && native.bitsPerSample <= 16) cvType = CV_16UC3;
        break;
    }
    if (cvType < 0) { if (error) *error = "Unsupported native camera pixel format"; return false; }
    const size_t elem = CV_ELEM_SIZE(cvType);
    const size_t minStride = size_t(w) * elem;
    const size_t stride = native.strideBytes ? size_t(native.strideBytes) : minStride;
    if (stride < minStride || native.bytes.size() < qsizetype(stride * size_t(h))) {
        if (error) *error = "Native camera frame buffer is shorter than declared geometry";
        return false;
    }
    frame.image = cv::Mat(h, w, cvType, native.bytes.data(), stride).clone();
    frame.id = native.frameId.isEmpty() ? QString("native-%1").arg(token) : native.frameId;
    frame.capturedUtc = native.capturedUnixNs > 0
        ? QDateTime::fromMSecsSinceEpoch(native.capturedUnixNs / 1000000, Qt::UTC)
        : QDateTime::currentDateTimeUtc();
    frame.exposureSec = native.exposureSec > 0.0 ? native.exposureSec : r.exposureSec;
    frame.gain = int(native.gain);
    // ABI v2 does not yet return explicit actual-bin metadata. For full-frame
    // captures infer it from returned geometry so a driver that ignores a
    // requested bin (for example a DSLR) cannot silently corrupt plate scale.
    int actualBinX = 1;
    int actualBinY = 1;
    const int requestedBinX = std::max(1, r.binX);
    const int requestedBinY = std::max(1, r.binY);
    const QSize fullSensor = sensorSize();
    if (r.roi.width <= 0 && r.roi.height <= 0 && fullSensor.width() > 0 && fullSensor.height() > 0) {
        const int expectedW = std::max(1, fullSensor.width() / requestedBinX);
        const int expectedH = std::max(1, fullSensor.height() / requestedBinY);
        if (std::abs(w - expectedW) <= 1) actualBinX = requestedBinX;
        if (std::abs(h - expectedH) <= 1) actualBinY = requestedBinY;
    } else {
        // ROI semantics are driver-defined in ABI v2; preserve the request
        // until the ABI exposes actual frame binning explicitly.
        actualBinX = requestedBinX;
        actualBinY = requestedBinY;
    }
    frame.binX = actualBinX;
    frame.binY = actualBinY;
    frame.source = deviceId_;
    return true;
}

bool NativeOalCamera::canAbortExposure() const {
    const auto c = capabilities(nullptr).value("camera").toObject().value("exposure").toObject();
    return c.value("abortSupported").toBool(false);
}
bool NativeOalCamera::abortExposure(QString *error) {
    return invokeOk("camera.abortExposure", {}, nullptr, error);
}
QSize NativeOalCamera::sensorSize() const {
    const auto sensor = capabilities(nullptr).value("camera").toObject().value("sensor").toObject();
    return {sensor.value("widthPx").toInt(), sensor.value("heightPx").toInt()};
}

NativeOalMount::NativeOalMount(std::shared_ptr<OalDriverPluginLoader> l, const QJsonObject &d)
    : NativeOalDeviceBase(std::move(l), d) {}
bool NativeOalMount::connectDevice(QString *error) { state_=ConnectionState::Connecting;if(!invokeOk("device.connect",{},nullptr,error)){state_=ConnectionState::Error;return false;}state_=ConnectionState::Connected;return true; }
void NativeOalMount::disconnectDevice(){if(state_==ConnectionState::Disconnected)return;invokeOk("device.disconnect",{},nullptr,nullptr);state_=ConnectionState::Disconnected;}
bool NativeOalMount::status(MountStatus &s, QString *error){QJsonObject d;if(!invokeOk("mount.status",{},&d,error))return false;s.connection=state_;s.coordinate={d.value("raDeg").toDouble(),d.value("decDeg").toDouble()};s.coordinateValid=d.contains("coordinateValid")?d.value("coordinateValid").toBool():true;s.tracking=d.value("tracking").toBool();s.slewing=d.value("slewing").toBool();s.parked=d.value("parked").toBool();s.pierSide=d.value("pierSide").toString("unknown");return true;}
bool NativeOalMount::slewTo(const EquatorialCoord&t,QString*e){return invokeOk("mount.slew",{{"raDeg",t.raDeg},{"decDeg",t.decDeg}},nullptr,e);}
bool NativeOalMount::abortMotion(QString*e){return invokeOk("mount.abort",{},nullptr,e);}
bool NativeOalMount::syncTo(const EquatorialCoord&t,QString*e){return invokeOk("mount.sync",{{"raDeg",t.raDeg},{"decDeg",t.decDeg}},nullptr,e);}
bool NativeOalMount::setTracking(bool v,QString*e){return invokeOk("mount.setTracking",{{"enabled",v}},nullptr,e);}
bool NativeOalMount::park(bool v,QString*e){return invokeOk("mount.park",{{"parked",v}},nullptr,e);}
bool NativeOalMount::pulseGuide(GuideDirection dir,int ms,QString*e){QString d;switch(dir){case GuideDirection::North:d="north";break;case GuideDirection::South:d="south";break;case GuideDirection::East:d="east";break;case GuideDirection::West:d="west";break;}return invokeOk("mount.pulseGuide",{{"direction",d},{"durationMs",ms}},nullptr,e);}

NativeOalFocuser::NativeOalFocuser(std::shared_ptr<OalDriverPluginLoader> l, const QJsonObject &d)
    : NativeOalDeviceBase(std::move(l), d) {}
bool NativeOalFocuser::connectDevice(QString*e){state_=ConnectionState::Connecting;if(!invokeOk("device.connect",{},nullptr,e)){state_=ConnectionState::Error;return false;}state_=ConnectionState::Connected;return true;}
void NativeOalFocuser::disconnectDevice(){if(state_==ConnectionState::Disconnected)return;invokeOk("device.disconnect",{},nullptr,nullptr);state_=ConnectionState::Disconnected;}
bool NativeOalFocuser::status(FocuserStatus&s,QString*e){QJsonObject d;if(!invokeOk("focuser.status",{},&d,e))return false;s.connection=state_;s.position=d.value("position").toInt();s.moving=d.value("moving").toBool();if(d.contains("temperatureC"))s.temperatureC=d.value("temperatureC").toDouble();else s.temperatureC.reset();return true;}
bool NativeOalFocuser::moveAbsolute(int p,QString*e){return invokeOk("focuser.moveAbsolute",{{"position",p}},nullptr,e);}
bool NativeOalFocuser::moveRelative(int d,QString*e){return invokeOk("focuser.moveRelative",{{"delta",d}},nullptr,e);}
bool NativeOalFocuser::halt(QString*e){return invokeOk("focuser.halt",{},nullptr,e);}

} // namespace oas
