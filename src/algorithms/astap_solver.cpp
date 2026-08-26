#include "algorithms/astap_solver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace oas {
namespace {

QString envString(const char *name) {
    return QString::fromLocal8Bit(qgetenv(name)).trimmed();
}

bool parseIniValue(const QString &text, const QString &key, QString &value) {
    const QRegularExpression re(
        QStringLiteral("(?:^|\\n)\\s*%1\\s*=\\s*([^\\r\\n]*?)\\s*(?://.*)?(?:\\r?$|\\n)")
            .arg(QRegularExpression::escape(key)),
        QRegularExpression::MultilineOption);
    const auto m = re.match(text);
    if (!m.hasMatch()) return false;
    value = m.captured(1).trimmed();
    return true;
}

bool parseIniDouble(const QString &text, const QString &key, double &value) {
    QString raw;
    if (!parseIniValue(text, key, raw)) return false;
    bool ok = false;
    const double v = raw.toDouble(&ok);
    if (!ok) return false;
    value = v;
    return true;
}

QString readTextFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}

QString astapIniFor(const QString &outputBase, const QString &imagePath) {
    const QStringList candidates{
        outputBase + ".ini",
        QFileInfo(imagePath).absolutePath() + "/" + QFileInfo(imagePath).completeBaseName() + ".ini"
    };
    for (const auto &p : candidates) {
        if (QFileInfo::exists(p)) return p;
    }
    return candidates.front();
}

cv::Mat solverImage(const cv::Mat &input) {
    if (input.empty()) return {};
    if (input.channels() == 1 && (input.depth() == CV_8U || input.depth() == CV_16U))
        return input;

    cv::Mat mono;
    if (input.channels() == 3) {
        cv::cvtColor(input, mono, cv::COLOR_BGR2GRAY);
    } else if (input.channels() == 4) {
        cv::cvtColor(input, mono, cv::COLOR_BGRA2GRAY);
    } else {
        mono = input;
    }

    if (mono.depth() == CV_8U || mono.depth() == CV_16U) return mono;

    double mn = 0.0, mx = 0.0;
    cv::minMaxLoc(mono, &mn, &mx);
    cv::Mat out;
    if (mx <= mn) mono.convertTo(out, CV_16U);
    else mono.convertTo(out, CV_16U, 65535.0 / (mx - mn), -mn * 65535.0 / (mx - mn));
    return out;
}

} // namespace

AstapSolver::AstapSolver(QString executable, QString databasePath)
    : executable_(std::move(executable)), databasePath_(std::move(databasePath)) {
    if (executable_.trimmed().isEmpty()) executable_ = findExecutable();
    if (databasePath_.trimmed().isEmpty()) databasePath_ = envString("OAL_ASTAP_DATABASE");
}

QString AstapSolver::findExecutable() {
    const QString fromEnv = envString("OAL_ASTAP_EXECUTABLE");
    if (!fromEnv.isEmpty() && QFileInfo::exists(fromEnv)) return QFileInfo(fromEnv).absoluteFilePath();

    for (const QString &name : {QStringLiteral("astap_cli"), QStringLiteral("astap"),
                                QStringLiteral("astap_cli.exe"), QStringLiteral("astap.exe")}) {
        const QString p = QStandardPaths::findExecutable(name);
        if (!p.isEmpty()) return p;
    }

    for (const QString &p : {QStringLiteral("/opt/astap/astap_cli"),
                             QStringLiteral("/opt/astap/astap"),
                             QStringLiteral("/usr/bin/astap_cli"),
                             QStringLiteral("/usr/bin/astap"),
                             QStringLiteral("/usr/local/bin/astap_cli"),
                             QStringLiteral("/usr/local/bin/astap")}) {
        QFileInfo fi(p);
        if (fi.exists() && fi.isExecutable()) return fi.absoluteFilePath();
    }
    return {};
}

bool AstapSolver::available(QString *reason) const {
    if (executable_.isEmpty()) {
        if (reason) *reason = "ASTAP executable was not found. Set OAL_ASTAP_EXECUTABLE or install astap/astap_cli.";
        return false;
    }
    QFileInfo fi(executable_);
    if (!fi.exists() || !fi.isExecutable()) {
        if (reason) *reason = "ASTAP executable is not executable: " + executable_;
        return false;
    }
    return true;
}

SolveResult AstapSolver::solve(const CameraFrame &frame, const TelescopeProfile &profile,
                               const SolveHint &hint) {
    SolveResult result;
    result.catalog = "ASTAP";

    QString availabilityError;
    if (!available(&availabilityError)) {
        result.message = availabilityError;
        return result;
    }
    if (frame.image.empty()) {
        result.message = "ASTAP: input frame is empty";
        return result;
    }

    QTemporaryDir temp(QDir::tempPath() + "/oal-astap-XXXXXX");
    if (!temp.isValid()) {
        result.message = "ASTAP: cannot create temporary directory";
        return result;
    }

    const QString imagePath = temp.filePath("solve.png");
    const cv::Mat image = solverImage(frame.image);
    if (image.empty() || !cv::imwrite(imagePath.toStdString(), image)) {
        result.message = "ASTAP: failed to write temporary PNG";
        return result;
    }

    const QString outputBase = temp.filePath("solution");
    QStringList args{"-f", imagePath, "-o", outputBase, "-z", "0"};

    // A camera-side bin changes the angular size of each returned pixel.
    // Without this correction a 2x2 solver exposure reports roughly half the
    // true FOV, which makes hinted ASTAP searches unnecessarily fragile.
    const double derivedFov = (frame.image.rows > 0 && profile.focalLengthMm > 0.0)
                                  ? frame.image.rows * profile.arcsecPerPixel() * std::max(1, frame.binY) / 3600.0
                                  : 0.0;
    const double fovDeg = hint.fovDeg.value_or(derivedFov);
    if (fovDeg > 0.0) args << "-fov" << QString::number(fovDeg, 'g', 12);

    if (hint.raDeg && hint.decDeg) {
        args << "-ra" << QString::number(*hint.raDeg / 15.0, 'g', 12)
             << "-spd" << QString::number(*hint.decDeg + 90.0, 'g', 12)
             << "-r" << QString::number(qBound(0.1, hint.searchRadiusDeg, 180.0), 'g', 12);
    } else {
        // No sky hint: explicitly allow a blind search.  The supplied/derived
        // FOV keeps the search practical on the Raspberry Pi.
        args << "-r" << "180";
    }

    if (!databasePath_.isEmpty()) args << "-d" << databasePath_;

    QProcess proc;
    proc.setProgram(executable_);
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();
    if (!proc.waitForStarted(5000)) {
        result.message = "ASTAP failed to start: " + proc.errorString();
        return result;
    }

    bool timeoutOk = false;
    const int configuredTimeout = envString("OAL_ASTAP_TIMEOUT_MS").toInt(&timeoutOk);
    const int timeoutMs = timeoutOk && configuredTimeout >= 1000 ? configuredTimeout : 180000;
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(3000);
        result.message = QString("ASTAP timed out after %1 ms").arg(timeoutMs);
        return result;
    }

    const QString stdoutText = QString::fromUtf8(proc.readAll());
    const QString iniPath = astapIniFor(outputBase, imagePath);
    const QString ini = readTextFile(iniPath);
    if (ini.isEmpty()) {
        result.message = QString("ASTAP produced no result INI (exit %1). %2")
                             .arg(proc.exitCode()).arg(stdoutText.trimmed());
        return result;
    }

    QString solvedRaw;
    parseIniValue(ini, "PLTSOLVD", solvedRaw);
    if (solvedRaw.compare("T", Qt::CaseInsensitive) != 0) {
        QString error, warning;
        parseIniValue(ini, "ERROR", error);
        parseIniValue(ini, "WARNING", warning);
        result.message = "ASTAP: no solution";
        if (!error.isEmpty()) result.message += " — " + error;
        if (!warning.isEmpty()) result.message += " — " + warning;
        if (proc.exitCode() != 0) result.message += QString(" (exit %1)").arg(proc.exitCode());
        return result;
    }

    if (!parseIniDouble(ini, "CRVAL1", result.raDeg) ||
        !parseIniDouble(ini, "CRVAL2", result.decDeg)) {
        result.message = "ASTAP solved the frame but CRVAL1/CRVAL2 are missing";
        return result;
    }

    double crota2 = 0.0;
    if (parseIniDouble(ini, "CROTA2", crota2)) result.rotationDeg = crota2;

    double cd11 = 0.0, cd12 = 0.0, cd21 = 0.0, cd22 = 0.0;
    if (parseIniDouble(ini, "CD1_1", cd11) && parseIniDouble(ini, "CD1_2", cd12) &&
        parseIniDouble(ini, "CD2_1", cd21) && parseIniDouble(ini, "CD2_2", cd22)) {
        const double sx = std::hypot(cd11, cd21);
        const double sy = std::hypot(cd12, cd22);
        result.scaleArcsecPerPx = 3600.0 * 0.5 * (sx + sy);
        if (std::abs(result.rotationDeg) < 1e-12)
            result.rotationDeg = std::atan2(cd21, cd11) * 180.0 / std::numbers::pi;
    } else {
        double dx = 0.0, dy = 0.0;
        if (parseIniDouble(ini, "CDELT1", dx) && parseIniDouble(ini, "CDELT2", dy))
            result.scaleArcsecPerPx = 3600.0 * 0.5 * (std::abs(dx) + std::abs(dy));
    }

    QString warning;
    parseIniValue(ini, "WARNING", warning);
    result.success = true;
    result.message = QString("ASTAP solved: RA=%1 deg DEC=%2 deg scale=%3 arcsec/px")
                         .arg(result.raDeg, 0, 'f', 6)
                         .arg(result.decDeg, 0, 'f', 6)
                         .arg(result.scaleArcsecPerPx, 0, 'f', 3);
    if (!warning.isEmpty()) result.message += " — " + warning;
    return result;
}

} // namespace oas
