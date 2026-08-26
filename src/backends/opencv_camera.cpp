#include "backends/opencv_camera.h"
#include <QDateTime>
#include <algorithm>
#include <opencv2/imgproc.hpp>

namespace oas {
bool OpenCvCamera::connectDevice(QString *error) {
    state_=ConnectionState::Connecting;
    if(!capture_.open(index_)){state_=ConnectionState::Error;if(error)*error=QString("Cannot open camera index %1").arg(index_);return false;}
    state_=ConnectionState::Connected; return true;
}
void OpenCvCamera::disconnectDevice(){capture_.release();state_=ConnectionState::Disconnected;}
bool OpenCvCamera::capture(const ExposureRequest &r, CameraFrame &f, QString *error){
    if(state_!=ConnectionState::Connected){if(error)*error="Camera disconnected";return false;}
    capture_.set(cv::CAP_PROP_EXPOSURE,r.exposureSec);
    capture_.set(cv::CAP_PROP_GAIN,r.gain);
    cv::Mat img; if(!capture_.read(img)||img.empty()){if(error)*error="OpenCV read failed";return false;}
    const int binX = std::max(1, r.binX);
    const int binY = std::max(1, r.binY);
    if (binX > 1 || binY > 1) {
        const int outW = std::max(1, img.cols / binX);
        const int outH = std::max(1, img.rows / binY);
        cv::Mat binned;
        cv::resize(img, binned, cv::Size(outW, outH), 0.0, 0.0, cv::INTER_AREA);
        img = std::move(binned);
    }
    f.id=QString("uvc-%1").arg(QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmsszzz"));f.image=img;f.capturedUtc=QDateTime::currentDateTimeUtc();f.exposureSec=r.exposureSec;f.gain=r.gain;f.binX=binX;f.binY=binY;f.source=id();return true;
}
QSize OpenCvCamera::sensorSize() const{return {static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_WIDTH)),static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_HEIGHT))};}
}
