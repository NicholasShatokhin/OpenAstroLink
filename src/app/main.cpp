#include "core/application_controller.h"
#include "core/remote_observatory_controller.h"
#include "gui/main_window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QVBoxLayout>
#include <memory>

namespace {
enum class Mode { LocalNode, RemoteNode, Embedded };
struct Choice { Mode mode{Mode::LocalNode}; QUrl url{"http://127.0.0.1:8080"}; bool accepted{false}; };
QUrl normalizedNodeUrl(QString text){text=text.trimmed();if(!text.contains("://"))text.prepend("http://");return QUrl(text);}
Choice chooseCore(QWidget *parent=nullptr){
    QSettings settings("OpenAstroLink","OpenAstroSuiteGui");
    QDialog d(parent);d.setWindowTitle("OpenAstroSuite — choose observatory core");
    auto*layout=new QVBoxLayout(&d);auto*info=new QLabel("The GUI is a client. For Raspberry Pi production use the local node service; from another computer use Remote node. Embedded core is kept for development/offline use.");info->setWordWrap(true);layout->addWidget(info);
    QString savedRemote=settings.value("control/nodeUrl","http://openastrolink-rpi.local:8080").toString();auto*form=new QFormLayout;auto*mode=new QComboBox;mode->addItem("This computer — local OAL node",int(Mode::LocalNode));mode->addItem("Remote OAL node",int(Mode::RemoteNode));mode->addItem("Embedded core (developer mode)",int(Mode::Embedded));auto*url=new QLineEdit(savedRemote);form->addRow("Core",mode);form->addRow("Node URL",url);layout->addLayout(form);
    auto applyMode=[=](){auto m=Mode(mode->currentData().toInt());if(m==Mode::LocalNode){url->setText("http://127.0.0.1:8080");url->setEnabled(false);}else if(m==Mode::RemoteNode){if(url->text()=="http://127.0.0.1:8080")url->setText(savedRemote);url->setEnabled(true);}else url->setEnabled(false);};
    QObject::connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),[=](int){applyMode();});
    int wanted=settings.value("control/mode",int(Mode::LocalNode)).toInt();int idx=mode->findData(wanted);mode->setCurrentIndex(idx>=0?idx:0);applyMode();
    auto*buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);QObject::connect(buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
    Choice c;if(d.exec()!=QDialog::Accepted)return c;c.accepted=true;c.mode=Mode(mode->currentData().toInt());c.url=normalizedNodeUrl(url->text());settings.setValue("control/mode",int(c.mode));if(c.mode==Mode::RemoteNode)settings.setValue("control/nodeUrl",c.url.toString());return c;
}
}

int main(int argc,char **argv){
    QApplication app(argc,argv);QCoreApplication::setApplicationName("OpenAstroSuite");QCoreApplication::setApplicationVersion(OAS_VERSION);QCoreApplication::setOrganizationName("OpenAstroLink");
    QCommandLineParser parser;parser.addHelpOption();parser.addVersionOption();QCommandLineOption embedded("embedded","Run an in-process core (developer mode).");QCommandLineOption node("node","Connect to an OAL node URL, e.g. http://rpi4:8080.","url");parser.addOption(embedded);parser.addOption(node);parser.process(app);

    std::unique_ptr<oas::ObservatoryController> controller;
    if(parser.isSet(embedded))controller=std::make_unique<oas::ApplicationController>();
    else if(parser.isSet(node)){
        auto r=std::make_unique<oas::RemoteObservatoryController>(normalizedNodeUrl(parser.value(node)));QString e;if(!r->probe(&e)){QMessageBox::critical(nullptr,"OpenAstroSuite","Cannot connect to OAL node:\n"+e);return 2;}controller=std::move(r);
    }else{
        while(!controller){auto c=chooseCore();if(!c.accepted)return 0;if(c.mode==Mode::Embedded){controller=std::make_unique<oas::ApplicationController>();break;}auto r=std::make_unique<oas::RemoteObservatoryController>(c.url);QString e;if(r->probe(&e)){controller=std::move(r);break;}QMessageBox::warning(nullptr,"OpenAstroSuite",QString("Cannot connect to %1:\n%2\n\nStart openastrolink-node on that computer or choose Embedded core.").arg(c.url.toString(),e));}
    }
    oas::MainWindow window(controller.get());window.show();return app.exec();
}
