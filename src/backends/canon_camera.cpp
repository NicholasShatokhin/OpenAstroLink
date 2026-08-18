#ifdef OAS_HAVE_GPHOTO2
#include "backends/canon_camera.h"
#include <QDateTime>
#include <opencv2/imgcodecs.hpp>

namespace oas {
bool CanonGPhotoCamera::connectDevice(QString*e){context_=gp_context_new();if(gp_camera_new(&camera_)<GP_OK||gp_camera_init(camera_,context_)<GP_OK){if(e)*e="libgphoto2 could not initialize the camera";disconnectDevice();return false;}state_=ConnectionState::Connected;return true;}
void CanonGPhotoCamera::disconnectDevice(){if(camera_){gp_camera_exit(camera_,context_);gp_camera_free(camera_);camera_=nullptr;}if(context_){gp_context_unref(context_);context_=nullptr;}state_=ConnectionState::Disconnected;}
bool CanonGPhotoCamera::capture(const ExposureRequest&r,CameraFrame&f,QString*e){Q_UNUSED(r);if(!camera_){if(e)*e="Canon camera disconnected";return false;}CameraFilePath path{};int rc=gp_camera_capture(camera_,GP_CAPTURE_IMAGE,&path,context_);if(rc<GP_OK){if(e)*e=QString("gp_camera_capture failed: %1").arg(rc);return false;}CameraFile *file=nullptr;gp_file_new(&file);rc=gp_camera_file_get(camera_,path.folder,path.name,GP_FILE_TYPE_NORMAL,file,context_);if(rc<GP_OK){if(e)*e=QString("gp_camera_file_get failed: %1").arg(rc);gp_file_free(file);return false;}const char *ptr=nullptr;unsigned long size=0;gp_file_get_data_and_size(file,&ptr,&size);std::vector<uchar> bytes(ptr,ptr+size);f.image=cv::imdecode(bytes,cv::IMREAD_UNCHANGED);gp_file_free(file);gp_camera_file_delete(camera_,path.folder,path.name,context_);if(f.image.empty()){if(e)*e="OpenCV cannot decode DSLR file";return false;}sensorSize_={f.image.cols,f.image.rows};f.id="canon-"+QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmsszzz");f.capturedUtc=QDateTime::currentDateTimeUtc();f.source=id();return true;}
}
#endif
