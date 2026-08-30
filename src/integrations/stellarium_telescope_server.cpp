#include "integrations/stellarium_telescope_server.h"
#include "core/observatory_controller.h"
#include "core/equatorial_frames.h"

#include <QDateTime>
#include <QDataStream>
#include <QHostAddress>
#include <QTcpSocket>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <limits>

namespace oas {
namespace {
constexpr quint16 kMessageTypePositionOrGoto = 0;
constexpr quint16 kGotoPacketSize = 20;
constexpr quint16 kPositionPacketSize = 24;
constexpr double kU32Turns = 4294967296.0; // 2^32

quint32 encodeRa(double raDeg) {
    double x = std::fmod(raDeg, 360.0);
    if (x < 0.0) x += 360.0;
    return quint32(std::llround(x / 360.0 * kU32Turns)) ;
}
qint32 encodeDec(double decDeg) {
    const double clamped = std::clamp(decDeg, -90.0, 90.0);
    const auto v = std::llround(clamped / 360.0 * kU32Turns);
    return qint32(v);
}
double decodeRa(quint32 v) {
    return double(v) * 360.0 / kU32Turns;
}
double decodeDec(qint32 v) {
    return double(v) * 360.0 / kU32Turns;
}
}

StellariumTelescopeServer::StellariumTelescopeServer(ObservatoryController *controller, QObject *parent)
    : QObject(parent), controller_(controller) {
    positionTimer_.setInterval(500);
    connect(&server_, &QTcpServer::newConnection, this, &StellariumTelescopeServer::acceptConnections);
    connect(&positionTimer_, &QTimer::timeout, this, &StellariumTelescopeServer::broadcastPosition);
}

bool StellariumTelescopeServer::start(quint16 port, QString *error) {
    if (server_.isListening()) {
        if (server_.serverPort() == port) return true;
        stop();
    }
    if (!server_.listen(QHostAddress::Any, port)) {
        if (error) *error = server_.errorString();
        return false;
    }
    positionTimer_.start();
    emit logMessage(QString("Stellarium Telescope Control bridge listening on TCP %1").arg(port));
    return true;
}

void StellariumTelescopeServer::stop() {
    positionTimer_.stop();
    for (auto *socket : buffers_.keys()) {
        if (!socket) continue;
        socket->disconnect(this);
        socket->disconnectFromHost();
        delete socket;
    }
    buffers_.clear();
    server_.close();
}

void StellariumTelescopeServer::acceptConnections() {
    while (auto *socket = server_.nextPendingConnection()) {
        buffers_.insert(socket, {});
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            buffers_.remove(socket);
            socket->deleteLater();
        });
        emit logMessage(QString("Stellarium connected from %1; live mount position stream active").arg(socket->peerAddress().toString()));
        QTimer::singleShot(0,this,&StellariumTelescopeServer::broadcastPosition);
    }
}

void StellariumTelescopeServer::onReadyRead(QTcpSocket *socket) {
    buffers_[socket].append(socket->readAll());
    processBuffer(socket);
}

void StellariumTelescopeServer::processBuffer(QTcpSocket *socket) {
    auto &buffer = buffers_[socket];
    while (buffer.size() >= 4) {
        QDataStream head(buffer.left(4));
        head.setByteOrder(QDataStream::LittleEndian);
        quint16 length = 0, type = 0;
        head >> length >> type;
        if (length < 4 || length > 4096) {
            emit logMessage("Stellarium sent an invalid packet length; closing connection");
            socket->disconnectFromHost();
            return;
        }
        if (buffer.size() < length) return;
        const QByteArray packet = buffer.left(length);
        buffer.remove(0, length);

        if (type != kMessageTypePositionOrGoto || length != kGotoPacketSize) continue;
        EquatorialCoord target;
        if (!parseGotoPacket(packet, target)) continue;
        QString error;
        if (!controller_->slewMount(target, &error))
            emit logMessage("Stellarium GOTO rejected: " + error);
        else
            emit logMessage(QString("Stellarium GOTO decoded/forwarded unchanged: RA=%1 deg DEC=%2 deg")
                            .arg(target.raDeg, 0, 'f', 6).arg(target.decDeg, 0, 'f', 6));
    }
}

QByteArray StellariumTelescopeServer::makePositionPacket(const MountStatus &status) {
    const auto j2000=convertEquatorialFrame(status.coordinate,EquatorialFrame::J2000);
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    const quint64 micros = quint64(QDateTime::currentMSecsSinceEpoch()) * 1000ULL;
    const qint32 protocolStatus = status.slewing ? 1 : 0;
    out << quint16(kPositionPacketSize) << quint16(kMessageTypePositionOrGoto)
        << micros << encodeRa(j2000.raDeg) << encodeDec(j2000.decDeg)
        << protocolStatus;
    return packet;
}

bool StellariumTelescopeServer::parseGotoPacket(const QByteArray &packet, EquatorialCoord &target) {
    if (packet.size() != kGotoPacketSize) return false;
    QDataStream in(packet);
    in.setByteOrder(QDataStream::LittleEndian);
    quint16 length = 0, type = 0;
    quint64 timestamp = 0;
    quint32 ra = 0;
    qint32 dec = 0;
    in >> length >> type >> timestamp >> ra >> dec;
    Q_UNUSED(timestamp);
    if (length != kGotoPacketSize || type != kMessageTypePositionOrGoto) return false;
    target.raDeg = decodeRa(ra);
    target.decDeg = decodeDec(dec);
    return std::isfinite(target.raDeg) && std::isfinite(target.decDeg)
        && target.decDeg >= -90.0 && target.decDeg <= 90.0;
}

void StellariumTelescopeServer::broadcastPosition() {
    if (buffers_.isEmpty()) return;
    MountStatus status;
    if (!controller_->mountStatus(status, nullptr) || !status.coordinateValid) return;
    const QByteArray packet = makePositionPacket(status);
    for (auto *socket : buffers_.keys()) {
        if (socket && socket->state() == QAbstractSocket::ConnectedState)
            socket->write(packet);
    }
}
} // namespace oas
