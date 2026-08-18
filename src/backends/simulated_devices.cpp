#include "backends/simulated_devices.h"
#include <QDateTime>
#include <QRandomGenerator>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <atomic>

namespace oas {
namespace {
std::atomic<int> gSimFocusPosition{20000};
constexpr int kBestFocusPosition = 21500;
}
bool SimulatedCamera::connectDevice(QString *) { state_ = ConnectionState::Connected; return true; }
void SimulatedCamera::disconnectDevice() { state_ = ConnectionState::Disconnected; }
bool SimulatedCamera::capture(const ExposureRequest &request, CameraFrame &frame, QString *error) {
    if (state_ != ConnectionState::Connected) { if (error) *error = "Camera is disconnected"; return false; }
    cv::Mat img(960, 1280, CV_16UC1, cv::Scalar(600));
    // Keep the same artificial sky between frames so motion, solve and
    // autofocus tests are repeatable. Blur depends on the shared simulated
    // focuser position and is minimal around kBestFocusPosition.
    cv::RNG rng(0x4f414c31u);
    const double defocus = std::abs(gSimFocusPosition.load() - kBestFocusPosition) / 700.0;
    const double sigma = 1.05 + std::min(7.0, defocus);
    for (int i = 0; i < 45; ++i) {
        const int x = rng.uniform(30, img.cols - 30);
        const int y = rng.uniform(30, img.rows - 30);
        const double amp = rng.uniform(12000.0, 56000.0);
        const int r = static_cast<int>(std::ceil(4 * sigma));
        for (int yy = std::max(0, y-r); yy < std::min(img.rows, y+r+1); ++yy)
            for (int xx = std::max(0, x-r); xx < std::min(img.cols, x+r+1); ++xx) {
                const double d2 = (xx-x)*(xx-x) + (yy-y)*(yy-y);
                auto &p = img.at<std::uint16_t>(yy,xx);
                p = cv::saturate_cast<std::uint16_t>(p + amp * std::exp(-d2/(2*sigma*sigma)));
            }
    }
    cv::Mat noise(img.size(), CV_16SC1);
    cv::randn(noise, 0, 90);
    cv::add(img, noise, img, cv::noArray(), CV_16UC1);
    frame.id = QString("sim-%1-%2").arg(QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmsszzz")).arg(++frameNo_);
    frame.image = img;
    frame.capturedUtc = QDateTime::currentDateTimeUtc();
    frame.exposureSec = request.exposureSec;
    frame.gain = request.gain;
    frame.source = id();
    return true;
}

bool SimulatedMount::connectDevice(QString *) { QMutexLocker l(&mutex_); state_=ConnectionState::Connected; status_.connection=state_; status_.coordinate={83.8221,-5.3911}; status_.tracking=true; return true; }
void SimulatedMount::disconnectDevice() { QMutexLocker l(&mutex_); state_=ConnectionState::Disconnected; status_.connection=state_; }
bool SimulatedMount::status(MountStatus &s, QString *error) { QMutexLocker l(&mutex_); if(state_!=ConnectionState::Connected){if(error)*error="Mount disconnected";return false;} s=status_; return true; }
bool SimulatedMount::slewTo(const EquatorialCoord &t, QString *error){QMutexLocker l(&mutex_);if(state_!=ConnectionState::Connected){if(error)*error="Mount disconnected";return false;}status_.slewing=true;status_.coordinate=t;status_.slewing=false;status_.parked=false;return true;}
bool SimulatedMount::syncTo(const EquatorialCoord &t, QString *error){return slewTo(t,error);}
bool SimulatedMount::setTracking(bool e, QString *error){QMutexLocker l(&mutex_);if(state_!=ConnectionState::Connected){if(error)*error="Mount disconnected";return false;}status_.tracking=e;return true;}
bool SimulatedMount::park(bool e, QString *error){QMutexLocker l(&mutex_);if(state_!=ConnectionState::Connected){if(error)*error="Mount disconnected";return false;}status_.parked=e;status_.tracking=!e;return true;}
bool SimulatedMount::pulseGuide(GuideDirection d,int ms,QString *error){QMutexLocker l(&mutex_);if(state_!=ConnectionState::Connected){if(error)*error="Mount disconnected";return false;}const double arcsec=0.5*ms/1000.0; if(d==GuideDirection::East)status_.coordinate.raDeg+=arcsec/3600.0; if(d==GuideDirection::West)status_.coordinate.raDeg-=arcsec/3600.0; if(d==GuideDirection::North)status_.coordinate.decDeg+=arcsec/3600.0; if(d==GuideDirection::South)status_.coordinate.decDeg-=arcsec/3600.0; return true;}

bool SimulatedFocuser::connectDevice(QString *) { state_=ConnectionState::Connected; return true; }
void SimulatedFocuser::disconnectDevice(){state_=ConnectionState::Disconnected;}
bool SimulatedFocuser::status(FocuserStatus &s, QString *error){if(state_!=ConnectionState::Connected){if(error)*error="Focuser disconnected";return false;}s.connection=state_;s.position=gSimFocusPosition.load();s.moving=false;return true;}
bool SimulatedFocuser::moveAbsolute(int p, QString *error){if(state_!=ConnectionState::Connected){if(error)*error="Focuser disconnected";return false;}gSimFocusPosition.store(std::max(0,p));return true;}
bool SimulatedFocuser::moveRelative(int d, QString *error){return moveAbsolute(gSimFocusPosition.load()+d,error);}
bool SimulatedFocuser::halt(QString *){return state_==ConnectionState::Connected;}
} // namespace oas
