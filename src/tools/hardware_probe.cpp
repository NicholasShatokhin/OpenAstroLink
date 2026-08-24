#include "algorithms/astap_solver.h"
#include "oal/driver_plugin_loader.h"
#include "backends/oal_native_devices.h"
#ifdef OAS_HAVE_INDI
#include "backends/indi_devices.h"
#endif

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>
#include <QSerialPortInfo>

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
    parser.addOption({"no-canon", "Do not require a native Canon EOS camera"});
    parser.addOption({"require-native-telescope", "Require native QHY + Gemini EAF + Sky-Watcher device discovery"});
    parser.addOption({"require-native-observatory", "Require native QHY + Canon EOS + Gemini EAF + Sky-Watcher device discovery"});
    parser.addOption({"require-zwo", "Require at least one native ZWO ASI camera and one native ZWO EAF focuser"});
    parser.addOption({"gemini-port", "Probe only this serial port for native Gemini EAF (for example COM5 or /dev/ttyUSB0)", "port"});
    parser.addOption({"skywatcher-port", "Probe only this serial port for native Sky-Watcher", "port"});
    parser.addOption({"no-astap", "Skip ASTAP probe"});
    parser.process(app);

    QTextStream out(stdout), err(stderr);
    bool allOk = true;
    if (parser.isSet("gemini-port")) qputenv("OAL_GEMINI_PORT", parser.value("gemini-port").toUtf8());
    if (parser.isSet("skywatcher-port")) qputenv("OAL_SKYWATCHER_PORT", parser.value("skywatcher-port").toUtf8());

    if (!parser.isSet("no-astap")) {
        AstapSolver astap;
        QString why;
        if (astap.available(&why)) out << "ASTAP: OK  " << astap.executable() << "\n";
        else { out << "ASTAP: NOT READY  " << why << "\n"; allOk = false; }
    }

    if (!parser.isSet("no-native")) {
        OalDriverPluginLoader loader;
        QObject::connect(&loader, &OalDriverPluginLoader::driverLog, &loader,
                         [&out](const QString &driver, int level, const QString &message) {
            out << "  [" << driver << "] L" << level << ": " << message << "\n";
            out.flush();
        });
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
        bool qhyFound = false, canonFound = false, geminiFound = false, skywatcherFound = false, zwoAsiFound = false, zwoEafFound = false;
        for (const auto &v : devices) {
            const auto d = v.toObject();
            const QString key = nativeBackendKey(d.value("driverId").toString(), d.value("id").toString());
            out << "  - " << d.value("name").toString() << " [" << d.value("type").toString() << "]\n"
                << "    backend: " << key << "\n";
            const QString driver = d.value("driverId").toString();
            qhyFound |= driver == "oal.qhy";
            canonFound |= driver == "oal.canon";
            geminiFound |= driver == "oal.gemini";
            skywatcherFound |= driver == "oal.skywatcher";
            zwoAsiFound |= driver == "oal.zwo.asi";
            zwoEafFound |= driver == "oal.zwo.eaf";
        }
        for (const auto &e : errors) out << "  warning: " << e << "\n";
        if (!parser.isSet("no-qhy") && !qhyFound) {
            out << "Native QHY: NOT READY  oal.qhy driver loaded no camera (or driver not built)\n";
            allOk = false;
        } else if (!parser.isSet("no-qhy")) out << "Native QHY: OK\n";
        if (!parser.isSet("no-canon") && !canonFound) {
            out << "Native Canon EOS: NOT READY  oal.canon driver loaded no camera (or driver not built)\n";
            allOk = false;
        } else if (!parser.isSet("no-canon")) out << "Native Canon EOS: OK\n";
        out << "Native Gemini EAF: " << (geminiFound ? "OK" : "not discovered") << "\n";
        if (!geminiFound) {
            out << "  Serial ports visible to Qt:\n";
            const auto ports = QSerialPortInfo::availablePorts();
            if (ports.isEmpty()) out << "    (none)\n";
            for (const auto &pi : ports) {
                out << "    " << pi.portName();
                if (!pi.description().isEmpty()) out << "  " << pi.description();
                if (!pi.serialNumber().isEmpty()) out << "  serial=" << pi.serialNumber();
                if (pi.hasVendorIdentifier()) out << "  VID=0x" << QString::number(pi.vendorIdentifier(),16);
                if (pi.hasProductIdentifier()) out << " PID=0x" << QString::number(pi.productIdentifier(),16);
                out << "\n";
            }
            out << "  Hint: rerun with --gemini-port COMx (or set OAL_GEMINI_PORT=COMx) for focused handshake diagnostics.\n";
        }
        out << "Native Sky-Watcher: " << (skywatcherFound ? "OK" : "not discovered") << "\n";
        if (parser.isSet("require-native-telescope") && (!qhyFound || !geminiFound || !skywatcherFound)) {
            out << "Native telescope pack: NOT READY (QHY=" << qhyFound << ", Gemini=" << geminiFound << ", SkyWatcher=" << skywatcherFound << ")\n";
            allOk = false;
        } else if (parser.isSet("require-native-telescope")) out << "Native telescope pack: OK\n";
        if (parser.isSet("require-native-observatory") && (!qhyFound || !canonFound || !geminiFound || !skywatcherFound)) {
            out << "Native observatory pack: NOT READY (QHY=" << qhyFound << ", Canon=" << canonFound << ", Gemini=" << geminiFound << ", SkyWatcher=" << skywatcherFound << ")\n";
            allOk = false;
        } else if (parser.isSet("require-native-observatory")) out << "Native observatory pack: OK\n";
        if (parser.isSet("require-zwo") && (!zwoAsiFound || !zwoEafFound)) {
            out << "Native ZWO pack: NOT READY (ASI=" << zwoAsiFound << ", EAF=" << zwoEafFound << ")\n";
            allOk = false;
        } else if (parser.isSet("require-zwo")) out << "Native ZWO pack: OK\n";
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
