#pragma once
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>

namespace oas {
class ApplicationController;
class OalWsServer final : public QObject {
    Q_OBJECT
public:
    explicit OalWsServer(ApplicationController *controller,QObject *parent=nullptr);
    bool start(quint16 port,QString *error=nullptr);void stop();bool isRunning() const{return server_.isListening();}
    void broadcast(const QString &type,const QJsonObject &payload);
private slots:void newConnection();void disconnected();void tick();
private:ApplicationController *controller_{};QWebSocketServer server_;QList<::QWebSocket*> clients_;QTimer timer_;
};
}
