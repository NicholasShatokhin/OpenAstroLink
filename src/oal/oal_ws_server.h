#pragma once
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>
#include <atomic>

namespace oas {
class ApplicationController;
class OalWsServer final : public QObject {
    Q_OBJECT
public:
    explicit OalWsServer(ApplicationController *controller,QObject *parent=nullptr);
    bool start(quint16 port,QString *error=nullptr);void stop();bool isRunning() const{return server_.isListening();}
    void broadcast(const QString &type,const QJsonObject &payload);
    bool hasVideoClients() const{return hasVideoClients_.load(std::memory_order_relaxed);}
    // Thread-safe latest-only video handoff. /events carries JSON state/events;
    // /video carries OALV binary preview frames. Acquisition never waits for
    // JPEG encoding or socket I/O. Slow preview clients lose old PREVIEW frames
    // while raw acquisition/SER continues independently.
    void offerVideoFrame(const QString &role,const QImage &image,const QJsonObject &stats,int jpegQuality=82);
private slots:void newConnection();void disconnected();void tick();
private:
    struct PendingVideo {QImage image;QJsonObject stats;int quality{82};quint64 sequence{0};bool scheduled{false};};
    void flushVideo(const QString &role);
    void sendEncodedVideo(const QString &role,const QByteArray &packet,quint64 sequence);
    static QByteArray videoPacket(const QString &role,const QByteArray &jpeg,const QJsonObject &stats);
    ApplicationController *controller_{};QWebSocketServer server_;QList<::QWebSocket*> eventClients_;QList<::QWebSocket*> videoClients_;QTimer timer_;
    mutable QMutex videoMutex_;QHash<QString,PendingVideo> pendingVideo_;std::atomic_bool hasVideoClients_{false};
};
}
