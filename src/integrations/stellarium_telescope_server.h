#pragma once

#include "core/astro_types.h"
#include <QObject>
#include <QTcpServer>
#include <QTimer>
#include <QHash>

class QTcpSocket;

namespace oas {
class ObservatoryController;

// Bridge for Stellarium's external Telescope Control TCP protocol.
// The protocol carries mount position and GOTO only. Camera/focuser/session
// control remains available through OAL; a future native Stellarium plug-in can
// expose the complete observatory UI.
class StellariumTelescopeServer final : public QObject {
    Q_OBJECT
public:
    explicit StellariumTelescopeServer(ObservatoryController *controller, QObject *parent = nullptr);
    bool start(quint16 port, QString *error = nullptr);
    void stop();
    bool isRunning() const { return server_.isListening(); }
    quint16 port() const { return server_.serverPort(); }

signals:
    void logMessage(const QString &message);

private:
    void acceptConnections();
    void onReadyRead(QTcpSocket *socket);
    void processBuffer(QTcpSocket *socket);
    void broadcastPosition();
    static QByteArray makePositionPacket(const MountStatus &status);
    static bool parseGotoPacket(const QByteArray &packet, EquatorialCoord &target);

    ObservatoryController *controller_{};
    QTcpServer server_;
    QTimer positionTimer_;
    QHash<QTcpSocket *, QByteArray> buffers_;
};
} // namespace oas
