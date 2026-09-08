#include "oal/oal_ws_server.h"
#include "core/application_controller.h"
#include <QBuffer>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QPointer>
#include <QRegularExpression>
#include <QStringList>
#include <QThreadPool>
#include <QtEndian>
#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace oas {
namespace {
constexpr int kWebRtcFragmentPayload=48*1024;
constexpr int kWebRtcBufferedLimit=2*1024*1024;
constexpr quint32 kWebRtcMaxFrameBytes=32u*1024u*1024u;
}

#ifdef OAS_HAVE_WEBRTC
struct OalWsServer::WebRtcPeer {
    QPointer<OalWsServer> owner;
    QPointer<QWebSocket> signal;
    std::atomic_int *openCounter{};
    int pc{-1};
    int mainDc{-1};
    int guideDc{-1};
    std::atomic_bool mainOpen{false};
    std::atomic_bool guideOpen{false};
    bool remoteDescriptionSet{false}; // accessed only on the Qt signaling thread
    QList<QPair<QString,QString>> pendingCandidates;
};
#endif

OalWsServer::OalWsServer(ApplicationController*c,QObject*p):QObject(p),controller_(c),server_("OpenAstroLink events/video/WebRTC",QWebSocketServer::NonSecureMode,this),timer_(this){connect(&server_,&QWebSocketServer::newConnection,this,&OalWsServer::newConnection);connect(&timer_,&QTimer::timeout,this,&OalWsServer::tick);timer_.setInterval(1000);}
OalWsServer::~OalWsServer(){stop();}
bool OalWsServer::start(quint16 p,QString*e){stop();if(!server_.listen(QHostAddress::Any,p)){if(e)*e=server_.errorString();return false;}timer_.start();return true;}
void OalWsServer::stop(){
    timer_.stop();
#ifdef OAS_HAVE_WEBRTC
    const auto rtcSockets=webrtcPeers_.keys();
    for(auto*c:rtcSockets){
        destroyWebRtcPeer(c);
        if(!c)continue;
        c->disconnect();
        c->close();
        delete c;
    }
#endif
    auto close=[](QList<QWebSocket*>&xs){for(auto*c:xs){if(!c)continue;c->disconnect();c->close();delete c;}xs.clear();};close(eventClients_);close(videoClients_);hasVideoClients_.store(false,std::memory_order_relaxed);webrtcOpenChannels_.store(0,std::memory_order_relaxed);{QMutexLocker l(&videoMutex_);pendingVideo_.clear();}server_.close();
}
void OalWsServer::newConnection(){
    while(server_.hasPendingConnections()){
        auto*c=server_.nextPendingConnection();const QString path=c->requestUrl().path();
        if(path=="/video")videoClients_<<c;
#ifdef OAS_HAVE_WEBRTC
        else if(path=="/webrtc"){
            if(!createWebRtcPeer(c)){c->close(QWebSocketProtocol::CloseCodeGoingAway,"WebRTC initialization failed");c->deleteLater();continue;}
            connect(c,&QWebSocket::textMessageReceived,this,[this,c](const QString&m){handleWebRtcSignal(c,m);});
        }
#endif
        else if(path=="/events"||path=="/")eventClients_<<c;
        else {c->close(QWebSocketProtocol::CloseCodePolicyViolated,"Unknown OpenAstroLink WebSocket path");c->deleteLater();continue;}
        connect(c,&QWebSocket::disconnected,this,&OalWsServer::disconnected);
    }
    hasVideoClients_.store(!videoClients_.isEmpty(),std::memory_order_relaxed);
}
void OalWsServer::disconnected(){
    auto*c=qobject_cast<QWebSocket*>(sender());eventClients_.removeAll(c);videoClients_.removeAll(c);
#ifdef OAS_HAVE_WEBRTC
    if(c&&webrtcPeers_.contains(c))destroyWebRtcPeer(c);
#endif
    hasVideoClients_.store(!videoClients_.isEmpty(),std::memory_order_relaxed);if(c)c->deleteLater();
}
void OalWsServer::tick(){broadcast("state",controller_->stateJson());}
void OalWsServer::broadcast(const QString&t,const QJsonObject&p){QJsonObject root{{"type",t},{"timestampUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"payload",p}};const QString text=QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));for(auto*c:eventClients_)if(c&&c->isValid())c->sendTextMessage(text);}
void OalWsServer::offerVideoFrame(const QString&role,const QImage&image,const QJsonObject&stats,int jpegQuality){
    if(image.isNull()||!hasVideoClients())return;bool schedule=false;{
        QMutexLocker l(&videoMutex_);auto&p=pendingVideo_[role];p.image=image;p.stats=stats;p.quality=std::clamp(jpegQuality,40,95);++p.sequence;if(!p.scheduled){p.scheduled=true;schedule=true;}
    }
    if(schedule)QMetaObject::invokeMethod(this,[this,role](){flushVideo(role);},Qt::QueuedConnection);
}
QByteArray OalWsServer::videoPacket(const QString&role,const QByteArray&jpeg,const QJsonObject&stats){
    QJsonObject h=stats;h["codec"]="jpeg";h["role"]=role;const QByteArray header=QJsonDocument(h).toJson(QJsonDocument::Compact);
    QByteArray out;out.reserve(10+header.size()+jpeg.size());out.append("OALV",4);out.append(char(1));out.append(char(role=="guide"?1:0));char len[4];qToBigEndian<quint32>(quint32(header.size()),reinterpret_cast<uchar*>(len));out.append(len,4);out.append(header);out.append(jpeg);return out;
}
void OalWsServer::flushVideo(const QString&role){
    QImage image;QJsonObject stats;int quality=82;quint64 seq=0;{
        QMutexLocker l(&videoMutex_);auto it=pendingVideo_.find(role);if(it==pendingVideo_.end())return;image=it->image;stats=it->stats;quality=it->quality;seq=it->sequence;
    }
    // JPEG compression is intentionally outside the WebSocket/WebRTC/event-loop
    // thread. At 60+ FPS a full-frame encode must never stall REST/events or SER.
    QPointer<OalWsServer> self(this);QThreadPool::globalInstance()->start([self,role,image=std::move(image),stats=std::move(stats),quality,seq]() mutable {
        QByteArray jpeg;QBuffer buffer(&jpeg);buffer.open(QIODevice::WriteOnly);if(!image.save(&buffer,"JPEG",quality))jpeg.clear();if(!self)return;const QByteArray packet=videoPacket(role,jpeg,stats);QMetaObject::invokeMethod(self.data(),[self,role,packet,seq](){if(self)self->sendEncodedVideo(role,packet,seq);},Qt::QueuedConnection);
    });
}
void OalWsServer::sendEncodedVideo(const QString&role,const QByteArray&packet,quint64 seq){
    if(!packet.isEmpty()){
        for(auto*c:videoClients_){if(!c||!c->isValid())continue;if(c->bytesToWrite()>4*1024*1024)continue;c->sendBinaryMessage(packet);}
#ifdef OAS_HAVE_WEBRTC
        if(packet.size()<=int(kWebRtcMaxFrameBytes)){
            const quint32 total=quint32(packet.size());const int fragments=(packet.size()+kWebRtcFragmentPayload-1)/kWebRtcFragmentPayload;
            if(fragments>0&&fragments<=std::numeric_limits<quint16>::max()){
                for(auto*peer:webrtcPeers_){
                    if(!peer)continue;const bool guide=role=="guide";const int dc=guide?peer->guideDc:peer->mainDc;const bool open=guide?peer->guideOpen.load(std::memory_order_relaxed):peer->mainOpen.load(std::memory_order_relaxed);if(dc<0||!open||!rtcIsOpen(dc))continue;
                    const int buffered=rtcGetBufferedAmount(dc);if(buffered<0||buffered>kWebRtcBufferedLimit)continue;
                    bool ok=true;for(int i=0;i<fragments;++i){const int offset=i*kWebRtcFragmentPayload;const int remaining=int(packet.size())-offset;const int n=std::min(kWebRtcFragmentPayload,remaining);const QByteArray fragment=webRtcFragment(role,seq,total,quint16(i),quint16(fragments),packet.constData()+offset,n);if(rtcSendMessage(dc,fragment.constData(),int(fragment.size()))<0){ok=false;break;}}
                    Q_UNUSED(ok);
                }
            }
        }
#endif
    }
    bool again=false;{
        QMutexLocker l(&videoMutex_);auto it=pendingVideo_.find(role);if(it==pendingVideo_.end())return;if(it->sequence==seq)it->scheduled=false;else again=true;
    }
    if(again)QMetaObject::invokeMethod(this,[this,role](){flushVideo(role);},Qt::QueuedConnection);
}

#ifdef OAS_HAVE_WEBRTC
QByteArray OalWsServer::webRtcFragment(const QString&role,quint64 sequence,quint32 totalLength,quint16 index,quint16 count,const char*payload,int payloadSize){
    QByteArray out(24+payloadSize,Qt::Uninitialized);char*d=out.data();memcpy(d,"OALW",4);d[4]=char(1);d[5]=char(role=="guide"?1:0);d[6]=char((index==0?1:0)|(index+1==count?2:0));d[7]=0;qToBigEndian<quint64>(sequence,reinterpret_cast<uchar*>(d+8));qToBigEndian<quint32>(totalLength,reinterpret_cast<uchar*>(d+16));qToBigEndian<quint16>(index,reinterpret_cast<uchar*>(d+20));qToBigEndian<quint16>(count,reinterpret_cast<uchar*>(d+22));if(payloadSize>0)memcpy(d+24,payload,size_t(payloadSize));return out;
}
OalWsServer::WebRtcPeer *OalWsServer::createWebRtcPeer(QWebSocket*signalSocket){
    auto*peer=new WebRtcPeer;peer->owner=this;peer->signal=signalSocket;peer->openCounter=&webrtcOpenChannels_;
    QList<QByteArray> iceStorage;std::vector<const char*> icePointers;const QString env=qEnvironmentVariable("OAL_WEBRTC_ICE_SERVERS").trimmed();if(!env.isEmpty()){const auto urls=env.split(QRegularExpression("[;,\\n]"),Qt::SkipEmptyParts);for(const auto&u:urls){iceStorage.push_back(u.trimmed().toUtf8());}for(const auto&u:iceStorage)icePointers.push_back(u.constData());}
    rtcConfiguration cfg{};cfg.iceServers=icePointers.empty()?nullptr:icePointers.data();cfg.iceServersCount=int(icePointers.size());cfg.disableAutoNegotiation=true;cfg.maxMessageSize=256*1024;
    peer->pc=rtcCreatePeerConnection(&cfg);if(peer->pc<0){delete peer;return nullptr;}rtcSetUserPointer(peer->pc,peer);rtcSetLocalDescriptionCallback(peer->pc,&OalWsServer::rtcDescriptionCallback);rtcSetLocalCandidateCallback(peer->pc,&OalWsServer::rtcCandidateCallback);
    rtcDataChannelInit init{};init.reliability.unordered=true;init.reliability.unreliable=true;init.reliability.maxPacketLifeTime=150;init.protocol="oalw-v1";
    peer->mainDc=rtcCreateDataChannelEx(peer->pc,"oalv-main",&init);peer->guideDc=rtcCreateDataChannelEx(peer->pc,"oalv-guide",&init);if(peer->mainDc<0||peer->guideDc<0){if(peer->mainDc>=0)rtcDeleteDataChannel(peer->mainDc);if(peer->guideDc>=0)rtcDeleteDataChannel(peer->guideDc);rtcDeletePeerConnection(peer->pc);delete peer;return nullptr;}
    for(int dc:{peer->mainDc,peer->guideDc}){rtcSetUserPointer(dc,peer);rtcSetOpenCallback(dc,&OalWsServer::rtcOpenCallback);rtcSetClosedCallback(dc,&OalWsServer::rtcClosedCallback);rtcSetErrorCallback(dc,&OalWsServer::rtcErrorCallback);}
    webrtcPeers_.insert(signalSocket,peer);if(rtcSetLocalDescription(peer->pc,"offer")<0){destroyWebRtcPeer(signalSocket);return nullptr;}return peer;
}
void OalWsServer::destroyWebRtcPeer(QWebSocket*signalSocket){
    auto*peer=webrtcPeers_.take(signalSocket);if(!peer)return;auto clearOpen=[peer](std::atomic_bool&flag){if(flag.exchange(false,std::memory_order_relaxed)&&peer->openCounter)peer->openCounter->fetch_sub(1,std::memory_order_relaxed);};clearOpen(peer->mainOpen);clearOpen(peer->guideOpen);if(peer->mainDc>=0){rtcSetUserPointer(peer->mainDc,nullptr);rtcDeleteDataChannel(peer->mainDc);peer->mainDc=-1;}if(peer->guideDc>=0){rtcSetUserPointer(peer->guideDc,nullptr);rtcDeleteDataChannel(peer->guideDc);peer->guideDc=-1;}if(peer->pc>=0){rtcSetUserPointer(peer->pc,nullptr);rtcDeletePeerConnection(peer->pc);peer->pc=-1;}delete peer;
}
void OalWsServer::handleWebRtcSignal(QWebSocket*signalSocket,const QString&message){
    auto*peer=webrtcPeers_.value(signalSocket,nullptr);if(!peer)return;QJsonParseError pe;const auto doc=QJsonDocument::fromJson(message.toUtf8(),&pe);if(pe.error!=QJsonParseError::NoError||!doc.isObject())return;const auto o=doc.object();const QString type=o.value("type").toString();
    if(type=="description"){
        const QByteArray sdp=o.value("sdp").toString().toUtf8();const QByteArray descriptionType=o.value("descriptionType").toString("answer").toUtf8();if(sdp.isEmpty())return;if(rtcSetRemoteDescription(peer->pc,sdp.constData(),descriptionType.constData())<0){sendWebRtcSignal(peer->signal,{{"type","error"},{"message","Could not apply remote WebRTC description"}});return;}peer->remoteDescriptionSet=true;const auto queued=peer->pendingCandidates;peer->pendingCandidates.clear();for(const auto&c:queued){const auto cand=c.first.toUtf8(),mid=c.second.toUtf8();rtcAddRemoteCandidate(peer->pc,cand.constData(),mid.isEmpty()?nullptr:mid.constData());}
    }else if(type=="candidate"){
        const QString cand=o.value("candidate").toString(),mid=o.value("mid").toString();if(cand.isEmpty())return;if(!peer->remoteDescriptionSet){peer->pendingCandidates.push_back({cand,mid});return;}const auto cu=cand.toUtf8(),mu=mid.toUtf8();rtcAddRemoteCandidate(peer->pc,cu.constData(),mu.isEmpty()?nullptr:mu.constData());
    }
}
void OalWsServer::sendWebRtcSignal(const QPointer<QWebSocket>&signalSocket,const QJsonObject&message){if(signalSocket&&signalSocket->isValid())signalSocket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));}
void OalWsServer::rtcDescriptionCallback(int,const char*sdp,const char*type,void*ptr){auto*peer=static_cast<WebRtcPeer*>(ptr);if(!peer||!peer->owner)return;const QPointer<OalWsServer> owner=peer->owner;const QPointer<QWebSocket> signal=peer->signal;const QString qsdp=QString::fromUtf8(sdp?sdp:"");const QString qtype=QString::fromUtf8(type?type:"");QMetaObject::invokeMethod(owner.data(),[owner,signal,qsdp,qtype](){if(owner)owner->sendWebRtcSignal(signal,{{"type","description"},{"descriptionType",qtype},{"sdp",qsdp}});},Qt::QueuedConnection);}
void OalWsServer::rtcCandidateCallback(int,const char*candidate,const char*mid,void*ptr){auto*peer=static_cast<WebRtcPeer*>(ptr);if(!peer||!peer->owner)return;const QPointer<OalWsServer> owner=peer->owner;const QPointer<QWebSocket> signal=peer->signal;const QString qc=QString::fromUtf8(candidate?candidate:"");const QString qm=QString::fromUtf8(mid?mid:"");QMetaObject::invokeMethod(owner.data(),[owner,signal,qc,qm](){if(owner)owner->sendWebRtcSignal(signal,{{"type","candidate"},{"candidate",qc},{"mid",qm}});},Qt::QueuedConnection);}
void OalWsServer::rtcOpenCallback(int dc,void*ptr){auto*peer=static_cast<WebRtcPeer*>(ptr);if(!peer)return;std::atomic_bool*flag=dc==peer->guideDc?&peer->guideOpen:&peer->mainOpen;if(!flag->exchange(true,std::memory_order_relaxed)&&peer->openCounter)peer->openCounter->fetch_add(1,std::memory_order_relaxed);}
void OalWsServer::rtcClosedCallback(int dc,void*ptr){auto*peer=static_cast<WebRtcPeer*>(ptr);if(!peer)return;std::atomic_bool*flag=dc==peer->guideDc?&peer->guideOpen:&peer->mainOpen;if(flag->exchange(false,std::memory_order_relaxed)&&peer->openCounter)peer->openCounter->fetch_sub(1,std::memory_order_relaxed);}
void OalWsServer::rtcErrorCallback(int dc,const char*error,void*ptr){auto*peer=static_cast<WebRtcPeer*>(ptr);if(!peer||!peer->owner)return;const QPointer<OalWsServer> owner=peer->owner;const QPointer<QWebSocket> signal=peer->signal;const QString message=QString("WebRTC DataChannel %1 error: %2").arg(dc).arg(QString::fromUtf8(error?error:"unknown"));QMetaObject::invokeMethod(owner.data(),[owner,signal,message](){if(owner)owner->sendWebRtcSignal(signal,{{"type","error"},{"message",message}});},Qt::QueuedConnection);}
#endif
}
