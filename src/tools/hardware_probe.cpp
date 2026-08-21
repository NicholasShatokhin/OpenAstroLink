#include "algorithms/astap_solver.h"
#include "oal/driver_plugin_loader.h"
#include "backends/oal_native_devices.h"
#ifdef OAS_HAVE_INDI
#include "backends/indi_devices.h"
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
    parser.setApplicationDescription("Probe native OpenAstroLink drivers plus optional compatibility paths");
    parser.addHelpOption();
    parser.addOption({"indi-host", "INDI server host", "host", "127.0.0.1"});
    parser.addOption({"indi-port", "INDI server port", "port", "7624"});
    parser.addOption({"no-indi", "Skip INDI compatibility discovery"});
    parser.addOption({"no-native", "Skip native OpenAstroLink driver discovery"});
    parser.addOption({"no-qhy", "Do not require a native QHY camera"});
    parser.addOption({"require-native-telescope", "Require native QHY + Gemini EAF + Sky-Watcher device discovery"});
    parser.addOption({"no-astap", "Skip ASTAP probe"});
    parser.process(app);

    QTextStream out(stdout), err(stderr);
    bool allOk = true;

    if (!parser.isSet("no-astap")) {
        AstapSolver astap;
        QString why;
        if (astap.available(&why)) out << "ASTAP: OK  " << astap.executable() << "\n";
        else { out << "ASTAP: NOT READY  " << why << "\n"; allOk = false; }
    }

    if (!parser.isSet("no-native")) {
        OalDriverPluginLoader loader;
        QStringList errors;
        loader.scanDefaultPaths(&errors);
        const auto drivers = loader.drivers();
        const auto devices = loader.devices();
        out << "Native OAL: " << drivers.size() << " driver(s), " << devices.size() << " device(s)\n";
        for (const auto &v : drivers) {
            const auto d = v.toObject();
            out << "  driver " << d.value("driverId").toString() << "  ABI " << d.value("abiVersion").toInt()
                << "  " << d.value("version").toString() << "  isolation=" << d.value("isolation").toString("unspecified") << "\n";
        }
        bool qhyFound = false, geminiFound = false, skywatcherFound = false;
        for (const auto &v : devices) {
            const auto d = v.toObject();
            const QString key = nativeBackendKey(d.value("driverId").toString(), d.value("id").toString());
            out << "  - " << d.value("name").toString() << " [" << d.value("type").toString() << "]\n"
                << "    backend: " << key << "\n";
            const QString driver = d.value("driverId").toString();
            qhyFound |= driver == "oal.qhy";
            geminiFound |= driver == "oal.gemini";
            skywatcherFound |= driver == "oal.skywatcher";
        }
        for (const auto &e : errors) out << "  warning: " << e << "\n";
        if (!parser.isSet("no-qhy") && !qhyFound) {
            out << "Native QHY: NOT READY  oal.qhy driver loaded no camera (or driver not built)\n";
            allOk = false;
        } else if (!parser.isSet("no-qhy")) out << "Native QHY: OK\n";
        out << "Native Gemini EAF: " << (geminiFound ? "OK" : "not discovered") << "\n";
        out << "Native Sky-Watcher: " << (skywatcherFound ? "OK" : "not discovered") << "\n";
        if (parser.isSet("require-native-telescope") && (!qhyFound || !geminiFound || !skywatcherFound)) {
            out << "Native telescope pack: NOT READY (QHY=" << qhyFound << ", Gemini=" << geminiFound << ", SkyWatcher=" << skywatcherFound << ")\n";
            allOk = false;
        } else if (parser.isSet("require-native-telescope")) out << "Native telescope pack: OK\n";
    }

    if (!parser.isSet("no-indi")) {
#ifdef OAS_HAVE_INDI
        bool portOk = false;
        const uint p = parser.value("indi-port").toUInt(&portOk);
        if (!portOk || p == 0 || p > 65535) { err << "Invalid --indi-port\n"; return 2; }
        QString e;
        const auto devices = discoverIndiDevices(parser.value("indi-host"), quint16(p), 3000, &e);
        if (devices.isEmpty()) out << "INDI compatibility: no devices  " << e << "\n";
        else {
            out << "INDI compatibility: " << devices.size() << " device(s)\n";
            for (const auto &d : devices) {
                out << "  - " << d.name << "  [" << classifyIndi(d.properties) << "]\n";
                out << "    endpoint: " << parser.value("indi-host") << ':' << p << '/' << d.name << "\n";
            }
        }
#else
        out << "INDI compatibility: disabled in this build\n";
#endif
    }

    return allOk ? 0 : 1;
}
