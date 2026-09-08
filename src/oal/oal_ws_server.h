#pragma once
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QList>
#include <QMutex>
#include <QPointer>
#include <QObject>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>
#include <atomic>
#ifdef OAS_HAVE_WEBRTC
#include <rtc/rtc.h>
#endif

namespace oas {
class ApplicationController;
class OalWsServer final : public QObject {
    Q_OBJECT
public:
    explicit OalWsServer(ApplicationController *controller,QObject *parent=nullptr);
    ~OalWsServer() override;
    bool start(quint16 port,QString *error=nullptr);void stop();bool isRunning() const{return server_.isListening();}
    void broadcast(const QString &type,const QJsonObject &payload);
    bool hasVideoClients() const{return hasVideoClients_.load(std::memory_order_relaxed)||webrtcOpenChannels_.load(std::memory_order_relaxed)>0;}
    // Thread-safe latest-only video handoff. /events carries JSON state/events;
    // /video carries OALV binary preview frames. When WebRTC is compiled in,
    // /webrtc carries signaling and OALW v1 fragments carry the same OALV/JPEG
    // packet over per-role DataChannels. Acquisition/SER never waits for JPEG
    // encoding or network I/O; slow preview transports drop old preview frames.
    void offerVideoFrame(const QString &role,const QImage &image,const QJsonObject &stats,int jpegQuality=82);
private slots:void newConnection();void disconnected();void tick();
private:
    struct PendingVideo {QImage image;QJsonObject stats;int quality{82};quint64 sequence{0};bool scheduled{false};};
    void flushVideo(const QString &role);
    void sendEncodedVideo(const QString &role,const QByteArray &packet,quint64 sequence);
    static QByteArray videoPacket(const QString &role,const QByteArray &jpeg,const QJsonObject &stats);
#ifdef OAS_HAVE_WEBRTC
    struct WebRtcPeer;
    WebRtcPeer *createWebRtcPeer(QWebSocket *signalSocket);
    void destroyWebRtcPeer(QWebSocket *signalSocket);
    void handleWebRtcSignal(QWebSocket *signalSocket,const QString &message);
    void sendWebRtcSignal(const QPointer<QWebSocket> &signalSocket,const QJsonObject &message);
    static QByteArray webRtcFragment(const QString &role,quint64 sequence,quint32 totalLength,quint16 index,quint16 count,const char *payload,int payloadSize);
    static void rtcDescriptionCallback(int pc,const char *sdp,const char *type,void *ptr);
    static void rtcCandidateCallback(int pc,const char *candidate,const char *mid,void *ptr);
    static void rtcOpenCallback(int dc,void *ptr);
    static void rtcClosedCallback(int dc,void *ptr);
    static void rtcErrorCallback(int dc,const char *error,void *ptr);
    QHash<QWebSocket*,WebRtcPeer*> webrtcPeers_;
#endif
    ApplicationController *controller_{};QWebSocketServer server_;QList<::QWebSocket*> eventClients_;QList<::QWebSocket*> videoClients_;QTimer timer_;
    mutable QMutex videoMutex_;QHash<QString,PendingVideo> pendingVideo_;std::atomic_bool hasVideoClients_{false};std::atomic_int webrtcOpenChannels_{0};
};
}
