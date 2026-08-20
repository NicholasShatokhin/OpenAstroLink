#include "algorithms/astap_solver.h"
#ifdef OAS_HAVE_INDI
#include "backends/indi_devices.h"
#endif
#ifdef OAS_HAVE_QHY
#include "backends/qhy_camera.h"
#endif

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>

using namespace oas;

static QString classifyIndi(const QStringList &p) {
    QStringList kinds;
    if (p.contains("EQUATORIAL_EOD_COORD") || p.contains("HORIZONTAL_COORD")) kinds << "mount";
    if (p.contains("ABS_FOCUS_POSITION") || p.contains("REL_FOCUS_POSITION")) kinds << "focuser";
    if (p.contains("CCD_EXPOSURE") || p.contains("CCD1")) kinds << "camera";
    return kinds.isEmpty() ? "other" : kinds.join(",");
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("oal-hardware-probe");
    QCommandLineParser parser;
    parser.setApplicationDescription("Probe the hardware/software path used by OpenAstroLink on a telescope node");
    parser.addHelpOption();
    parser.addOption({"indi-host", "INDI server host", "host", "127.0.0.1"});
    parser.addOption({"indi-port", "INDI server port", "port", "7624"});
    parser.addOption({"no-indi", "Skip INDI discovery"});
    parser.addOption({"no-qhy", "Skip QHY SDK scan"});
    parser.addOption({"no-astap", "Skip ASTAP probe"});
    parser.process(app);

    QTextStream out(stdout), err(stderr);
    bool allOk = true;

    if (!parser.isSet("no-astap")) {
        AstapSolver astap;
        QString why;
        if (astap.available(&why))
            out << "ASTAP: OK  " << astap.executable() << "\n";
        else {
            out << "ASTAP: NOT READY  " << why << "\n";
            allOk = false;
        }
    }

    if (!parser.isSet("no-indi")) {
#ifdef OAS_HAVE_INDI
        bool portOk = false;
        const uint p = parser.value("indi-port").toUInt(&portOk);
        if (!portOk || p == 0 || p > 65535) {
            err << "Invalid --indi-port\n";
            return 2;
        }
        QString e;
        const auto devices = discoverIndiDevices(parser.value("indi-host"), quint16(p), 3000, &e);
        if (devices.isEmpty()) {
            out << "INDI: NOT READY  " << e << "\n";
            allOk = false;
        } else {
            out << "INDI: OK  " << devices.size() << " device(s)\n";
            for (const auto &d : devices) {
                out << "  - " << d.name << "  [" << classifyIndi(d.properties) << "]\n";
                out << "    endpoint: " << parser.value("indi-host") << ':' << p << '/' << d.name << "\n";
                out << "    properties: " << d.properties.join(", ") << "\n";
            }
        }
#else
        out << "INDI: DISABLED IN THIS BUILD (configure OAS_ENABLE_INDI=ON)\n";
        allOk = false;
#endif
    }

    if (!parser.isSet("no-qhy")) {
#ifdef OAS_HAVE_QHY
        QString e;
        const auto ids = QhyCamera::scanCameraIds(&e);
        if (ids.isEmpty()) {
            out << "QHY: NOT READY  " << (e.isEmpty() ? "no cameras found" : e) << "\n";
            allOk = false;
        } else {
            out << "QHY: OK  " << ids.size() << " camera(s)\n";
            for (int i = 0; i < ids.size(); ++i)
                out << "  - index " << i << ": " << ids[i] << " (endpoint may be '" << i << "' or exact ID)\n";
        }
#else
        out << "QHY: DISABLED IN THIS BUILD (configure OAS_ENABLE_QHY=ON)\n";
        allOk = false;
#endif
    }

    return allOk ? 0 : 1;
}
