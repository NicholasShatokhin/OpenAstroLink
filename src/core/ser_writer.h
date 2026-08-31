#pragma once

#include "core/astro_types.h"
#include <QFile>
#include <QVector>

namespace oas {

// Minimal SER v3 writer for uncompressed planetary/lucky-imaging streams.
// Frames are stored before preview debayer/stretch. 16-bit values use the
// de-facto compatible little-endian flag convention used by Siril/FireCapture.
// A FireCapture-style human-readable .txt sidecar is finalized next to each
// SER file with the actual first-frame controls and recording/profile metadata.
class SerWriter {
public:
    SerWriter() = default;
    ~SerWriter();
    bool open(const QString &path,const CameraFrame &first,const TelescopeProfile &profile,
              const LiveViewRequest &request,const QString &cameraBackend={},
              const QString &cameraName={},QString *error=nullptr);
    bool append(const CameraFrame &frame,QString *error=nullptr);
    bool close(QString *error=nullptr);
    QString path() const { return path_; }
    QString sidecarPath() const;
    quint32 frameCount() const { return frameCount_; }
    bool isOpen() const { return file_.isOpen(); }
private:
    bool writeHeader(const CameraFrame &first,const TelescopeProfile &profile,QString *error);
    bool writeSidecar(QString *error) const;
    bool compatible(const CameraFrame &frame,QString *error) const;
    static quint32 colorId(const CameraFrame &frame);
    static quint32 pixelDepth(const CameraFrame &frame);
    static quint64 serTicks(const QDateTime &utc);
    static QByteArray fixedAscii(const QString &text,int bytes);
    QFile file_;
    QString path_;
    int width_{0},height_{0},channels_{0},depthBits_{0};
    quint32 colorId_{0},frameCount_{0};
    QVector<quint64> timestamps_;
    TelescopeProfile profile_{};
    LiveViewRequest request_{};
    QString cameraBackend_,cameraName_,source_,bayerPattern_;
    bool bayerEncoded_{false};
    double actualExposureSec_{0.0};
    int actualGain_{0},actualOffset_{0},actualBinX_{1},actualBinY_{1};
    QDateTime startUtc_,endUtc_;
};

} // namespace oas
