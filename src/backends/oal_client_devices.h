#pragma once
#include "backends/http_json_client.h"
#include "core/interfaces.h"

namespace oas {
class OalCameraClient final : public ICamera {
public:
    explicit OalCameraClient(QUrl base):base_(std::move(base)){}
    QString id() const override{return "oal-camera-client";}QString displayName() const override{return "Remote OAL camera";}QString backendName() const override{return "oal";}ConnectionState connectionState() const override{return state_;}
    bool connectDevice(QString *error=nullptr) override;void disconnectDevice() override{state_=ConnectionState::Disconnected;}
    bool capture(const ExposureRequest&,CameraFrame&,QString *error=nullptr) override;QSize sensorSize() const override{return size_;}
private:bool accepted(const HttpJsonClient::Reply&,QString*)const;QUrl route(const QString&)const;QUrl base_;ConnectionState state_{ConnectionState::Disconnected};QSize size_;HttpJsonClient http_;
};

class OalMountClient final : public IMount {
public:
    explicit OalMountClient(QUrl base) : base_(std::move(base)) {}
    QString id() const override{return "oal-mount-client";} QString displayName() const override{return "Remote OAL mount";} QString backendName() const override{return "oal";} ConnectionState connectionState() const override{return state_;}
    bool connectDevice(QString *error=nullptr) override;void disconnectDevice() override{state_=ConnectionState::Disconnected;}
    bool status(MountStatus&,QString *error=nullptr) override;bool slewTo(const EquatorialCoord&,QString *error=nullptr) override;bool syncTo(const EquatorialCoord&,QString *error=nullptr) override;bool setTracking(bool,QString *error=nullptr) override;bool park(bool,QString *error=nullptr) override;bool pulseGuide(GuideDirection,int,QString *error=nullptr) override;
private:bool accepted(const HttpJsonClient::Reply&,QString*)const;QUrl route(const QString&)const;QUrl base_;ConnectionState state_{ConnectionState::Disconnected};HttpJsonClient http_;
};
class OalFocuserClient final : public IFocuser {
public:
    explicit OalFocuserClient(QUrl base):base_(std::move(base)){}
    QString id() const override{return "oal-focuser-client";}QString displayName() const override{return "Remote OAL focuser";}QString backendName() const override{return "oal";}ConnectionState connectionState() const override{return state_;}
    bool connectDevice(QString *error=nullptr) override;void disconnectDevice() override{state_=ConnectionState::Disconnected;}bool status(FocuserStatus&,QString *error=nullptr) override;bool moveAbsolute(int,QString *error=nullptr) override;bool moveRelative(int,QString *error=nullptr) override;bool halt(QString *error=nullptr) override;
private:bool accepted(const HttpJsonClient::Reply&,QString*)const;QUrl route(const QString&)const;QUrl base_;ConnectionState state_{ConnectionState::Disconnected};HttpJsonClient http_;
};
}
