#include "backends/http_json_client.h"
#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace oas {
QUrl appendPath(const QUrl &base,const QString &part){QUrl u(base);QString p=u.path();if(!p.endsWith('/'))p+='/';p+=part.startsWith('/')?part.mid(1):part;u.setPath(p);return u;}
HttpJsonClient::Reply HttpJsonClient::wait(QNetworkReply *r,int timeout){QEventLoop loop;QTimer timer;timer.setSingleShot(true);QObject::connect(r,&QNetworkReply::finished,&loop,&QEventLoop::quit);QObject::connect(&timer,&QTimer::timeout,&loop,&QEventLoop::quit);timer.start(timeout);loop.exec();Reply out;if(!timer.isActive()){r->abort();out.error="HTTP timeout";}else{timer.stop();out.httpStatus=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();out.bytes=r->readAll();if(r->error()!=QNetworkReply::NoError)out.error=r->errorString();auto doc=QJsonDocument::fromJson(out.bytes);if(doc.isObject())out.json=doc.object();}r->deleteLater();return out;}
HttpJsonClient::Reply HttpJsonClient::get(const QUrl&u,int t){QNetworkRequest req(u);req.setHeader(QNetworkRequest::UserAgentHeader,"OpenAstroSuite/0.2");return wait(manager_.get(req),t);}
HttpJsonClient::Reply HttpJsonClient::postJson(const QUrl&u,const QJsonObject&b,int t){QNetworkRequest req(u);req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");req.setHeader(QNetworkRequest::UserAgentHeader,"OpenAstroSuite/0.2");return wait(manager_.post(req,QJsonDocument(b).toJson(QJsonDocument::Compact)),t);}
HttpJsonClient::Reply HttpJsonClient::putForm(const QUrl&u,const QUrlQuery&f,int t){QNetworkRequest req(u);req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");req.setHeader(QNetworkRequest::UserAgentHeader,"OpenAstroSuite/0.2");return wait(manager_.put(req,f.toString(QUrl::FullyEncoded).toUtf8()),t);}
}
