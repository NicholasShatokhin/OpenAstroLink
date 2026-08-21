#include "core/application_controller.h"
#include "core/settings.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    QCoreApplication::setApplicationName("openastrolink-node");
    QCoreApplication::setApplicationVersion(OAS_VERSION);
    QCoreApplication::setOrganizationName("OpenAstroLink");

    QCommandLineParser parser;
    parser.setApplicationDescription("Headless OpenAstroLink observatory node");
    parser.addHelpOption();parser.addVersionOption();
    QCommandLineOption httpOpt({"p","http-port"},"HTTP control port.","port");
    QCommandLineOption wsOpt({"w","ws-port"},"WebSocket event port.","port");
    QCommandLineOption noWs("no-websocket","Disable the WebSocket event stream.");
    QCommandLineOption noAuto("no-autoconnect","Do not restore persisted main-camera/guide-camera/mount/focuser bindings.");
    QCommandLineOption stellariumOpt("stellarium-port","Enable Stellarium Telescope Control bridge on TCP port.","port");
    QCommandLineOption noStellarium("no-stellarium","Do not start the persisted Stellarium bridge.");
    parser.addOption(httpOpt);parser.addOption(wsOpt);parser.addOption(noWs);parser.addOption(noAuto);parser.addOption(stellariumOpt);parser.addOption(noStellarium);parser.process(app);

    oas::AppSettings settings;
    quint16 httpPort=parser.isSet(httpOpt)?parser.value(httpOpt).toUShort():settings.oalPort();
    quint16 wsPort=parser.isSet(wsOpt)?parser.value(wsOpt).toUShort():settings.wsPort();
    bool wsEnabled=!parser.isSet(noWs);

    oas::ApplicationController controller;
    QObject::connect(&controller,&oas::ObservatoryController::logMessage,[](const QString&m){qInfo().noquote()<<m;});

    QTimer reconnectTimer;
    reconnectTimer.setInterval(10000);
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
    return app.exec();
}
