#include "oal/oal_ws_server.h"
#include "core/application_controller.h"
#include <QBuffer>
#include <QDateTime>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QPointer>
#include <QtConcurrent>
#include <QtEndian>
#include <algorithm>

namespace oas {
OalWsServer::OalWsServer(ApplicationController*c,QObject*p):QObject(p),controller_(c),server_("OpenAstroLink events/video",QWebSocketServer::NonSecureMode,this),timer_(this){connect(&server_,&QWebSocketServer::newConnection,this,&OalWsServer::newConnection);connect(&timer_,&QTimer::timeout,this,&OalWsServer::tick);timer_.setInterval(1000);}
bool OalWsServer::start(quint16 p,QString*e){stop();if(!server_.listen(QHostAddress::Any,p)){if(e)*e=server_.errorString();return false;}timer_.start();return true;}
void OalWsServer::stop(){timer_.stop();auto close=[](QList<QWebSocket*>&xs){for(auto*c:xs){if(!c)continue;c->disconnect();c->close();delete c;}xs.clear();};close(eventClients_);close(videoClients_);hasVideoClients_.store(false,std::memory_order_relaxed);{QMutexLocker l(&videoMutex_);pendingVideo_.clear();}server_.close();}
void OalWsServer::newConnection(){while(server_.hasPendingConnections()){auto*c=server_.nextPendingConnection();const QString path=c->requestUrl().path();if(path=="/video")videoClients_<<c;else eventClients_<<c;connect(c,&QWebSocket::disconnected,this,&OalWsServer::disconnected);}hasVideoClients_.store(!videoClients_.isEmpty(),std::memory_order_relaxed);}
void OalWsServer::disconnected(){auto*c=qobject_cast<QWebSocket*>(sender());eventClients_.removeAll(c);videoClients_.removeAll(c);hasVideoClients_.store(!videoClients_.isEmpty(),std::memory_order_relaxed);if(c)c->deleteLater();}
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
    // JPEG compression is intentionally outside the WebSocket/event-loop
    // thread. At 60+ FPS a full-frame encode must never stall REST/events.
    QPointer<OalWsServer> self(this);QtConcurrent::run([self,role,image=std::move(image),stats=std::move(stats),quality,seq]() mutable {
        QByteArray jpeg;QBuffer buffer(&jpeg);buffer.open(QIODevice::WriteOnly);if(!image.save(&buffer,"JPEG",quality))jpeg.clear();if(!self)return;const QByteArray packet=videoPacket(role,jpeg,stats);QMetaObject::invokeMethod(self.data(),[self,role,packet,seq](){if(self)self->sendEncodedVideo(role,packet,seq);},Qt::QueuedConnection);
    });
}
void OalWsServer::sendEncodedVideo(const QString&role,const QByteArray&packet,quint64 seq){
    if(!packet.isEmpty())for(auto*c:videoClients_){if(!c||!c->isValid())continue;if(c->bytesToWrite()>4*1024*1024)continue;c->sendBinaryMessage(packet);}
    bool again=false;{
        QMutexLocker l(&videoMutex_);auto it=pendingVideo_.find(role);if(it==pendingVideo_.end())return;if(it->sequence==seq)it->scheduled=false;else again=true;
    }
    if(again)QMetaObject::invokeMethod(this,[this,role](){flushVideo(role);},Qt::QueuedConnection);
}
}
