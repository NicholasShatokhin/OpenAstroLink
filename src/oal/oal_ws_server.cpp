#include "oal/oal_ws_server.h"
#include "core/application_controller.h"
#include <QJsonDocument>
#include <QWebSocket>
#include <QDateTime>

namespace oas {
OalWsServer::OalWsServer(ApplicationController*c,QObject*p):QObject(p),controller_(c),server_("OpenAstroLink events",QWebSocketServer::NonSecureMode,this),timer_(this){connect(&server_,&QWebSocketServer::newConnection,this,&OalWsServer::newConnection);connect(&timer_,&QTimer::timeout,this,&OalWsServer::tick);timer_.setInterval(1000);}
bool OalWsServer::start(quint16 p,QString*e){stop();if(!server_.listen(QHostAddress::Any,p)){if(e)*e=server_.errorString();return false;}timer_.start();return true;}void OalWsServer::stop(){timer_.stop();for(auto*c:clients_){c->close();c->deleteLater();}clients_.clear();server_.close();}
void OalWsServer::newConnection(){while(server_.hasPendingConnections()){auto*c=server_.nextPendingConnection();clients_<<c;connect(c,&QWebSocket::disconnected,this,&OalWsServer::disconnected);}}
void OalWsServer::disconnected(){auto*c=qobject_cast<QWebSocket*>(sender());clients_.removeAll(c);c->deleteLater();}
void OalWsServer::tick(){broadcast("state",controller_->stateJson());}
void OalWsServer::broadcast(const QString&t,const QJsonObject&p){QJsonObject root{{"type",t},{"timestampUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"payload",p}};QString text=QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));for(auto*c:clients_)if(c->isValid())c->sendTextMessage(text);}
}
