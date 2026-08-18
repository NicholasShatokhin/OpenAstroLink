#pragma once
#include <QHttpServer>
#include <QTcpServer>

namespace oas {
class ApplicationController;
class OalServer final : public QObject {
    Q_OBJECT
public:
    explicit OalServer(ApplicationController *controller,QObject *parent=nullptr);
    bool start(quint16 port,QString *error=nullptr);
    void stop();
    bool isRunning() const{return tcp_.isListening();}
    quint16 port() const{return tcp_.serverPort();}
private:
    void registerRoutes();
    ApplicationController *controller_{};
    QHttpServer http_;
    QTcpServer tcp_;
    bool bound_{false};
};
}
