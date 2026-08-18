#pragma once
#ifdef OAS_HAVE_GPHOTO2
#include "core/interfaces.h"
#include <gphoto2/gphoto2-camera.h>

namespace oas {
class CanonGPhotoCamera final : public ICamera {
public:
    CanonGPhotoCamera()=default;~CanonGPhotoCamera() override{disconnectDevice();}
    QString id() const override{return "canon-gphoto2";}QString displayName() const override{return "Canon DSLR via libgphoto2";}QString backendName() const override{return "libgphoto2";}ConnectionState connectionState() const override{return state_;}
    bool connectDevice(QString *error=nullptr) override;void disconnectDevice() override;bool capture(const ExposureRequest&,CameraFrame&,QString *error=nullptr) override;QSize sensorSize() const override{return sensorSize_;}
private:Camera *camera_{nullptr};GPContext *context_{nullptr};ConnectionState state_{ConnectionState::Disconnected};QSize sensorSize_;
};
}
#endif
