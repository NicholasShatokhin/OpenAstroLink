#pragma once
#ifdef OAS_HAVE_INDI
#include "core/interfaces.h"
#include <QTcpSocket>

namespace oas {
struct IndiEndpoint { QString host{"127.0.0.1"}; quint16 port{7624}; QString device; static IndiEndpoint parse(const QString&); };
class IndiXmlClient {
public:
    explicit IndiXmlClient(IndiEndpoint ep):ep_(std::move(ep)){}
    bool connectServer(QString*);void close();bool setConnection(bool,QString*);QByteArray queryProperty(const QString&,int,QString*);bool sendNumber(const QString&,const QList<QPair<QString,double>>&,QString*);bool sendSwitch(const QString&,const QList<QPair<QString,bool>>&,QString*);
    static bool number(const QByteArray&,const QString&,double&);static bool switchValue(const QByteArray&,const QString&,bool&);
private:bool write(const QByteArray&,QString*);IndiEndpoint ep_;QTcpSocket socket_;
};
class IndiMount final : public IMount {public:explicit IndiMount(QString endpoint):client_(IndiEndpoint::parse(endpoint)){}QString id()const override{return"indi-mount";}QString displayName()const override{return"INDI telescope";}QString backendName()const override{return"indi";}ConnectionState connectionState()const override{return state_;}bool connectDevice(QString*e=nullptr)override;void disconnectDevice()override;bool status(MountStatus&,QString*e=nullptr)override;bool slewTo(const EquatorialCoord&,QString*e=nullptr)override;bool syncTo(const EquatorialCoord&,QString*e=nullptr)override;bool setTracking(bool,QString*e=nullptr)override;bool park(bool,QString*e=nullptr)override;bool pulseGuide(GuideDirection,int,QString*e=nullptr)override;private:IndiXmlClient client_;ConnectionState state_{ConnectionState::Disconnected};};
class IndiFocuser final : public IFocuser {public:explicit IndiFocuser(QString endpoint):client_(IndiEndpoint::parse(endpoint)){}QString id()const override{return"indi-focuser";}QString displayName()const override{return"INDI focuser";}QString backendName()const override{return"indi";}ConnectionState connectionState()const override{return state_;}bool connectDevice(QString*e=nullptr)override;void disconnectDevice()override;bool status(FocuserStatus&,QString*e=nullptr)override;bool moveAbsolute(int,QString*e=nullptr)override;bool moveRelative(int,QString*e=nullptr)override;bool halt(QString*e=nullptr)override;private:IndiXmlClient client_;ConnectionState state_{ConnectionState::Disconnected};};
}
#endif
