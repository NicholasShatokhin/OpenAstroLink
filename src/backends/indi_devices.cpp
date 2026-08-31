#ifdef OAS_HAVE_INDI
#include "backends/indi_devices.h"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSet>
#include <QMap>
#include <QUrl>
#include <QTcpSocket>
#include <algorithm>
#include <cmath>

namespace oas {
namespace {

QString xmlAttr(QString s) {
    s.replace('&', "&amp;");
    s.replace('<', "&lt;");
    s.replace('>', "&gt;");
    s.replace('"', "&quot;");
    return s;
}

QString xmlText(QString s) {
    s.replace('&', "&amp;");
    s.replace('<', "&lt;");
    s.replace('>', "&gt;");
    return s;
}

bool containsDeviceDefinition(const QByteArray &xml, const QString &device) {
    const QString text = QString::fromUtf8(xml);
    const QRegularExpression re(
        QStringLiteral("<def(?:Number|Switch|Text|Light|BLOB)Vector[^>]*\\bdevice=[\\\"']%1[\\\"']")
            .arg(QRegularExpression::escape(device)));
    return re.match(text).hasMatch();
}



bool openIndiSocket(QTcpSocket &socket, const IndiEndpoint &ep, int timeoutMs, QString *error) {
    socket.connectToHost(ep.host, ep.port);
    if (!socket.waitForConnected(std::min(timeoutMs, 5000))) {
        if (error) *error = socket.errorString();
        return false;
    }
    return true;
}

bool writeIndiSocket(QTcpSocket &socket, const QByteArray &bytes, QString *error) {
    if (socket.write(bytes) != bytes.size() || !socket.waitForBytesWritten(3000)) {
        if (error) *error = socket.errorString();
        return false;
    }
    return true;
}

QByteArray readIndiSocket(QTcpSocket &socket, int timeoutMs,
                          const QByteArray &needle1 = {}, const QByteArray &needle2 = {}) {
    QByteArray all;
    QElapsedTimer timer;
    timer.start();
    qint64 lastDataAt = 0;
    while (timer.elapsed() < timeoutMs) {
        if (socket.waitForReadyRead(100)) {
            all += socket.readAll();
            lastDataAt = timer.elapsed();
        } else if (!all.isEmpty() && timer.elapsed() - lastDataAt > 180 &&
                   ((needle1.isEmpty() && needle2.isEmpty()) ||
                    all.contains(needle1) || all.contains(needle2))) {
            break;
        }
    }
    return all;
}

bool sendIndiMessage(const IndiEndpoint &ep, const QByteArray &bytes, QString *error) {
    QTcpSocket socket;
    if (!openIndiSocket(socket, ep, 5000, error)) return false;
    if (!writeIndiSocket(socket, bytes, error)) return false;
    // Give indiserver a chance to consume the message before closing this
    // short-lived client connection. The hardware connection itself belongs
    // to the driver and is not tied to this TCP client.
    socket.flush();
    socket.waitForBytesWritten(500);
    socket.disconnectFromHost();
    return true;
}
} // namespace

IndiEndpoint IndiEndpoint::parse(const QString &text) {
    IndiEndpoint ep;
    QString x = text.trimmed();
    if (x.startsWith("indi:", Qt::CaseInsensitive)) x = x.mid(5);
    if (x.startsWith("//")) x = x.mid(2);

    const int slash = x.indexOf('/');
    if (slash >= 0) {
        ep.device = QUrl::fromPercentEncoding(x.mid(slash + 1).toUtf8()).trimmed();
        x = x.left(slash);
    }

    const int colon = x.lastIndexOf(':');
    if (colon >= 0 && colon < x.size() - 1) {
        ep.host = x.left(colon).trimmed();
        bool ok = false;
        const uint p = x.mid(colon + 1).toUInt(&ok);
        if (ok && p > 0 && p <= 65535) ep.port = quint16(p);
    } else if (!x.trimmed().isEmpty()) {
        ep.host = x.trimmed();
    }
    if (ep.host.isEmpty()) ep.host = "127.0.0.1";
    return ep;
}

QString IndiEndpoint::normalized() const {
    return QString("%1:%2/%3").arg(host).arg(port).arg(device);
}

QList<IndiDeviceInfo> discoverIndiDevices(const QString &host, quint16 port,
                                          int timeoutMs, QString *error) {
    QTcpSocket socket;
    socket.connectToHost(host, port);
    if (!socket.waitForConnected(std::min(timeoutMs, 5000))) {
        if (error) *error = socket.errorString();
        return {};
    }
    const QByteArray req("<getProperties version=\"1.7\"/>\n");
    if (socket.write(req) != req.size() || !socket.waitForBytesWritten(2000)) {
        if (error) *error = socket.errorString();
        return {};
    }

    QByteArray all;
    QElapsedTimer timer;
    timer.start();
    qint64 lastDataAt = 0;
    while (timer.elapsed() < timeoutMs) {
        if (socket.waitForReadyRead(150)) {
            all += socket.readAll();
            lastDataAt = timer.elapsed();
        } else if (!all.isEmpty() && timer.elapsed() - lastDataAt > 350) {
            break;
        }
    }

    const QString xml = QString::fromUtf8(all);
    const QRegularExpression re(
        QStringLiteral("<def(?:Number|Switch|Text|Light|BLOB)Vector[^>]*\\bdevice=[\\\"']([^\\\"']+)[\\\"'][^>]*\\bname=[\\\"']([^\\\"']+)[\\\"']"));
    QMap<QString, QSet<QString>> found;
    auto it = re.globalMatch(xml);
    while (it.hasNext()) {
        const auto m = it.next();
        found[m.captured(1)].insert(m.captured(2));
    }

    QList<IndiDeviceInfo> out;
    for (auto i = found.cbegin(); i != found.cend(); ++i) {
        IndiDeviceInfo d;
        d.name = i.key();
        for (const auto &p : i.value()) d.properties << p;
        d.properties.sort();
        out.push_back(std::move(d));
    }
    if (out.isEmpty() && error) *error = "No INDI device definitions received";
    return out;
}

bool IndiXmlClient::connectServer(QString *error) {
    if (ep_.device.trimmed().isEmpty()) {
        if (error) *error = "INDI endpoint must include the exact device name: host:7624/Device Name";
        return false;
    }

    // Validate the exact device name without keeping a QTcpSocket as object
    // state. OAL operations run in worker threads; short-lived sockets created
    // in the calling thread avoid QObject thread-affinity violations and let
    // concurrent status/safety requests use independent INDI client sessions.
    QTcpSocket socket;
    if (!openIndiSocket(socket, ep_, 5000, error)) return false;
    const QByteArray req = QString("<getProperties version=\"1.7\" device=\"%1\"/>\n")
                               .arg(xmlAttr(ep_.device)).toUtf8();
    if (!writeIndiSocket(socket, req, error)) return false;
    const QByteArray defs = readIndiSocket(socket, 1800);
    if (!containsDeviceDefinition(defs, ep_.device)) {
        if (error) {
            *error = QString("INDI device '%1' was not found at %2:%3. Use indi_getprop or oal-hardware-probe to list exact names.")
                         .arg(ep_.device, ep_.host).arg(ep_.port);
        }
        return false;
    }

    if (!setConnection(true, error) || !waitConnectionState(true, 5000, error))
        return false;
    return true;
}

void IndiXmlClient::close() {
    if (ep_.device.trimmed().isEmpty()) return;
    QString ignored;
    setConnection(false, &ignored);
}

bool IndiXmlClient::setConnection(bool on, QString *error) {
    return sendSwitch("CONNECTION", {{"CONNECT", on}, {"DISCONNECT", !on}}, error);
}

bool IndiXmlClient::waitConnectionState(bool wantConnected, int timeoutMs, QString *error) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        const auto xml = queryProperty("CONNECTION", std::min(800, timeoutMs - int(timer.elapsed())), nullptr);
        bool connected = false, disconnected = false;
        const bool haveC = switchValue(xml, "CONNECT", connected);
        const bool haveD = switchValue(xml, "DISCONNECT", disconnected);
        const QString state = vectorState(xml, "CONNECTION");
        if (state.compare("Alert", Qt::CaseInsensitive) == 0) {
            if (error) *error = "INDI driver reported CONNECTION=Alert";
            return false;
        }
        if (haveC && haveD && ((wantConnected && connected && !disconnected) ||
                               (!wantConnected && disconnected && !connected)))
            return true;
    }
    if (error) *error = wantConnected ? "INDI device connection timeout" : "INDI device disconnect timeout";
    return false;
}

QByteArray IndiXmlClient::queryProperty(const QString &name, int timeoutMs, QString *error) {
    QTcpSocket socket;
    if (!openIndiSocket(socket, ep_, std::max(1000, timeoutMs), error)) return {};
    const QByteArray req = QString("<getProperties version=\"1.7\" device=\"%1\" name=\"%2\"/>\n")
                               .arg(xmlAttr(ep_.device), xmlAttr(name)).toUtf8();
    if (!writeIndiSocket(socket, req, error)) return {};
    const QByteArray needle1 = ("name=\"" + name + "\"").toUtf8();
    const QByteArray needle2 = ("name='" + name + "'").toUtf8();
    const QByteArray all = readIndiSocket(socket, timeoutMs, needle1, needle2);
    if (all.isEmpty() && error) *error = "INDI property timeout: " + name;
    return all;
}

bool IndiXmlClient::propertyExists(const QString &name, int timeoutMs) {
    QString ignored;
    const auto x = queryProperty(name, timeoutMs, &ignored);
    return x.contains(("name=\"" + name + "\"").toUtf8()) ||
           x.contains(("name='" + name + "'").toUtf8());
}

bool IndiXmlClient::sendNumber(const QString &property,
                               const QList<QPair<QString, double>> &values,
                               QString *error) {
    QString xml = QString("<newNumberVector device=\"%1\" name=\"%2\">")
                      .arg(xmlAttr(ep_.device), xmlAttr(property));
    for (const auto &p : values)
        xml += QString("<oneNumber name=\"%1\">%2</oneNumber>")
                   .arg(xmlAttr(p.first), QString::number(p.second, 'g', 15));
    xml += "</newNumberVector>\n";
    return sendIndiMessage(ep_, xml.toUtf8(), error);
}

bool IndiXmlClient::sendSwitch(const QString &property,
                               const QList<QPair<QString, bool>> &values,
                               QString *error) {
    QString xml = QString("<newSwitchVector device=\"%1\" name=\"%2\">")
                      .arg(xmlAttr(ep_.device), xmlAttr(property));
    for (const auto &p : values)
        xml += QString("<oneSwitch name=\"%1\">%2</oneSwitch>")
                   .arg(xmlAttr(p.first), p.second ? "On" : "Off");
    xml += "</newSwitchVector>\n";
    return sendIndiMessage(ep_, xml.toUtf8(), error);
}

bool IndiXmlClient::number(const QByteArray &xml, const QString &name, double &value) {
    const QRegularExpression re(
        QString("<(?:def|one)Number[^>]*name=[\\\"']%1[\\\"'][^>]*>([^<]+)")
            .arg(QRegularExpression::escape(name)));
    const auto m = re.match(QString::fromUtf8(xml));
    if (!m.hasMatch()) return false;
    bool ok = false;
    const double v = m.captured(1).trimmed().toDouble(&ok);
    if (!ok) return false;
    value = v;
    return true;
}

bool IndiXmlClient::switchValue(const QByteArray &xml, const QString &name, bool &value) {
    const QRegularExpression re(
        QString("<(?:def|one)Switch[^>]*name=[\\\"']%1[\\\"'][^>]*>\\s*(On|Off)")
            .arg(QRegularExpression::escape(name)));
    const auto m = re.match(QString::fromUtf8(xml));
    if (!m.hasMatch()) return false;
    value = m.captured(1).compare("On", Qt::CaseInsensitive) == 0;
    return true;
}

QString IndiXmlClient::vectorState(const QByteArray &xml, const QString &name) {
    const QRegularExpression re(
        QString("<(?:def|set)(?:Number|Switch|Text|Light|BLOB)Vector[^>]*name=[\\\"']%1[\\\"'][^>]*state=[\\\"']([^\\\"']+)[\\\"']")
            .arg(QRegularExpression::escape(name)));
    const auto m = re.match(QString::fromUtf8(xml));
    return m.hasMatch() ? m.captured(1) : QString();
}

IndiMount::IndiMount(QString endpoint)
    : client_(IndiEndpoint::parse(endpoint)), deviceName_(client_.endpoint().device) {
    if (deviceName_.isEmpty()) deviceName_ = "INDI telescope";
}

bool IndiMount::connectDevice(QString *error) {
    state_ = ConnectionState::Connecting;
    if (!client_.connectServer(error)) {
        state_ = ConnectionState::Error;
        return false;
    }
    if (!client_.propertyExists("EQUATORIAL_EOD_COORD", 1500) &&
        !client_.propertyExists("HORIZONTAL_COORD", 1500)) {
        if (error) *error = "Selected INDI device does not expose telescope coordinates";
        client_.close();
        state_ = ConnectionState::Error;
        return false;
    }
    state_ = ConnectionState::Connected;
    return true;
}

void IndiMount::disconnectDevice() {
    client_.close();
    state_ = ConnectionState::Disconnected;
}

bool IndiMount::status(MountStatus &s, QString *error) {
    auto x = client_.queryProperty("EQUATORIAL_EOD_COORD", 1800, error);
    double ra = 0.0, dec = 0.0;
    if (!IndiXmlClient::number(x, "RA", ra) || !IndiXmlClient::number(x, "DEC", dec))
        return false;
    s.connection = state_;
    s.coordinate = {ra * 15.0, dec};
    s.slewing = IndiXmlClient::vectorState(x, "EQUATORIAL_EOD_COORD")
                     .compare("Busy", Qt::CaseInsensitive) == 0;

    auto t = client_.queryProperty("TELESCOPE_TRACK_STATE", 700, nullptr);
    bool trackOn = false;
    if (IndiXmlClient::switchValue(t, "TRACK_ON", trackOn)) s.tracking = trackOn;

    auto p = client_.queryProperty("TELESCOPE_PARK", 700, nullptr);
    bool parked = false;
    if (IndiXmlClient::switchValue(p, "PARK", parked)) s.parked = parked;

    auto pier = client_.queryProperty("TELESCOPE_PIER_SIDE", 500, nullptr);
    bool east = false, west = false;
    if (IndiXmlClient::switchValue(pier, "PIER_EAST", east) && east) s.pierSide = "east";
    else if (IndiXmlClient::switchValue(pier, "PIER_WEST", west) && west) s.pierSide = "west";
    else s.pierSide = "unknown";
    return true;
}

bool IndiMount::slewTo(const EquatorialCoord &target, QString *error) {
    // TRACK is the appropriate GOTO mode for imaging: slew and keep tracking
    // after the target is reached.
    if (!client_.sendSwitch("ON_COORD_SET", {{"SLEW", false}, {"TRACK", true}, {"SYNC", false}}, error))
        return false;
    return client_.sendNumber("EQUATORIAL_EOD_COORD",
                              {{"RA", target.raDeg / 15.0}, {"DEC", target.decDeg}}, error);
}

bool IndiMount::abortMotion(QString *error) {
    return client_.sendSwitch("TELESCOPE_ABORT_MOTION", {{"ABORT_MOTION", true}}, error);
}

bool IndiMount::syncTo(const EquatorialCoord &target, QString *error) {
    if (!client_.sendSwitch("ON_COORD_SET", {{"SLEW", false}, {"TRACK", false}, {"SYNC", true}}, error))
        return false;
    return client_.sendNumber("EQUATORIAL_EOD_COORD",
                              {{"RA", target.raDeg / 15.0}, {"DEC", target.decDeg}}, error);
}

bool IndiMount::setTracking(bool enabled, TrackingRate, QString *error) {
    if (!client_.propertyExists("TELESCOPE_TRACK_STATE", 700)) {
        if (error) *error = "INDI mount does not expose TELESCOPE_TRACK_STATE";
        return false;
    }
    return client_.sendSwitch("TELESCOPE_TRACK_STATE",
                              {{"TRACK_ON", enabled}, {"TRACK_OFF", !enabled}}, error);
}

bool IndiMount::park(bool enabled, QString *error) {
    if (!client_.propertyExists("TELESCOPE_PARK", 700)) {
        if (error) *error = "INDI mount does not expose TELESCOPE_PARK";
        return false;
    }
    return client_.sendSwitch("TELESCOPE_PARK", {{"PARK", enabled}, {"UNPARK", !enabled}}, error);
}

bool IndiMount::pulseGuide(GuideDirection direction, int ms, QString *error) {
    if (direction == GuideDirection::North || direction == GuideDirection::South) {
        if (!client_.propertyExists("TELESCOPE_TIMED_GUIDE_NS", 500)) {
            if (error) *error = "INDI mount does not support NS pulse guiding";
            return false;
        }
        return client_.sendNumber("TELESCOPE_TIMED_GUIDE_NS",
                                  {{"TIMED_GUIDE_N", direction == GuideDirection::North ? double(ms) : 0.0},
                                   {"TIMED_GUIDE_S", direction == GuideDirection::South ? double(ms) : 0.0}}, error);
    }
    if (!client_.propertyExists("TELESCOPE_TIMED_GUIDE_WE", 500)) {
        if (error) *error = "INDI mount does not support WE pulse guiding";
        return false;
    }
    return client_.sendNumber("TELESCOPE_TIMED_GUIDE_WE",
                              {{"TIMED_GUIDE_W", direction == GuideDirection::West ? double(ms) : 0.0},
                               {"TIMED_GUIDE_E", direction == GuideDirection::East ? double(ms) : 0.0}}, error);
}

IndiFocuser::IndiFocuser(QString endpoint)
    : client_(IndiEndpoint::parse(endpoint)), deviceName_(client_.endpoint().device) {
    if (deviceName_.isEmpty()) deviceName_ = "INDI focuser";
}

bool IndiFocuser::connectDevice(QString *error) {
    state_ = ConnectionState::Connecting;
    if (!client_.connectServer(error)) {
        state_ = ConnectionState::Error;
        return false;
    }
    if (!client_.propertyExists("ABS_FOCUS_POSITION", 1200)) {
        if (error) *error = "Selected INDI focuser has no ABS_FOCUS_POSITION; the current OAL autofocus profile requires an absolute focuser";
        client_.close();
        state_ = ConnectionState::Error;
        return false;
    }
    state_ = ConnectionState::Connected;
    return true;
}

void IndiFocuser::disconnectDevice() {
    client_.close();
    state_ = ConnectionState::Disconnected;
}

bool IndiFocuser::status(FocuserStatus &s, QString *error) {
    const auto x = client_.queryProperty("ABS_FOCUS_POSITION", 1500, error);
    double pos = 0.0;
    if (!IndiXmlClient::number(x, "FOCUS_ABSOLUTE_POSITION", pos)) {
        if (error && error->isEmpty()) *error = "INDI focuser has no readable absolute position";
        return false;
    }
    s.connection = state_;
    s.position = int(std::llround(pos));
    s.moving = IndiXmlClient::vectorState(x, "ABS_FOCUS_POSITION")
                   .compare("Busy", Qt::CaseInsensitive) == 0;

    const auto temp = client_.queryProperty("FOCUS_TEMPERATURE", 350, nullptr);
    double t = 0.0;
    if (IndiXmlClient::number(temp, "TEMPERATURE", t) ||
        IndiXmlClient::number(temp, "FOCUS_TEMPERATURE", t))
        s.temperatureC = t;
    return true;
}

bool IndiFocuser::moveAbsolute(int position, QString *error) {
    if (!client_.propertyExists("ABS_FOCUS_POSITION", 700)) {
        if (error) *error = "INDI focuser does not support absolute moves";
        return false;
    }
    return client_.sendNumber("ABS_FOCUS_POSITION",
                              {{"FOCUS_ABSOLUTE_POSITION", double(position)}}, error);
}

bool IndiFocuser::moveRelative(int delta, QString *error) {
    if (delta == 0) return true;
    if (client_.propertyExists("REL_FOCUS_POSITION", 500) &&
        client_.propertyExists("FOCUS_MOTION", 500)) {
        const bool outward = delta > 0;
        if (!client_.sendSwitch("FOCUS_MOTION",
                                {{"FOCUS_INWARD", !outward}, {"FOCUS_OUTWARD", outward}}, error))
            return false;
        return client_.sendNumber("REL_FOCUS_POSITION",
                                  {{"FOCUS_RELATIVE_POSITION", double(std::abs(delta))}}, error);
    }
    FocuserStatus s;
    if (!status(s, error)) return false;
    return moveAbsolute(s.position + delta, error);
}

bool IndiFocuser::halt(QString *error) {
    if (!client_.propertyExists("FOCUS_ABORT_MOTION", 500)) {
        if (error) *error = "INDI focuser does not expose FOCUS_ABORT_MOTION";
        return false;
    }
    return client_.sendSwitch("FOCUS_ABORT_MOTION", {{"ABORT", true}}, error);
}

} // namespace oas
#endif
