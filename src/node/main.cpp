#include "core/application_controller.h"
#include "core/settings.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <csignal>
#  include <cstdlib>
#endif

namespace {
#ifdef Q_OS_WIN
volatile LONG gConsoleInterruptCount = 0;
BOOL WINAPI oalConsoleCtrlHandler(DWORD type) {
    if (type != CTRL_C_EVENT && type != CTRL_BREAK_EVENT) return FALSE;
    const LONG count = InterlockedIncrement(&gConsoleInterruptCount);
    // First interrupt requests an orderly Qt-thread shutdown.  A second
    // interrupt falls through to Windows' default handler as an escape hatch.
    return count == 1 ? TRUE : FALSE;
}
bool consoleShutdownRequested() {
    return InterlockedCompareExchange(&gConsoleInterruptCount, 0, 0) > 0;
}
#else
volatile std::sig_atomic_t gConsoleInterruptCount = 0;
void oalPosixSignalHandler(int) {
    if (gConsoleInterruptCount > 0) std::_Exit(130);
    gConsoleInterruptCount = 1;
}
bool consoleShutdownRequested() { return gConsoleInterruptCount > 0; }
#endif
}

int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    QCoreApplication::setApplicationName("openastrolink-node");
    QCoreApplication::setApplicationVersion(OAS_VERSION);
    QCoreApplication::setOrganizationName("OpenAstroLink");

#ifdef Q_OS_WIN
    const bool consoleHandlerInstalled = SetConsoleCtrlHandler(&oalConsoleCtrlHandler, TRUE) != FALSE;
    if(!consoleHandlerInstalled)qWarning()<<"Could not install Windows console Ctrl+C handler; shutdown may be abrupt.";
#else
    std::signal(SIGINT,&oalPosixSignalHandler);
    std::signal(SIGTERM,&oalPosixSignalHandler);
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription("Headless OpenAstroLink observatory node");
    parser.addHelpOption();parser.addVersionOption();
    QCommandLineOption httpOpt({"p","http-port"},"HTTP control port.","port");
    QCommandLineOption wsOpt({"w","ws-port"},"WebSocket event port.","port");
    QCommandLineOption noWs("no-websocket","Disable the WebSocket event stream.");
    QCommandLineOption noAuto("no-autoconnect","Do not restore persisted main-camera/guide-camera/mount/focuser bindings.");
    QCommandLineOption stellariumOpt("stellarium-port","Enable Stellarium Telescope Control bridge on TCP port.","port");
    QCommandLineOption noStellarium("no-stellarium","Do not start the persisted Stellarium bridge.");
    QCommandLineOption geminiPortOpt("gemini-port","Pin native Gemini EAF discovery to one serial port (for example COM4 or /dev/ttyUSB0). Omit to use the saved setting or automatic scan.","port");
    QCommandLineOption skywatcherPortOpt("skywatcher-port","Pin native Sky-Watcher discovery to one serial port.","port");
    parser.addOption(httpOpt);parser.addOption(wsOpt);parser.addOption(noWs);parser.addOption(noAuto);parser.addOption(stellariumOpt);parser.addOption(noStellarium);parser.addOption(geminiPortOpt);parser.addOption(skywatcherPortOpt);parser.process(app);
    if(parser.isSet(geminiPortOpt))qputenv("OAL_GEMINI_PORT",parser.value(geminiPortOpt).toUtf8());
    if(parser.isSet(skywatcherPortOpt))qputenv("OAL_SKYWATCHER_PORT",parser.value(skywatcherPortOpt).toUtf8());

    oas::AppSettings settings;
    quint16 httpPort=parser.isSet(httpOpt)?parser.value(httpOpt).toUShort():settings.oalPort();
    quint16 wsPort=parser.isSet(wsOpt)?parser.value(wsOpt).toUShort():settings.wsPort();
    bool wsEnabled=!parser.isSet(noWs);

    oas::ApplicationController controller;
    QObject::connect(&controller,&oas::ObservatoryController::logMessage,[](const QString&m){qInfo().noquote()<<m;});

    QTimer reconnectTimer;
    reconnectTimer.setInterval(10000);

    QTimer consoleSignalTimer;
    consoleSignalTimer.setInterval(50);
    QObject::connect(&consoleSignalTimer,&QTimer::timeout,&app,[&](){
        if(!consoleShutdownRequested())return;
        consoleSignalTimer.stop();
        reconnectTimer.stop();
        qInfo()<<"Console interrupt received; starting graceful shutdown. Press Ctrl+C again to force termination.";
        QCoreApplication::quit();
    });
    consoleSignalTimer.start();

    QObject::connect(&app,&QCoreApplication::aboutToQuit,&controller,[&](){
        consoleSignalTimer.stop();
        reconnectTimer.stop();
        controller.shutdown();
    });
    if(!parser.isSet(noAuto)){
        auto tryRestore=[&](){
            QStringList errors;bool ok=controller.restoreConfiguredDevices(&errors);
            for(const auto&e:errors)qWarning().noquote()<<e;
            if(ok&&reconnectTimer.isActive())reconnectTimer.stop();
            return ok;
        };
        if(!tryRestore()){
            QObject::connect(&reconnectTimer,&QTimer::timeout,&app,[&](){
                QStringList errors;if(controller.restoreConfiguredDevices(&errors)){reconnectTimer.stop();qInfo()<<"Persisted devices restored.";}
                else for(const auto&e:errors)qWarning().noquote()<<e;
            });
            reconnectTimer.start();
            qWarning()<<"One or more persisted devices are unavailable; retrying every 10 seconds while the node stays remotely configurable.";
        }
    }

    QString error;
    if(!controller.startOalServer(httpPort,wsEnabled,wsPort,&error)){
        qCritical().noquote()<<"Cannot start OpenAstroLink node:" << error;
        return 2;
    }
    qInfo().noquote()<<QString("OpenAstroLink node ready: HTTP 0.0.0.0:%1, WebSocket %2").arg(httpPort).arg(wsEnabled?QString("0.0.0.0:%1").arg(wsPort):"disabled");
    if(!parser.isSet(noStellarium)){
        const bool enableStellarium=parser.isSet(stellariumOpt)||settings.stellariumEnabled();
        if(enableStellarium){
            const quint16 port=parser.isSet(stellariumOpt)?parser.value(stellariumOpt).toUShort():settings.stellariumPort();
            QString stellariumError;if(!controller.startStellariumServer(port,&stellariumError))qWarning().noquote()<<"Stellarium bridge not started:"<<stellariumError;
        }
    }
    qInfo().noquote()<<"All autofocus, polar-alignment, solve, guiding and session operations execute in this process.";
    const int exitCode=app.exec();
#ifdef Q_OS_WIN
    if(consoleHandlerInstalled)SetConsoleCtrlHandler(&oalConsoleCtrlHandler,FALSE);
#endif
    return exitCode;
}
