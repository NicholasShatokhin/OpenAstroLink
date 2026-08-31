#include "core/ser_writer.h"
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <opencv2/core.hpp>
#include <algorithm>

namespace oas {
namespace {
constexpr qint64 kUnixEpochInSerMs = 62135596800000LL;
QString yn(bool v){return v?"true":"false";}
}

SerWriter::~SerWriter(){ close(nullptr); }

QByteArray SerWriter::fixedAscii(const QString &text,int bytes){
    QByteArray out=text.toUtf8();if(out.size()>bytes)out.truncate(bytes);out.resize(bytes,'\0');return out;
}
quint64 SerWriter::serTicks(const QDateTime &utc){
    const qint64 ms=utc.toUTC().toMSecsSinceEpoch()+kUnixEpochInSerMs;
    return ms>0?quint64(ms)*10000ULL:0ULL;
}
quint32 SerWriter::pixelDepth(const CameraFrame &frame){return frame.image.depth()==CV_16U?16u:8u;}
quint32 SerWriter::colorId(const CameraFrame &frame){
    if(frame.image.channels()==3)return 101u; // OpenCV BGR
    if(!frame.bayerEncoded)return 0u;
    const QString p=frame.bayerPattern.trimmed().toUpper();
    if(p=="RGGB")return 8u;if(p=="GRBG")return 9u;if(p=="GBRG")return 10u;if(p=="BGGR")return 11u;
    return 0u;
}

bool SerWriter::writeHeader(const CameraFrame &first,const TelescopeProfile &profile,QString *error){
    QDataStream d(&file_);d.setByteOrder(QDataStream::LittleEndian);d.setFloatingPointPrecision(QDataStream::DoublePrecision);
    if(d.writeRawData("LUCAM-RECORDER",14)!=14){if(error)*error="Could not write SER FileID";return false;}
    d<<quint32(0)<<colorId_<<quint32(0)<<quint32(width_)<<quint32(height_)<<quint32(depthBits_)<<quint32(0);
    const QByteArray observer=fixedAscii("OpenAstroLink",40),instrument=fixedAscii(first.source.isEmpty()?"OpenAstroLink camera":first.source,40),telescope=fixedAscii(profile.name,40);
    d.writeRawData(observer.constData(),40);d.writeRawData(instrument.constData(),40);d.writeRawData(telescope.constData(),40);
    const quint64 t=serTicks(first.capturedUtc.isValid()?first.capturedUtc:QDateTime::currentDateTimeUtc());d<<t<<t;
    if(file_.pos()!=178){if(error)*error=QString("SER header size mismatch: %1").arg(file_.pos());return false;}
    return true;
}

QString SerWriter::sidecarPath() const{
    if(path_.isEmpty())return{};QFileInfo fi(path_);return QDir(fi.absolutePath()).filePath(fi.completeBaseName()+".txt");
}

bool SerWriter::writeSidecar(QString *error) const{
    const QString p=sidecarPath();if(p.isEmpty())return true;
    QFile f(p);if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text)){if(error)*error="Could not write SER metadata sidecar: "+f.errorString();return false;}
    QTextStream t(&f);
#ifdef OAS_VERSION
    const QString version=QString::fromLatin1(OAS_VERSION);
#else
    const QString version="unknown";
#endif
    const qint64 durationMs=(startUtc_.isValid()&&endUtc_.isValid())?std::max<qint64>(0,startUtc_.msecsTo(endUtc_)):0;
    const double durationSec=durationMs/1000.0;
    const double actualFps=(durationSec>0.0&&frameCount_>1)?double(frameCount_-1)/durationSec:0.0;
    t<<"OpenAstroLink planetary capture metadata\n";
    t<<"Version="<<version<<"\n";
    t<<"SERFile="<<QFileInfo(path_).fileName()<<"\n";
    t<<"Camera="<<(cameraName_.isEmpty()?source_:cameraName_)<<"\n";
    t<<"CameraBackend="<<cameraBackend_<<"\n";
    t<<"CameraSource="<<source_<<"\n";
    t<<"StartUTC="<<startUtc_.toUTC().toString(Qt::ISODateWithMs)<<"\n";
    t<<"EndUTC="<<endUtc_.toUTC().toString(Qt::ISODateWithMs)<<"\n";
    t<<"DurationSec="<<QString::number(durationSec,'f',3)<<"\n";
    t<<"Frames="<<frameCount_<<"\n";
    t<<"TargetFPS="<<QString::number(request_.targetFps,'f',3)<<"\n";
    t<<"MeasuredFPS="<<QString::number(actualFps,'f',3)<<"\n";
    t<<"Width="<<width_<<"\nHeight="<<height_<<"\n";
    t<<"Channels="<<channels_<<"\nBitDepth="<<depthBits_<<"\nSERColorID="<<colorId_<<"\n";
    t<<"BayerEncoded="<<yn(bayerEncoded_)<<"\nBayerPattern="<<(bayerPattern_.isEmpty()?"NONE":bayerPattern_)<<"\n";
    t<<"RawPreDebayer=true\nPreviewDebayerRequested="<<yn(request_.debayer)<<"\nPreviewBayerPattern="<<bayerPatternName(request_.bayerPattern)<<"\n";
    t<<"ExposureRequestedSec="<<QString::number(request_.exposureSec,'g',12)<<"\n";
    t<<"ExposureActualFirstFrameSec="<<QString::number(actualExposureSec_,'g',12)<<"\n";
    t<<"GainRequested="<<request_.gain<<"\nGainActualFirstFrame="<<actualGain_<<"\n";
    t<<"OffsetRequested="<<request_.offset<<"\nOffsetActualFirstFrame="<<actualOffset_<<"\n";
    t<<"BinRequested="<<request_.binX<<"x"<<request_.binY<<"\nBinActual="<<actualBinX_<<"x"<<actualBinY_<<"\n";
    t<<"OpticalProfile="<<profile_.name<<"\nOpticalDesign="<<profile_.opticalDesign<<"\n";
    t<<"ApertureMm="<<QString::number(profile_.apertureMm,'f',3)<<"\n";
    t<<"FocalLengthMm="<<QString::number(profile_.focalLengthMm,'f',3)<<"\n";
    t<<"FocalRatio="<<QString::number(profile_.focalRatio(),'f',3)<<"\n";
    t<<"PixelSizeUm="<<QString::number(profile_.pixelSizeUm,'f',4)<<"\n";
    t<<"ImageScaleArcsecPerPixel="<<QString::number(profile_.arcsecPerPixel(),'f',6)<<"\n";
    t<<"SiteLatitudeDeg="<<QString::number(profile_.observer.latitudeDeg,'f',8)<<"\n";
    t<<"SiteLongitudeDeg="<<QString::number(profile_.observer.longitudeDeg,'f',8)<<"\n";
    t<<"SiteElevationM="<<QString::number(profile_.observer.elevationM,'f',3)<<"\n";
    t<<"TimestampTrailerUTC=true\n";
    f.flush();if(f.error()!=QFileDevice::NoError){if(error)*error="SER metadata sidecar write failed: "+f.errorString();return false;}return true;
}

bool SerWriter::open(const QString &path,const CameraFrame &first,const TelescopeProfile &profile,
                     const LiveViewRequest &request,const QString &cameraBackend,
                     const QString &cameraName,QString *error){
    close(nullptr);if(first.image.empty()){if(error)*error="Cannot create SER from an empty frame";return false;}
    path_=path;QFileInfo fi(path_);if(!QDir().mkpath(fi.absolutePath())){if(error)*error="Could not create SER directory: "+fi.absolutePath();return false;}
    width_=first.image.cols;height_=first.image.rows;channels_=first.image.channels();depthBits_=int(pixelDepth(first));colorId_=colorId(first);frameCount_=0;timestamps_.clear();
    profile_=profile;request_=request;cameraBackend_=cameraBackend;cameraName_=cameraName;source_=first.source;bayerEncoded_=first.bayerEncoded;bayerPattern_=first.bayerEncoded?first.bayerPattern.trimmed().toUpper():QString();
    actualExposureSec_=first.exposureSec;actualGain_=first.gain;actualOffset_=first.offset;actualBinX_=first.binX;actualBinY_=first.binY;startUtc_=first.capturedUtc.isValid()?first.capturedUtc:QDateTime::currentDateTimeUtc();endUtc_=startUtc_;
    if((channels_!=1&&channels_!=3)||(first.image.depth()!=CV_8U&&first.image.depth()!=CV_16U)){if(error)*error="SER supports only 8/16-bit mono/Bayer or BGR frames in this release";return false;}
    file_.setFileName(path_);if(!file_.open(QIODevice::WriteOnly|QIODevice::Truncate)){if(error)*error="Could not open SER file: "+file_.errorString();return false;}
    if(!writeHeader(first,profile,error)){file_.close();return false;}return true;
}

bool SerWriter::compatible(const CameraFrame &frame,QString *error) const{
    if(frame.image.empty()||frame.image.cols!=width_||frame.image.rows!=height_||frame.image.channels()!=channels_||int(pixelDepth(frame))!=depthBits_||colorId(frame)!=colorId_){
        if(error)*error="Live frame geometry/pixel format changed while SER recording; stop and start a new recording";return false;}
    return true;
}

bool SerWriter::append(const CameraFrame &frame,QString *error){
    if(!file_.isOpen()){if(error)*error="SER writer is not open";return false;}if(!compatible(frame,error))return false;
    const int rowBytes=frame.image.cols*int(frame.image.elemSize());
    for(int y=0;y<frame.image.rows;++y){const char *row=reinterpret_cast<const char*>(frame.image.ptr(y));if(file_.write(row,rowBytes)!=rowBytes){if(error)*error="SER frame write failed: "+file_.errorString();return false;}}
    ++frameCount_;const QDateTime ts=frame.capturedUtc.isValid()?frame.capturedUtc:QDateTime::currentDateTimeUtc();timestamps_.push_back(serTicks(ts));endUtc_=ts;return true;
}

bool SerWriter::close(QString *error){
    if(!file_.isOpen())return true;
    QDataStream d(&file_);d.setByteOrder(QDataStream::LittleEndian);for(quint64 t:timestamps_)d<<t;
    if(!file_.seek(38)){if(error)*error="Could not seek to SER frame-count field";file_.close();return false;}d<<quint32(frameCount_);file_.flush();const bool serOk=file_.error()==QFileDevice::NoError;if(!serOk&&error)*error="SER finalize failed: "+file_.errorString();file_.close();
    if(!serOk)return false;QString sidecarError;const bool sidecarOk=writeSidecar(&sidecarError);if(!sidecarOk&&error)*error=sidecarError;return sidecarOk;
}

} // namespace oas
