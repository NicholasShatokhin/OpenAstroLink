#pragma once
#ifdef OAS_HAVE_QHY
#include "core/interfaces.h"
#include <qhyccd.h>

namespace oas {
class QhyCamera final : public ICamera {
public:
    explicit QhyCamera(int index=0):index_(index){}
    ~QhyCamera() override{disconnectDevice();}
    QString id() const override{return QString("qhy-%1").arg(index_);}QString displayName() const override{return cameraId_.isEmpty()?"QHY camera":cameraId_;}QString backendName() const override{return "qhy-sdk";}ConnectionState connectionState() const override{return state_;}
    bool connectDevice(QString *error=nullptr) override;void disconnectDevice() override;bool capture(const ExposureRequest&,CameraFrame&,QString *error=nullptr) override;QSize sensorSize() const override{return sensorSize_;}
private:int index_{0};QString cameraId_;ConnectionState state_{ConnectionState::Disconnected};qhyccd_handle *handle_{nullptr};QSize sensorSize_;bool resourceInitialized_{false};
};
}
#endif
