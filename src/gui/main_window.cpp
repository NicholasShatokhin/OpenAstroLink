#include "gui/main_window.h"
#include "core/equatorial_frames.h"
#include "core/mount_geometry.h"
#include "backends/ascom_classic_mount.h"
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollArea>
#include <QStatusBar>
#include <QSplitter>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QTime>
#include <QDateTimeEdit>
#include <QJsonArray>
#include <QTimer>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace oas {
static QDoubleSpinBox *dspin(double lo,double hi,double value,int decimals=4){auto*w=new QDoubleSpinBox;w->setRange(lo,hi);w->setDecimals(decimals);w->setValue(value);return w;}
static QSpinBox *ispin(int lo,int hi,int value){auto*w=new QSpinBox;w->setRange(lo,hi);w->setValue(value);return w;}
static void populateBackendCombo(QComboBox *combo,const QStringList &items){
    combo->clear();auto *model=qobject_cast<QStandardItemModel*>(combo->model());int firstSelectable=-1;bool nativeHeader=false,compatHeader=false;
    auto header=[&](const QString&text,bool &flag){if(flag)return;flag=true;combo->addItem(text);const int row=combo->count()-1;if(model){auto*item=model->item(row);if(item){item->setEnabled(false);item->setSelectable(false);}}};
    for(const auto &item:items){const bool native=item.startsWith("native:");if(native)header("── Detected native devices ──",nativeHeader);else header("── Compatibility / embedded ──",compatHeader);combo->addItem(item);if(firstSelectable<0)firstSelectable=combo->count()-1;}
    if(firstSelectable>=0)combo->setCurrentIndex(firstSelectable);
}
static QStringList backendComboItems(const QComboBox *combo){
    QStringList out;if(!combo)return out;
    for(int i=0;i<combo->count();++i){const QString t=combo->itemText(i);if(!t.startsWith(QStringLiteral("── ")))out<<t;}
    return out;
}
static void refreshBackendComboIfChanged(QComboBox *combo,const QStringList &items){
    if(!combo||backendComboItems(combo)==items)return;
    const QString old=combo->currentText();QSignalBlocker blocker(combo);populateBackendCombo(combo,items);
    const int i=combo->findText(old);if(i>=0)combo->setCurrentIndex(i);
}
static QImage autoStretchPreview(const QImage &input){
    if(input.isNull())return input;
    QImage rgb=input.convertToFormat(QImage::Format_RGB888);
    QImage sample=rgb.convertToFormat(QImage::Format_Grayscale8);
    if(std::max(sample.width(),sample.height())>900)sample=sample.scaled(900,900,Qt::KeepAspectRatio,Qt::FastTransformation);
    std::array<quint64,256> hist{};quint64 total=0;
    for(int y=0;y<sample.height();++y){const uchar*r=sample.constScanLine(y);for(int x=0;x<sample.width();++x){++hist[r[x]];++total;}}
    if(!total)return rgb;
    auto pct=[&](double p){quint64 target=quint64(p*double(total-1)),acc=0;for(int i=0;i<256;++i){acc+=hist[size_t(i)];if(acc>target)return i;}return 255;};
    const int lo=pct(0.01),hiPct=pct(0.995);
    // Do not invent a black preview from an almost-uniform saturated frame.
    // If there is no useful dynamic range to stretch, keep the physical display
    // level so white stays white and the exposure warning remains truthful.
    if(hiPct-lo<8||lo>=248)return rgb;
    const int hi=hiPct;const double scale=255.0/double(hi-lo);
    for(int y=0;y<rgb.height();++y){uchar*r=rgb.scanLine(y);for(int x=0;x<rgb.width();++x)for(int c=0;c<3;++c){const int v=r[3*x+c];r[3*x+c]=uchar(std::clamp(int(std::lround((v-lo)*scale)),0,255));}}
    return rgb;
}
static QPointF brightestRegion(const QImage &input,bool *valid,double *contrast){
    if(valid)*valid=false;if(contrast)*contrast=0.0;if(input.isNull())return{};
    QImage g=input.convertToFormat(QImage::Format_Grayscale8);const double sx=g.width(),sy=g.height();
    if(std::max(g.width(),g.height())>800)g=g.scaled(800,800,Qt::KeepAspectRatio,Qt::FastTransformation);
    const int block=std::clamp(std::min(g.width(),g.height())/32,8,32);double total=0;quint64 count=0,best=-1;int bx=0,by=0;
    for(int y=0;y<g.height();++y){const uchar*r=g.constScanLine(y);for(int x=0;x<g.width();++x){total+=r[x];++count;}}
    for(int y=0;y+block<=g.height();y+=std::max(4,block/2))for(int x=0;x+block<=g.width();x+=std::max(4,block/2)){double sum=0;for(int yy=0;yy<block;++yy){const uchar*r=g.constScanLine(y+yy);for(int xx=0;xx<block;++xx)sum+=r[x+xx];}const double mean=sum/double(block*block);if(mean>best){best=mean;bx=x;by=y;}}
    const double global=count?total/double(count):0.0;if(contrast)*contrast=best-global;if(best<global+2.0)return{};if(valid)*valid=true;
    return QPointF((bx+0.5*block)*sx/double(g.width()),(by+0.5*block)*sy/double(g.height()));
}
MainWindow::MainWindow(ObservatoryController*c,QWidget*p):QMainWindow(p),c_(c){setWindowTitle("OpenAstroSuite / OpenAstroLink");resize(1500,900);statusBar()->showMessage(QString("Core: %1 — %2").arg(c_->controlMode(),c_->endpointDescription()));auto*central=new QWidget;auto*root=new QHBoxLayout(central);auto*split=new QSplitter(Qt::Horizontal);root->addWidget(split);setCentralWidget(central);
    auto*left=new QWidget;auto*ll=new QVBoxLayout(left);auto*images=new QSplitter(Qt::Horizontal);rawImage_=new QLabel("No camera frame");astroImage_=new QLabel("No astrometry overlay");for(auto*l:{rawImage_,astroImage_}){l->setAlignment(Qt::AlignCenter);l->setMinimumSize(420,300);l->setStyleSheet("background:#111;color:#bbb;border:1px solid #444");}images->addWidget(rawImage_);images->addWidget(astroImage_);ll->addWidget(images,3);starScene_=new QGraphicsScene(this);auto*map=new QGraphicsView(starScene_);map->setMinimumHeight(220);map->setStyleSheet("background:#050510");ll->addWidget(map,1);log_=new QTextEdit;log_->setReadOnly(true);log_->setMaximumHeight(170);ll->addWidget(log_);split->addWidget(left);
    tabs_=new QTabWidget;tabs_->addTab(buildDevicesTab(),"Devices");tabs_->addTab(buildLiveFinderTab(),"Live / Finder");tabs_->addTab(buildCaptureTab(),"Capture & Solve");tabs_->addTab(buildMountTab(),"Mount & Guide");tabs_->addTab(buildFocusTab(),"Focus");tabs_->addTab(buildPolarTab(),"Polar Align");tabs_->addTab(buildSchedulerTab(),"Scheduler");tabs_->addTab(buildOperationsTab(),"Operations");tabs_->addTab(buildServerTab(),"OAL Server");tabs_->addTab(buildProfileTab(),"Profile");split->addWidget(tabs_);split->setStretchFactor(0,4);split->setStretchFactor(1,2);
    connect(c_,&ObservatoryController::logMessage,this,&MainWindow::appendLog);connect(c_,&ObservatoryController::frameCaptured,this,[this](const QImage&i,const QString&id){lastImage_=i;renderCameraFrame(i);const bool scienceFrame=!id.startsWith("live-")&&!id.startsWith("af-preview-");if(histogramEnabled_&&histogramEnabled_->isChecked())updateHistogram(i,scienceFrame);if(scienceFrame)appendLog("Frame: "+id);if(!pendingSolveFrameId_.isEmpty()&&pendingSolveFrameId_==id){pendingSolveFrameId_.clear();QTimer::singleShot(0,this,[this](){SolveHint h;h.raDeg=hintRa_->value();h.decDeg=hintDec_->value();h.searchRadiusDeg=hintRadius_->value();c_->solveLast(h);});}});connect(c_,&ObservatoryController::solveCompleted,this,[this](const QJsonObject&o){appendLog(QString("Solve: %1 RA=%2 DEC=%3 matches=%4").arg(o.value("success").toBool()?"OK":"FAIL").arg(o.value("raDeg").toDouble()).arg(o.value("decDeg").toDouble()).arg(o.value("matchedStars").toInt()));updateAstrometryOverlay();updateStarMap();});connect(c_,&ObservatoryController::autofocusProgress,this,[this](const QJsonObject&o){appendLog(QString("AF pos=%1 score=%2 stars=%3").arg(o.value("position").toInt()).arg(o.value("score").toDouble()).arg(o.value("detectedStars").toInt()));if(focuserStatus_)focuserStatus_->setText(QString("Autofocus — position %1, score %2").arg(o.value("position").toInt()).arg(o.value("score").toDouble()));});connect(c_,&ObservatoryController::autofocusCompleted,this,[this](const QJsonObject&o){appendLog(QString("AF %1 best=%2 score=%3 — %4").arg(o.value("success").toBool()?"OK":"FAIL").arg(o.value("bestPosition").toInt()).arg(o.value("bestScore").toDouble()).arg(o.value("message").toString()));if(o.value("success").toBool()&&focusPosition_){QSignalBlocker b(focusPosition_);focusPosition_->setValue(o.value("bestPosition").toInt());focusTargetInitialized_=true;focusTargetDirty_=false;}setAutofocusBusy(false);autofocusOperationId_.clear();refreshFocuserStatus();});connect(c_,&ObservatoryController::polarSampleCountChanged,this,[this](int n){polarSamples_->setText(QString("Samples: %1").arg(n));});connect(c_,&ObservatoryController::polarAlignmentCompleted,this,[this](const QJsonObject&o){polarResult_->setText(QString("%1\nAxis RA %2°, DEC %3°\nTotal %4′\nAdjust altitude %5′, azimuth %6′").arg(o.value("message").toString()).arg(o.value("axisRaDeg").toDouble()).arg(o.value("axisDecDeg").toDouble()).arg(o.value("totalErrorArcmin").toDouble()).arg(o.value("altitudeAdjustmentArcmin").toDouble()).arg(o.value("azimuthAdjustmentArcmin").toDouble()));appendLog(QString("Automatic Polar Alignment completed: total error %1 arcmin").arg(o.value("totalErrorArcmin").toDouble(),0,'f',2));});connect(c_,&ObservatoryController::sessionChanged,this,[this](const QJsonObject&o){appendLog(QString("Session %1: %2").arg(o.value("name").toString(),o.value("state").toString()));});connect(c_,&ObservatoryController::motionEstimated,this,[this](const QJsonObject&o){appendLog(QString("Motion valid=%1 dx=%2 px dy=%3 px rot=%4° inliers=%5").arg(o.value("valid").toBool()).arg(o.value("dxPx").toDouble()).arg(o.value("dyPx").toDouble()).arg(o.value("rotationDeg").toDouble()).arg(o.value("inliers").toInt()));});connect(c_,&ObservatoryController::stateChanged,this,&MainWindow::updateMountStatusFromState);connect(c_,&ObservatoryController::stateChanged,this,&MainWindow::updateFocuserStatusFromState);connect(c_,&ObservatoryController::stateChanged,this,&MainWindow::updateDeviceStatusFromState);connect(c_,&ObservatoryController::operationChanged,this,&MainWindow::updateOperation);connect(c_,&ObservatoryController::profileChanged,this,[this](){auto p=c_->profile();if(lat_){lat_->setValue(p.observer.latitudeDeg);lon_->setValue(p.observer.longitudeDeg);elevation_->setValue(p.observer.elevationM);}if(profileMountGeometry_){const int i=profileMountGeometry_->findData(int(p.mount.type));if(i>=0)profileMountGeometry_->setCurrentIndex(i);}if(profileAxis1Sign_)profileAxis1Sign_->setCurrentIndex(p.mount.axis1Sign>=0?0:1);if(profileAxis2Sign_)profileAxis2Sign_->setCurrentIndex(p.mount.axis2Sign>=0?0:1);if(profilePierSide_){const int i=profilePierSide_->findText(p.mount.preferredPierSide);if(i>=0)profilePierSide_->setCurrentIndex(i);}if(homeAxis1_)homeAxis1_->setValue(p.mount.homeAxis1Deg);if(homeAxis2_)homeAxis2_->setValue(p.mount.homeAxis2Deg);if(parkAxis1_)parkAxis1_->setValue(p.mount.parkAxis1Deg);if(parkAxis2_)parkAxis2_->setValue(p.mount.parkAxis2Deg);if(autoPierFlip_)autoPierFlip_->setChecked(p.mount.allowAutomaticPierFlip);if(maxGotoAxisDelta_)maxGotoAxisDelta_->setValue(p.mount.maxGotoAxisDeltaDeg);if(mountSiteLat_){mountSiteLat_->setValue(p.observer.latitudeDeg);mountSiteLon_->setValue(p.observer.longitudeDeg);mountSiteElevation_->setValue(p.observer.elevationM);}if(mountPreferBackendSite_){QSignalBlocker b(mountPreferBackendSite_);mountPreferBackendSite_->setChecked(p.mount.preferBackendSite);}if(polarSafeEnabled_)polarSafeEnabled_->setChecked(p.polarMotionLimits.enabled);if(polarMinAz_)polarMinAz_->setValue(p.polarMotionLimits.minAzDeg);if(polarMaxAz_)polarMaxAz_->setValue(p.polarMotionLimits.maxAzDeg);if(polarMinAlt_)polarMinAlt_->setValue(p.polarMotionLimits.minAltDeg);if(polarMaxAlt_)polarMaxAlt_->setValue(p.polarMotionLimits.maxAltDeg);if(mountRa_)synchronizeMountCoordinatesFrom("j2000");});QTimer::singleShot(0,this,[this](){c_->refreshState();refreshMountStatus();refreshFocuserStatus();for(const auto&v:c_->operations(false))updateOperation(v.toObject());});}
QWidget *MainWindow::buildDevicesTab(){auto*w=new QWidget;auto*l=new QVBoxLayout(w);
    auto make=[&](const QString&name,QComboBox*&combo,QLineEdit*&endpoint,QLabel*&status,const QStringList&items,auto connectCallback,auto disconnectCallback){
        auto*g=new QGroupBox(name);auto*f=new QFormLayout(g);combo=new QComboBox;populateBackendCombo(combo,items);endpoint=new QLineEdit;status=new QLabel("Disconnected");status->setWordWrap(true);auto*b=new QPushButton("Connect / reconnect");auto*d=new QPushButton("Disconnect");
        f->addRow("Backend",combo);f->addRow("Endpoint / index",endpoint);f->addRow("Status",status);f->addRow(b);f->addRow(d);
        connect(b,&QPushButton::clicked,this,[this,combo,endpoint,connectCallback](){QString e;if(!(c_->*connectCallback)(combo->currentText(),endpoint->text(),&e))showError(e);else c_->refreshState();});
        connect(d,&QPushButton::clicked,this,[this,disconnectCallback](){QString e;if(!(c_->*disconnectCallback)(&e))showError(e);else c_->refreshState();});l->addWidget(g);
    };
    make("Camera",cameraBackend_,cameraEndpoint_,cameraDeviceStatus_,c_->cameraBackends(),&ObservatoryController::connectCamera,&ObservatoryController::disconnectCamera);cameraEndpoint_->setText("0");
    connect(cameraBackend_,&QComboBox::currentTextChanged,this,[this](const QString&b){
        const bool native=b.startsWith("native:");cameraEndpoint_->setEnabled(!native);
        if(native){cameraEndpoint_->clear();cameraEndpoint_->setPlaceholderText("Transport is owned by the native OAL driver");}
        else if(b=="opencv")cameraEndpoint_->setPlaceholderText("Video device index, e.g. 0");
        else cameraEndpoint_->setPlaceholderText("Compatibility backend endpoint / index");
    });
    if(cameraBackend_->currentText().startsWith("native:")){cameraEndpoint_->clear();cameraEndpoint_->setEnabled(false);cameraEndpoint_->setPlaceholderText("Transport is owned by the native OAL driver");}
    make("Guide camera",guideCameraBackend_,guideCameraEndpoint_,guideCameraDeviceStatus_,c_->cameraBackends(),&ObservatoryController::connectGuideCamera,&ObservatoryController::disconnectGuideCamera);guideCameraEndpoint_->setText("1");
    connect(guideCameraBackend_,&QComboBox::currentTextChanged,this,[this](const QString&b){const bool native=b.startsWith("native:");guideCameraEndpoint_->setEnabled(!native);if(native){guideCameraEndpoint_->clear();guideCameraEndpoint_->setPlaceholderText("Transport is owned by the native OAL driver");}else guideCameraEndpoint_->setPlaceholderText("Guide camera endpoint / index");});
    if(guideCameraBackend_->currentText().startsWith("native:")){guideCameraEndpoint_->clear();guideCameraEndpoint_->setEnabled(false);guideCameraEndpoint_->setPlaceholderText("Transport is owned by the native OAL driver");}
    make("Mount",mountBackend_,mountEndpoint_,mountDeviceStatus_,c_->mountBackends(),&ObservatoryController::connectMount,&ObservatoryController::disconnectMount);mountEndpoint_->setPlaceholderText("COM3, /dev/ttyUSB0, URL, or INDI host:7624/Exact Device Name");
    lastMountBackend_=mountBackend_->currentText();
    connect(mountBackend_,&QComboBox::currentTextChanged,this,[this](const QString&b){
        if(!lastMountBackend_.isEmpty())mountEndpointsByBackend_[lastMountBackend_]=mountEndpoint_->text();
        lastMountBackend_=b;
        const bool native=b.startsWith("native:");mountEndpoint_->setEnabled(!native);
        if(native){mountEndpoint_->clear();mountEndpoint_->setPlaceholderText("Transport is owned by the native OAL driver");}
        else {
            QString next=mountEndpointsByBackend_.value(b);
            if(next.isEmpty()&&(b=="synscan-app"||b=="synscan-wifi"))next="auto";
            if(next.isEmpty()&&b=="ascom-classic")next="EQMOD.Telescope";
            mountEndpoint_->setText(next);
            if(b=="indi")mountEndpoint_->setPlaceholderText("127.0.0.1:7624/Exact INDI mount device name");
            else if(b=="serial-lx200")mountEndpoint_->setPlaceholderText("/dev/ttyUSB0 or COM3");
            else if(b=="synscan-app")mountEndpoint_->setPlaceholderText("auto, or IP of PHONE/PC running SynScan Pro:11881 (not mount Wi-Fi IP)");
            else if(b=="synscan-wifi")mountEndpoint_->setPlaceholderText("auto, or mount/EQDrive Wi-Fi adapter IP[:11880] (direct UDP, no SynScan Pro)");
            else if(b=="ascom-classic")mountEndpoint_->setPlaceholderText("ASCOM Telescope ProgID, e.g. EQMOD.Telescope");
            else mountEndpoint_->setPlaceholderText("Compatibility endpoint / URL");
        }
    });
    if(mountBackend_->currentText().startsWith("native:")){mountEndpoint_->clear();mountEndpoint_->setEnabled(false);mountEndpoint_->setPlaceholderText("Transport is owned by the native OAL driver");}
    else if(mountBackend_->currentText()=="ascom-classic"&&mountEndpoint_->text().trimmed().isEmpty())mountEndpoint_->setText("EQMOD.Telescope");
    auto*ascomChooser=new QPushButton("ASCOM Chooser...");auto*ascomSetup=new QPushButton("ASCOM Properties...");
    auto updateAscomButtons=[this,ascomChooser,ascomSetup](const QString&b){const bool on=!c_->isRemote()&&b=="ascom-classic";ascomChooser->setEnabled(on);ascomSetup->setEnabled(on);};updateAscomButtons(mountBackend_->currentText());
    connect(mountBackend_,&QComboBox::currentTextChanged,this,updateAscomButtons);
    connect(ascomChooser,&QPushButton::clicked,this,[this](){QString selected,e;if(!AscomClassicMount::chooseTelescope(mountEndpoint_->text(),selected,&e)){if(!e.isEmpty())showError(e);return;}const int i=mountBackend_->findText("ascom-classic");if(i>=0)mountBackend_->setCurrentIndex(i);mountEndpoint_->setText(selected);appendLog("Classic ASCOM telescope selected: "+selected);});
    connect(ascomSetup,&QPushButton::clicked,this,[this](){QString e;if(!AscomClassicMount::setupTelescope(mountEndpoint_->text(),&e))showError(e);else appendLog("Classic ASCOM properties closed");});
    l->addWidget(ascomChooser);l->addWidget(ascomSetup);
    make("Focuser",focuserBackend_,focuserEndpoint_,focuserDeviceStatus_,c_->focuserBackends(),&ObservatoryController::connectFocuser,&ObservatoryController::disconnectFocuser);focuserEndpoint_->setPlaceholderText("Compatibility only; native Gemini appears as native:oal.gemini/... after discovery");
    connect(focuserBackend_,&QComboBox::currentTextChanged,this,[this](const QString&b){
        const bool native=b.startsWith("native:");focuserEndpoint_->setEnabled(!native);
        if(native){focuserEndpoint_->clear();focuserEndpoint_->setPlaceholderText("Transport is owned by the native OAL driver");}
        else if(b=="gemini-eaf")focuserEndpoint_->setPlaceholderText("Compatibility only: alpaca:<URL> or indi:127.0.0.1:7624/Device; native Gemini uses native:oal.gemini/...");
        else if(b=="indi")focuserEndpoint_->setPlaceholderText("127.0.0.1:7624/Exact INDI focuser device name");
        else focuserEndpoint_->setPlaceholderText("Compatibility endpoint / URL");
    });
    if(focuserBackend_->currentText().startsWith("native:")){focuserEndpoint_->clear();focuserEndpoint_->setEnabled(false);focuserEndpoint_->setPlaceholderText("Transport is owned by the native OAL driver");}

    auto*auxBox=new QGroupBox("Observatory device classes — API placeholders");auto*auxForm=new QFormLayout(auxBox);
    const std::pair<const char*,const char*> aux[]={{"Filter wheel","filter-wheel"},{"Rotator","rotator"},{"Dome / roll-off roof","dome"},{"Weather station","weather"},{"GPS / GNSS","gps"},{"Power / switch","power"},{"Cover / flat calibrator","cover-calibrator"},{"Safety monitor","safety-monitor"}};
    for(const auto &entry:aux){auto*status=new QLabel(QString("Stub only — OAL type '%1' reserved; no backend/controls yet").arg(QString::fromLatin1(entry.second)));status->setWordWrap(true);status->setStyleSheet("color:#777");auxForm->addRow(entry.first,status);}
    l->addWidget(auxBox);

    auto*serialBox=new QGroupBox("Native serial discovery");auto*serialForm=new QFormLayout(serialBox);
    nativeSerialDriver_=new QComboBox;nativeSerialDriver_->addItem("Gemini EAF","oal.gemini");nativeSerialDriver_->addItem("Sky-Watcher / EqMount","oal.skywatcher");nativeSerialDriver_->addItem("EQDrive native","oal.eqdrive");nativeSerialPort_=new QComboBox;
    auto reloadSerialPorts=[this](){
        const QString driverId=nativeSerialDriver_->currentData().toString();const QString selected=c_->nativeSerialPortOverride(driverId);nativeSerialPort_->clear();nativeSerialPort_->addItem("Auto — scan all serial ports",QString());int selectedIndex=0;
        for(const auto&v:c_->availableSerialPorts()){const auto p=v.toObject();const QString port=p.value("port").toString();QString label=port;const QString description=p.value("description").toString();const QString manufacturer=p.value("manufacturer").toString();if(!description.isEmpty())label+=" — "+description;else if(!manufacturer.isEmpty())label+=" — "+manufacturer;nativeSerialPort_->addItem(label,port);if(port==selected)selectedIndex=nativeSerialPort_->count()-1;}
        if(!selected.isEmpty()&&selectedIndex==0){nativeSerialPort_->addItem(selected+" — not currently present",selected);selectedIndex=nativeSerialPort_->count()-1;}nativeSerialPort_->setCurrentIndex(selectedIndex);
    };
    auto*refreshPorts=new QPushButton("Refresh serial port list");auto*applySerial=new QPushButton("Apply port & rediscover selected serial driver");
    serialForm->addRow("Driver",nativeSerialDriver_);serialForm->addRow("Serial port",nativeSerialPort_);serialForm->addRow(refreshPorts);serialForm->addRow(applySerial);l->addWidget(serialBox);
    connect(nativeSerialDriver_,&QComboBox::currentIndexChanged,this,[reloadSerialPorts](int){reloadSerialPorts();});connect(refreshPorts,&QPushButton::clicked,this,reloadSerialPorts);
    connect(applySerial,&QPushButton::clicked,this,[this,reloadSerialPorts](){const QString driverId=nativeSerialDriver_->currentData().toString();const QString port=nativeSerialPort_->currentData().toString();QString e;if(!c_->setNativeSerialPortOverride(driverId,port,&e)){showError(e);return;}auto refresh=[&](QComboBox*combo,const QStringList&items){const QString old=combo->currentText();populateBackendCombo(combo,items);const int i=combo->findText(old);if(i>=0)combo->setCurrentIndex(i);};refresh(cameraBackend_,c_->cameraBackends());refresh(guideCameraBackend_,c_->cameraBackends());refresh(mountBackend_,c_->mountBackends());refresh(focuserBackend_,c_->focuserBackends());
        // Serial selection is an explicit user choice: make the newly discovered
        // native device the visible selection instead of leaving the combo on a
        // simulated/old COM binding. Persistence is migrated in the node too.
        QComboBox*target=driverId=="oal.gemini"?focuserBackend_:mountBackend_;const QString prefix="native:"+driverId+"/";for(int i=0;i<target->count();++i)if(target->itemText(i).startsWith(prefix)){target->setCurrentIndex(i);break;}
        reloadSerialPorts();c_->refreshState();appendLog(port.isEmpty()?driverId+" serial discovery: automatic scan":driverId+" serial discovery: "+port);});
    reloadSerialPorts();

    auto*refreshDiscovery=new QPushButton("Refresh all native devices (USB / serial)");connect(refreshDiscovery,&QPushButton::clicked,this,[this](){
        QString discoveryError;if(!c_->refreshNativeDiscovery(&discoveryError)){showError(discoveryError);return;}
        auto refresh=[&](QComboBox*combo,const QStringList&items){const QString old=combo->currentText();populateBackendCombo(combo,items);const int i=combo->findText(old);if(i>=0)combo->setCurrentIndex(i);};
        refresh(cameraBackend_,c_->cameraBackends());refresh(guideCameraBackend_,c_->cameraBackends());refresh(mountBackend_,c_->mountBackends());refresh(focuserBackend_,c_->focuserBackends());c_->refreshState();appendLog("Native OAL discovery started; device lists will update when the asynchronous scan completes");
    });l->addWidget(refreshDiscovery);
    auto*d=new QPushButton("Disconnect all");connect(d,&QPushButton::clicked,this,[this](){QString e;if(!c_->disconnectAll(&e))showError(e);else c_->refreshState();});l->addWidget(d);l->addStretch();return w;
}
QWidget *MainWindow::buildLiveFinderTab(){
    auto*w=new QWidget;auto*l=new QVBoxLayout(w);
    auto*liveBox=new QGroupBox("Live View / target acquisition");auto*f=new QFormLayout(liveBox);
    liveExposure_=dspin(0.0001,10.0,0.001,4);liveGain_=ispin(0,102400,0);liveOffset_=ispin(0,65535,0);liveBin_=ispin(1,4,2);liveFps_=dspin(0.2,10.0,5.0,1);
    liveAutoStretch_=new QCheckBox("Auto stretch preview");liveAutoStretch_->setChecked(true);liveCrosshair_=new QCheckBox("Center crosshair");liveCrosshair_->setChecked(true);liveHighlight_=new QCheckBox("Highlight brightest region");liveHighlight_->setChecked(true);
    liveMilDot_=new QCheckBox("Mil-Dot reticle (1 mrad spacing, requires optical scale)");liveMilDot_->setChecked(false);liveAngularGrid_=new QCheckBox("Angular measurement grid (arcmin)");liveAngularGrid_->setChecked(false);
    liveDebayer_=new QCheckBox("Debayer color preview (preview only; science data stays RAW)");liveDebayer_->setChecked(false);liveBayerPattern_=new QComboBox;liveBayerPattern_->addItems({"Auto from camera/driver","RGGB","BGGR","GRBG","GBRG"});
    liveRecordSer_=new QCheckBox("Record raw Live View to SER");liveRecordSer_->setChecked(false);liveSerPath_=new QLineEdit;liveSerPath_->setPlaceholderText("blank = Pictures/OpenAstroLink/SER/Live_*.ser on node");
    f->addRow("Exposure, s",liveExposure_);f->addRow("Gain",liveGain_);f->addRow("Offset",liveOffset_);f->addRow("Bin",liveBin_);f->addRow("Target FPS",liveFps_);f->addRow(liveAutoStretch_);f->addRow(liveCrosshair_);f->addRow(liveMilDot_);f->addRow(liveAngularGrid_);f->addRow(liveHighlight_);f->addRow(liveDebayer_);f->addRow("Bayer pattern",liveBayerPattern_);f->addRow(liveRecordSer_);f->addRow("SER path on node",liveSerPath_);
    liveViewButton_=new QPushButton("Start Live View");sceneAutofocusButton_=new QPushButton("Scene autofocus (auto-meter + safe contrast search)");liveTargetStatus_=new QLabel("Bright target: waiting for a frame");liveTargetStatus_->setWordWrap(true);f->addRow(liveViewButton_);f->addRow(sceneAutofocusButton_);f->addRow(liveTargetStatus_);l->addWidget(liveBox);
    connect(liveViewButton_,&QPushButton::clicked,this,[this](){
        if(liveViewBusy_&&!liveViewOperationId_.isEmpty()){QString e;if(!c_->cancelOperation(liveViewOperationId_,&e))showError(e);else appendLog("Live View stop requested");return;}
        LiveViewRequest r;r.exposureSec=liveExposure_->value();r.gain=liveGain_->value();r.offset=liveOffset_?liveOffset_->value():0;r.binX=r.binY=liveBin_->value();r.targetFps=liveFps_->value();r.debayer=liveDebayer_&&liveDebayer_->isChecked();if(liveBayerPattern_){const QString p=liveBayerPattern_->currentIndex()==0?"AUTO":liveBayerPattern_->currentText();r.bayerPattern=bayerPatternFromString(p);}r.recordSer=liveRecordSer_&&liveRecordSer_->isChecked();if(liveSerPath_)r.serPath=liveSerPath_->text().trimmed();QString e;const QString id=c_->startLiveView(r,&e);if(id.isEmpty()){showError(e);return;}liveViewOperationId_=id;setLiveViewBusy(true);appendLog(QString("Live View started: %1").arg(id));
    });
    connect(sceneAutofocusButton_,&QPushButton::clicked,this,[this](){
        if(liveViewBusy_){showError("Stop Live View before Scene autofocus so the focuser can lock the camera resource.");return;}if(autofocusBusy_){showError("Another autofocus operation is already running");return;}
        AutofocusRequest r;r.mode=AutofocusMode::Scene;r.rangeSteps=focusRange_?focusRange_->value():1600;r.coarseStep=coarseStep_?coarseStep_->value():200;r.fineStep=fineStep_?fineStep_->value():40;r.framesPerPosition=focusFrames_?focusFrames_->value():2;r.exposureSec=liveExposure_->value();r.gain=liveGain_->value();QString e;const QString id=c_->startAutofocus(r,&e);if(id.isEmpty()){showError(e);return;}autofocusOperationId_=id;setAutofocusBusy(true);appendLog("Scene autofocus started: "+id);
    });
    for(auto *overlay:{liveCrosshair_,liveMilDot_,liveAngularGrid_,liveHighlight_})connect(overlay,&QCheckBox::toggled,this,[this](bool){if(!lastImage_.isNull())renderCameraFrame(lastImage_);});
    connect(liveDebayer_,&QCheckBox::toggled,this,[this](bool enabled){if(enabled&&liveBin_&&liveBin_->value()!=1){liveBin_->setValue(1);appendLog("Live debayer enabled: bin set to 1x1 to preserve CFA color samples");}if(liveBayerPattern_)liveBayerPattern_->setEnabled(enabled);});liveBayerPattern_->setEnabled(false);
    auto*finderBox=new QGroupBox("Finder Alignment wizard");auto*fv=new QVBoxLayout(finderBox);finderWizardText_=new QLabel;finderWizardText_->setWordWrap(true);finderWizardButton_=new QPushButton("Start / restart finder alignment");finderWizardNextButton_=new QPushButton("Next step");finderWizardNextButton_->setEnabled(false);fv->addWidget(finderWizardText_);fv->addWidget(finderWizardButton_);fv->addWidget(finderWizardNextButton_);l->addWidget(finderBox);
    connect(finderWizardButton_,&QPushButton::clicked,this,[this](){finderWizardStep_=1;if(liveCrosshair_)liveCrosshair_->setChecked(true);if(liveAutoStretch_)liveAutoStretch_->setChecked(true);finderWizardNextButton_->setEnabled(true);updateFinderWizardText();if(!lastImage_.isNull())renderCameraFrame(lastImage_);});
    connect(finderWizardNextButton_,&QPushButton::clicked,this,[this](){finderWizardStep_=std::min(5,finderWizardStep_+1);updateFinderWizardText();if(finderWizardStep_>=5)finderWizardNextButton_->setEnabled(false);});
    finderWizardStep_=0;updateFinderWizardText();
    auto*note=new QLabel("Safety: never point an unfiltered telescope/camera at the Sun. For daylight alignment use a distant terrestrial target. Daylight QHY start: about 1 ms, gain 0; if the image is white, reduce exposure before changing focus. QHY and ZWO Live View use native continuous SDK streams where available. Debayer is optional and camera-neutral: Auto uses driver CFA metadata, while RGGB/BGGR/GRBG/GBRG can be selected manually. Science FITS/RAW and SER recording remain raw/undebayered. SER is uncompressed and can become very large at full resolution/FPS. Canon DSLR repeated-shutter Live View is intentionally blocked until the dedicated EDSDK EVF transport is added.");note->setWordWrap(true);l->addWidget(note);l->addStretch();return w;
}

void MainWindow::updateFinderWizardText(){
    if(!finderWizardText_)return;QString t;
    switch(finderWizardStep_){
    case 1:t="1/5 — Acquire a distant target. Start Live View and aim the telescope at a far, contrast-rich terrestrial detail (antenna, mast, roof feature). Rough focus by hand if the entire frame is featureless.";break;
    case 2:t="2/5 — Focus the main telescope. Stop Live View, then run Scene autofocus. When it finishes, restart Live View. Scene autofocus uses image edge contrast and does not require stars.";break;
    case 3:t="3/5 — Center the main optical axis. Using the mount joystick/manual controls, put one unmistakable detail exactly under the Live View crosshair.";break;
    case 4:t="4/5 — Align the finder. Do NOT move the telescope. Adjust only the finder-scope alignment screws until the same detail is centered in the finder crosshair.";break;
    case 5:t="5/5 — Verify. Move the telescope away slightly, return to the target with the finder, and confirm the feature lands near the camera crosshair. At night, refine once on a bright star and run Star autofocus at infinity.";break;
    default:t="This wizard aligns the finder to the camera-defined optical axis. Start with a distant daylight target; no eyepiece is required.";break;
    }
    finderWizardText_->setText(t);
}

void MainWindow::renderCameraFrame(const QImage &image){
    if(image.isNull()||!rawImage_)return;
    // Diagnose completely white/black finder frames before auto-stretch. This
    // makes the common daylight failure mode obvious instead of looking like a
    // broken camera or failed autofocus.
    QImage diagnostic=image.convertToFormat(QImage::Format_Grayscale8);double rawMean=0.0;qsizetype samples=0;
    const int sampleStep=std::max(1,int(std::sqrt(double(std::max<qsizetype>(1,diagnostic.width()*diagnostic.height()))/50000.0)));
    for(int y=0;y<diagnostic.height();y+=sampleStep){const uchar*row=diagnostic.constScanLine(y);for(int x=0;x<diagnostic.width();x+=sampleStep){rawMean+=row[x];++samples;}}
    if(samples>0)rawMean/=double(samples);
    const bool frameSaturated=rawMean>250.0,frameDark=rawMean<1.0;
    QImage shown=(liveAutoStretch_&&liveAutoStretch_->isChecked())?autoStretchPreview(image):image.convertToFormat(QImage::Format_RGB888);
    QPainter p(&shown);p.setRenderHint(QPainter::Antialiasing,true);const QPointF center(shown.width()/2.0,shown.height()/2.0);
    if(liveCrosshair_&&liveCrosshair_->isChecked()){p.setPen(QPen(QColor(80,255,120),std::max(1,shown.width()/1200)));const int arm=std::max(30,std::min(shown.width(),shown.height())/12);p.drawLine(QPointF(center.x()-arm,center.y()),QPointF(center.x()+arm,center.y()));p.drawLine(QPointF(center.x(),center.y()-arm),QPointF(center.x(),center.y()+arm));p.drawEllipse(center,8,8);}
    const double scaleArcsecPerPx=std::max(0.0,c_->profile().arcsecPerPixel());
    if(liveMilDot_&&liveMilDot_->isChecked()&&scaleArcsecPerPx>0.0){
        const double stepPx=206.264806247/scaleArcsecPerPx; // 1 mrad
        if(stepPx>=4.0&&stepPx<std::max(shown.width(),shown.height())*2.0){
            p.setPen(QPen(QColor(255,190,80,180),std::max(1,shown.width()/1600),Qt::DotLine));
            for(double x=center.x();x<shown.width();x+=stepPx)p.drawLine(QPointF(x,0),QPointF(x,shown.height()));
            for(double x=center.x()-stepPx;x>=0;x-=stepPx)p.drawLine(QPointF(x,0),QPointF(x,shown.height()));
            for(double y=center.y();y<shown.height();y+=stepPx)p.drawLine(QPointF(0,y),QPointF(shown.width(),y));
            for(double y=center.y()-stepPx;y>=0;y-=stepPx)p.drawLine(QPointF(0,y),QPointF(shown.width(),y));
            p.setPen(QColor(255,210,100));p.drawText(QPointF(8,18),QString("Mil-Dot: 1 mrad = %1 px").arg(stepPx,0,'f',1));
        }
    }
    if(liveAngularGrid_&&liveAngularGrid_->isChecked()&&scaleArcsecPerPx>0.0){
        const double fovArcmin=shown.width()*scaleArcsecPerPx/60.0;
        const double choices[]={0.5,1,2,5,10,20,30,60,120};double stepArcmin=choices[0];
        for(double c:choices){stepArcmin=c;if(fovArcmin/std::max(0.01,c)<=10.0)break;}
        const double stepPx=stepArcmin*60.0/scaleArcsecPerPx;
        p.setPen(QPen(QColor(100,190,255,150),std::max(1,shown.width()/1800),Qt::DashLine));
        for(double x=center.x();x<shown.width();x+=stepPx)p.drawLine(QPointF(x,0),QPointF(x,shown.height()));
        for(double x=center.x()-stepPx;x>=0;x-=stepPx)p.drawLine(QPointF(x,0),QPointF(x,shown.height()));
        for(double y=center.y();y<shown.height();y+=stepPx)p.drawLine(QPointF(0,y),QPointF(shown.width(),y));
        for(double y=center.y()-stepPx;y>=0;y-=stepPx)p.drawLine(QPointF(0,y),QPointF(shown.width(),y));
        p.setPen(QColor(140,210,255));p.drawText(QPointF(8,36),QString("Angular grid: %1′  scale=%2″/px").arg(stepArcmin,0,'g',3).arg(scaleArcsecPerPx,0,'f',3));
    }
    bool valid=false;double contrast=0.0;const QPointF bright=brightestRegion(shown,&valid,&contrast);
    if(liveHighlight_&&liveHighlight_->isChecked()&&valid){p.setPen(QPen(QColor(255,210,40),2));const int r=std::max(14,std::min(shown.width(),shown.height())/40);p.drawEllipse(bright,r,r);p.drawLine(bright+QPointF(-r*1.4,0),bright+QPointF(r*1.4,0));p.drawLine(bright+QPointF(0,-r*1.4),bright+QPointF(0,r*1.4));}
    p.end();rawImage_->setPixmap(QPixmap::fromImage(shown).scaled(rawImage_->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
    if(liveTargetStatus_){
        if(frameSaturated)liveTargetStatus_->setText(QString("Valid camera frame — exposure-quality warning: highlights are clipped / almost white (mean=%1/255). Reduce exposure first, then gain. This is NOT a camera or transport error.").arg(rawMean,0,'f',1));
        else if(frameDark)liveTargetStatus_->setText(QString("Frame is almost black (mean=%1/255). Increase Live exposure or gain.").arg(rawMean,0,'f',2));
        else if(valid){const double dx=bright.x()-center.x(),dy=bright.y()-center.y();liveTargetStatus_->setText(QString("Bright target approx: x=%1 y=%2; offset from center dx=%3 px, dy=%4 px; local contrast=%5").arg(bright.x(),0,'f',0).arg(bright.y(),0,'f',0).arg(dx,0,'f',0).arg(dy,0,'f',0).arg(contrast,0,'f',1));}
        else liveTargetStatus_->setText("Bright target: no robust bright region detected");
    }
}

QWidget *MainWindow::buildCaptureTab(){
    auto*w=new QWidget;auto*l=new QVBoxLayout(w);auto*f=new QFormLayout;
    exposure_=dspin(0.0001,3600,1,4);gain_=ispin(0,102400,0);solverBackend_=new QComboBox;solverBackend_->addItems(c_->solverBackends());catalogPath_=new QLineEdit("config/stars_example.csv");modelPath_=new QLineEdit;hintRa_=dspin(0,360,83.8221,6);hintDec_=dspin(-90,90,-5.3911,6);hintRadius_=dspin(0.1,180,20,2);
    solveBin_=ispin(1,4,2);solveStackFrames_=ispin(2,9,3);solveMinStars_=ispin(4,100,20);solveBaseExposure_=dspin(0.05,15.0,1.5,2);solveMaxExposure_=dspin(0.1,15.0,3.0,2);
    saveScience_=new QCheckBox("Save user captures as science files");saveScience_->setChecked(true);
    f->addRow("Exposure, s",exposure_);f->addRow("Gain",gain_);f->addRow(saveScience_);f->addRow("Solver",solverBackend_);f->addRow("Catalog CSV",catalogPath_);f->addRow("Neural model",modelPath_);f->addRow("Hint RA J2000, deg",hintRa_);f->addRow("Hint DEC J2000, deg",hintDec_);f->addRow("Search radius, deg",hintRadius_);
    f->addRow("Adaptive base exposure, s",solveBaseExposure_);f->addRow("Adaptive solve bin",solveBin_);f->addRow("Adaptive stack frames",solveStackFrames_);f->addRow("Adaptive max single exposure, s",solveMaxExposure_);f->addRow("Adaptive minimum stars",solveMinStars_);l->addLayout(f);
    auto*histGroup=new QGroupBox("Preview histogram / exposure assistant");auto*histLayout=new QVBoxLayout(histGroup);auto*histControls=new QFormLayout;histogramEnabled_=new QCheckBox("Compute after each received frame");histogramAutoExposure_=new QCheckBox("Auto-apply suggested exposure to next capture");histogramTarget_=dspin(3.0,40.0,18.0,1);histogramTarget_->setSuffix(" %");histControls->addRow(histogramEnabled_);histControls->addRow("Target background",histogramTarget_);histControls->addRow(histogramAutoExposure_);histLayout->addLayout(histControls);histogramView_=new QLabel("Histogram disabled");histogramView_->setAlignment(Qt::AlignCenter);histogramView_->setMinimumHeight(150);histogramView_->setStyleSheet("background:#101010;color:#bbb;border:1px solid #444");histogramStats_=new QLabel("Uses fixed sensor scale for linear 8/16-bit camera frames; Canon embedded-JPEG guidance remains approximate.");histogramStats_->setWordWrap(true);histogramApplyButton_=new QPushButton("Apply suggested exposure");histogramApplyButton_->setEnabled(false);histLayout->addWidget(histogramView_);histLayout->addWidget(histogramStats_);histLayout->addWidget(histogramApplyButton_);auto resetHistogramAuto=[this](){histogramAutoBestExposure_=0.0;histogramAutoBestObjective_=1.0e9;histogramAutoPreviousSignedError_=0.0;histogramAutoHavePrevious_=false;histogramAutoConverged_=false;histogramAutoOutOfBandFrames_=0;};connect(histogramEnabled_,&QCheckBox::toggled,this,[this,resetHistogramAuto](bool on){resetHistogramAuto();if(!on){histogramView_->setText("Histogram disabled");histogramView_->setPixmap(QPixmap());histogramStats_->setText("Uses fixed sensor scale for linear 8/16-bit camera frames; Canon embedded-JPEG guidance remains approximate.");histogramApplyButton_->setEnabled(false);histogramSuggestedExposure_=0.0;}else if(!lastImage_.isNull())updateHistogram(lastImage_,false);});connect(histogramAutoExposure_,&QCheckBox::toggled,this,[resetHistogramAuto](bool){resetHistogramAuto();});connect(histogramTarget_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[resetHistogramAuto](double){resetHistogramAuto();});connect(histogramApplyButton_,&QPushButton::clicked,this,[this](){if(histogramSuggestedExposure_>0.0){exposure_->setValue(histogramSuggestedExposure_);appendLog(QString("Histogram suggestion applied: next exposure %1 s; ISO/gain unchanged").arg(histogramSuggestedExposure_,0,'g',5));}});l->addWidget(histGroup);
    auto*loadCatalog=new QPushButton("Load catalog");auto*loadModel=new QPushButton("Load neural model");connect(loadCatalog,&QPushButton::clicked,this,[this](){QString e;if(!c_->loadCatalog(catalogPath_->text(),&e))showError(e);});connect(loadModel,&QPushButton::clicked,this,[this](){QString e;if(!c_->loadNeuralModel(modelPath_->text(),&e))showError(e);});connect(solverBackend_,&QComboBox::currentTextChanged,this,[this](const QString&x){QString e;if(!c_->selectSolver(x,&e))showError(e);});
    captureButton_=new QPushButton("Capture");solveButton_=new QPushButton("Solve last frame");captureSolveButton_=new QPushButton("Capture + solve");adaptiveSolveButton_=new QPushButton("Adaptive urban capture + solve");motionButton_=new QPushButton("Estimate motion between last two frames");
    auto startCapture=[this](bool solveAfter){if(captureBusy_&&!captureOperationId_.isEmpty()){QString e;if(!c_->cancelOperation(captureOperationId_,&e))showError(e);else appendLog("Exposure cancellation requested");return;}ExposureRequest r;r.exposureSec=exposure_->value();r.gain=gain_->value();r.saveRaw=!saveScience_||saveScience_->isChecked();captureSolveRequested_=solveAfter;QString e;const QString id=c_->startCapture(r,&e);if(id.isEmpty()){captureSolveRequested_=false;showError(e);return;}captureOperationId_=id;setCaptureBusy(true);appendLog(QString("Exposure operation started: %1 (%2 s)").arg(id).arg(r.exposureSec));};
    connect(captureButton_,&QPushButton::clicked,this,[startCapture](){startCapture(false);});auto doSolve=[this](){SolveHint h;h.raDeg=hintRa_->value();h.decDeg=hintDec_->value();h.searchRadiusDeg=hintRadius_->value();c_->solveLast(h);};connect(solveButton_,&QPushButton::clicked,this,doSolve);connect(captureSolveButton_,&QPushButton::clicked,this,[startCapture](){startCapture(true);});
    connect(adaptiveSolveButton_,&QPushButton::clicked,this,[this](){if(adaptiveSolveBusy_&&!adaptiveSolveOperationId_.isEmpty()){QString e;if(!c_->cancelOperation(adaptiveSolveOperationId_,&e))showError(e);else appendLog("Adaptive solve cancellation requested");return;}AdaptiveSolveRequest r;r.maxSingleExposureSec=solveMaxExposure_->value();r.exposure.exposureSec=std::min(solveBaseExposure_->value(),r.maxSingleExposureSec);r.exposure.gain=gain_->value();r.exposure.binX=solveBin_->value();r.exposure.binY=solveBin_->value();r.stackFrames=solveStackFrames_->value();r.finalStackFrames=std::min(15,solveStackFrames_->value()+2);r.minStarsForSolve=solveMinStars_->value();r.maxCapturePhaseSec=120.0;r.hint.searchRadiusDeg=hintRadius_->value();MountStatus solveMount;if(c_->mountStatus(solveMount,nullptr)&&solveMount.coordinateValid){r.hint.raDeg=solveMount.coordinate.raDeg;r.hint.decDeg=solveMount.coordinate.decDeg;}else{r.hint.raDeg=hintRa_->value();r.hint.decDeg=hintDec_->value();}r.useMountHint=true;QString e;const QString id=c_->startAdaptiveSolve(r,&e);if(id.isEmpty()){showError(e);return;}adaptiveSolveOperationId_=id;setAdaptiveSolveBusy(true);appendLog(QString("Adaptive urban solve started: %1 — short frames, %2x%2 bin, mount hint preferred").arg(id).arg(r.exposure.binX));});
    connect(motionButton_,&QPushButton::clicked,this,[this](){c_->estimateLastMotion();});l->addWidget(loadCatalog);l->addWidget(loadModel);l->addWidget(captureButton_);l->addWidget(solveButton_);l->addWidget(captureSolveButton_);l->addWidget(adaptiveSolveButton_);l->addWidget(motionButton_);l->addStretch();return w;
}

void MainWindow::updateHistogram(const QImage &image,bool allowAutoApply){
    if(image.isNull()||!histogramView_||!histogramStats_)return;
    QImage gray=image.convertToFormat(QImage::Format_Grayscale8);
    if(std::max(gray.width(),gray.height())>1200)gray=gray.scaled(1200,1200,Qt::KeepAspectRatio,Qt::FastTransformation);
    std::array<quint64,256> bins{};quint64 total=0;
    for(int y=0;y<gray.height();++y){const uchar*row=gray.constScanLine(y);for(int x=0;x<gray.width();++x){++bins[row[x]];++total;}}
    if(!total)return;
    auto percentile=[&](double p){const quint64 target=quint64(std::clamp(p,0.0,1.0)*double(total-1));quint64 acc=0;for(int i=0;i<256;++i){acc+=bins[size_t(i)];if(acc>target)return i;}return 255;};
    const int p01=percentile(0.01),p50=percentile(0.50),p99=percentile(0.99);
    const double lowClip=double(bins[0]+bins[1])/double(total);const double highClip=double(bins[254]+bins[255])/double(total);
    const int W=560,H=170;QImage plot(W,H,QImage::Format_ARGB32_Premultiplied);plot.fill(QColor(16,16,16));QPainter painter(&plot);painter.setRenderHint(QPainter::Antialiasing,false);
    double maxLog=1.0;for(auto n:bins)maxLog=std::max(maxLog,std::log1p(double(n)));
    painter.setPen(QPen(QColor(170,190,220),1));for(int i=0;i<256;++i){const double h=std::log1p(double(bins[size_t(i)]))/maxLog;const int x=int(std::lround(double(i)*(W-1)/255.0));painter.drawLine(x,H-2,x,H-2-int(std::lround(h*(H-12))));}
    const int targetBin=int(std::lround(std::clamp(histogramTarget_?histogramTarget_->value()/100.0:0.18,0.0,1.0)*255.0));
    const int targetX=int(std::lround(double(targetBin)*(W-1)/255.0));painter.setPen(QPen(QColor(90,220,120),1,Qt::DashLine));painter.drawLine(targetX,0,targetX,H);painter.setPen(QPen(QColor(240,190,80),1));const int medX=int(std::lround(double(p50)*(W-1)/255.0));painter.drawLine(medX,0,medX,H);painter.end();histogramView_->setPixmap(QPixmap::fromImage(plot));
    const double median=double(p50)/255.0,target=histogramTarget_?histogramTarget_->value()/100.0:0.18;
    const double p99Level=std::max(1.0/255.0,double(p99)/255.0);
    if(!allowAutoApply){
        histogramStats_->setText(QString("Sensor-scale preview only: P1=%1%, median=%2%, P99=%3%, low clip=%4%, high clip=%5%. Live/AF frames do not change the still-capture auto-exposure controller.")
            .arg(100.0*p01/255.0,0,'f',1).arg(100.0*p50/255.0,0,'f',1).arg(100.0*p99/255.0,0,'f',1).arg(100.0*lowClip,0,'f',2).arg(100.0*highClip,0,'f',2));
        histogramApplyButton_->setEnabled(false);return;
    }
    const double measured=std::max(1.0/255.0,median);
    const double current=std::max(0.0001,exposure_?exposure_->value():1.0);
    const double signedError=std::log(measured/std::max(1.0/255.0,target));
    const double relativeError=std::abs(median-target)/std::max(0.01,target);
    const double objective=std::abs(signedError)+8.0*highClip+std::max(0.0,p99Level-0.88)*3.0;

    if(histogramAutoBestExposure_<=0.0||objective<histogramAutoBestObjective_){
        histogramAutoBestObjective_=objective;histogramAutoBestExposure_=current;
    }

    const bool healthyHighlights=highClip<=0.002&&p99Level<=0.92;
    const bool inBand=relativeError<=0.14&&healthyHighlights;
    const bool crossed=histogramAutoHavePrevious_&&((histogramAutoPreviousSignedError_<0.0&&signedError>0.0)||(histogramAutoPreviousSignedError_>0.0&&signedError<0.0));
    if(inBand){
        histogramAutoConverged_=true;histogramAutoBestExposure_=current;histogramAutoBestObjective_=objective;histogramAutoOutOfBandFrames_=0;
    }
    if(histogramAutoConverged_){
        const bool sceneChanged=relativeError>0.40||highClip>0.02||p99Level>0.98;
        histogramAutoOutOfBandFrames_=sceneChanged?histogramAutoOutOfBandFrames_+1:0;
        if(histogramAutoOutOfBandFrames_>=2){histogramAutoConverged_=false;histogramAutoBestExposure_=current;histogramAutoBestObjective_=objective;histogramAutoHavePrevious_=false;histogramAutoOutOfBandFrames_=0;appendLog("Histogram auto exposure unlocked after a persistent scene/illumination change");}
    }

    double factor=1.0,next=current;
    if(!histogramAutoConverged_){
        const double ratio=target/measured;
        factor=std::pow(std::clamp(ratio,0.08,12.0),0.68);
        factor=std::clamp(factor,0.45,2.50);
        if(p99Level>0.88)factor=std::min(factor,std::pow(0.88/p99Level,0.90));
        if(highClip>0.005)factor=std::min(factor,0.45);
        if(lowClip>0.80&&median<0.015&&p99Level<0.55)factor=std::max(factor,1.35);
        next=std::clamp(current*factor,0.0001,3600.0);
        // A sign crossing is expected while converging; the damped proportional
        // step continues toward the target instead of declaring success early.
    }else if(histogramAutoBestExposure_>0.0){
        next=histogramAutoBestExposure_;
    }
    Q_UNUSED(crossed);histogramAutoPreviousSignedError_=signedError;histogramAutoHavePrevious_=true;
    histogramSuggestedExposure_=next;const double delta=std::abs(histogramSuggestedExposure_/current-1.0);
    QString advice;
    if(histogramAutoConverged_)advice=QString("AUTO LOCKED near optimum (%1 s); change scene/target or toggle Auto to reacquire").arg(histogramAutoBestExposure_,0,'g',5);
    else if(highClip>0.005)advice="highlights clipped — reduce exposure";
    else if(p99Level>0.88)advice="bright tail near saturation — shorten exposure";
    else if(median<target*0.86)advice="background low — increase exposure";
    else if(median>target*1.14)advice="background high — decrease exposure";
    else advice="inside acquisition band — locking exposure";
    histogramStats_->setText(QString("Sensor-scale histogram: P1=%1%, median=%2%, P99=%3%, low clip=%4%, high clip=%5%. %6. Best=%7 s; suggested=%8 s.")
        .arg(100.0*p01/255.0,0,'f',1).arg(100.0*p50/255.0,0,'f',1).arg(100.0*p99/255.0,0,'f',1).arg(100.0*lowClip,0,'f',2).arg(100.0*highClip,0,'f',2).arg(advice).arg(histogramAutoBestExposure_,0,'g',5).arg(histogramSuggestedExposure_,0,'g',5));
    histogramApplyButton_->setEnabled(!histogramAutoConverged_&&delta>0.025);
    if(allowAutoApply&&histogramAutoExposure_&&histogramAutoExposure_->isChecked()&&!histogramAutoConverged_&&delta>0.025){exposure_->setValue(histogramSuggestedExposure_);appendLog(QString("Histogram auto exposure: next capture %1 s (convergent sensor-scale controller)").arg(histogramSuggestedExposure_,0,'g',5));}
    else if(allowAutoApply&&histogramAutoExposure_&&histogramAutoExposure_->isChecked()&&histogramAutoConverged_){appendLog(QString("Histogram auto exposure LOCKED at %1 s; median=%2%, P99=%3%").arg(histogramAutoBestExposure_,0,'g',5).arg(100.0*median,0,'f',1).arg(100.0*p99Level,0,'f',1));}

}

QWidget *MainWindow::buildMountTab(){
    auto*w=new QWidget;auto*outer=new QVBoxLayout(w);outer->setContentsMargins(0,0,0,0);
    auto*scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
    auto*content=new QWidget;auto*l=new QVBoxLayout(content);scroll->setWidget(content);outer->addWidget(scroll);

    auto*coordsBox=new QGroupBox("Target coordinates — synchronized");auto*coords=new QGridLayout(coordsBox);
    mountRa_=dspin(0,360,83.8221,6);mountDec_=dspin(-90,90,-5.3911,6);
    mountJNowRa_=dspin(0,360,83.8221,6);mountJNowDec_=dspin(-90,90,-5.3911,6);
    mountAz_=dspin(0,360,0,6);mountAlt_=dspin(-90,90,0,6);
    mountGalL_=dspin(0,360,0,6);mountGalB_=dspin(-90,90,0,6);
    coords->addWidget(new QLabel("System"),0,0);coords->addWidget(new QLabel("Longitude / RA, deg"),0,1);coords->addWidget(new QLabel("Latitude / DEC, deg"),0,2);
    coords->addWidget(new QLabel("Equatorial J2000"),1,0);coords->addWidget(mountRa_,1,1);coords->addWidget(mountDec_,1,2);
    coords->addWidget(new QLabel("Equatorial JNow"),2,0);coords->addWidget(mountJNowRa_,2,1);coords->addWidget(mountJNowDec_,2,2);
    coords->addWidget(new QLabel("Horizontal (Az / Alt)"),3,0);coords->addWidget(mountAz_,3,1);coords->addWidget(mountAlt_,3,2);
    coords->addWidget(new QLabel("Galactic (l / b)"),4,0);coords->addWidget(mountGalL_,4,1);coords->addWidget(mountGalB_,4,2);
    mountCoordEpoch_=new QLabel;mountCoordEpoch_->setWordWrap(true);coords->addWidget(mountCoordEpoch_,5,0,1,3);l->addWidget(coordsBox);
    auto*siteTimeBox=new QGroupBox("Observatory site / time");auto*siteTime=new QGridLayout(siteTimeBox);const auto observer=c_->profile().observer;
    mountSiteLat_=dspin(-90,90,observer.latitudeDeg,6);mountSiteLon_=dspin(-180,180,observer.longitudeDeg,6);mountSiteElevation_=dspin(-500,10000,observer.elevationM,1);
    mountUseSystemUtc_=new QCheckBox("Use live system UTC for coordinate conversion / mount control");mountUseSystemUtc_->setChecked(true);mountPreferBackendSite_=new QCheckBox("For ASCOM, use mount/EQMOD site as authoritative");mountPreferBackendSite_->setChecked(c_->profile().mount.preferBackendSite);mountConversionUtc_=new QDateTimeEdit(QDateTime::currentDateTimeUtc());mountConversionUtc_->setDisplayFormat("yyyy-MM-dd HH:mm:ss 'UTC'");mountConversionUtc_->setTimeSpec(Qt::UTC);mountConversionUtc_->setEnabled(false);
    auto*saveSite=new QPushButton("Save site");auto*detectSite=new QPushButton("Auto-detect system location");auto*applySiteTime=new QPushButton("Apply site/time to mount backend");mountSiteTimeStatus_=new QLabel;mountSiteTimeStatus_->setWordWrap(true);
    siteTime->addWidget(new QLabel("Latitude"),0,0);siteTime->addWidget(mountSiteLat_,0,1);siteTime->addWidget(new QLabel("Longitude (east +)"),0,2);siteTime->addWidget(mountSiteLon_,0,3);siteTime->addWidget(new QLabel("Elevation m"),0,4);siteTime->addWidget(mountSiteElevation_,0,5);
    siteTime->addWidget(mountUseSystemUtc_,1,0,1,3);siteTime->addWidget(mountConversionUtc_,1,3,1,3);
    siteTime->addWidget(mountPreferBackendSite_,2,0,1,6);
    siteTime->addWidget(saveSite,3,0,1,2);siteTime->addWidget(detectSite,3,2,1,2);siteTime->addWidget(applySiteTime,3,4,1,2);siteTime->addWidget(mountSiteTimeStatus_,4,0,1,6);l->addWidget(siteTimeBox);
    connect(saveSite,&QPushButton::clicked,this,[this](){auto p=c_->profile();p.observer={mountSiteLat_->value(),mountSiteLon_->value(),mountSiteElevation_->value()};c_->setProfile(p);appendLog(QString("Observatory site saved: lat=%1 lon=%2 elev=%3 m").arg(p.observer.latitudeDeg,0,'f',6).arg(p.observer.longitudeDeg,0,'f',6).arg(p.observer.elevationM,0,'f',1));synchronizeMountCoordinatesFrom("j2000");});
    connect(detectSite,&QPushButton::clicked,c_,&ObservatoryController::requestSystemLocation);
    connect(applySiteTime,&QPushButton::clicked,this,[this](){ObserverLocation site{mountSiteLat_->value(),mountSiteLon_->value(),mountSiteElevation_->value()};const QDateTime utc=(mountUseSystemUtc_&&mountUseSystemUtc_->isChecked())?QDateTime::currentDateTimeUtc():mountConversionUtc_->dateTime().toUTC();QString e;if(!c_->setMountSiteTime(site,utc,&e))showError(e);else{appendLog(QString("Mount backend site/time synchronized: lat=%1 lon=%2 elev=%3m UTC=%4").arg(site.latitudeDeg,0,'f',6).arg(site.longitudeDeg,0,'f',6).arg(site.elevationM,0,'f',1).arg(utc.toString(Qt::ISODateWithMs)));refreshMountStatus();}});
    connect(mountPreferBackendSite_,&QCheckBox::toggled,this,[this](bool on){auto p=c_->profile();if(p.mount.preferBackendSite==on)return;p.mount.preferBackendSite=on;c_->setProfile(p);appendLog(on?"ASCOM site authority: backend/EQMOD -> OpenAstroLink profile":"ASCOM site authority: OpenAstroLink profile -> backend (requires writable ASCOM site properties)");});
    connect(mountUseSystemUtc_,&QCheckBox::toggled,this,[this](bool live){mountConversionUtc_->setEnabled(!live);if(live)mountConversionUtc_->setDateTime(QDateTime::currentDateTimeUtc());synchronizeMountCoordinatesFrom("j2000");});
    connect(mountConversionUtc_,&QDateTimeEdit::dateTimeChanged,this,[this](const QDateTime&){if(mountUseSystemUtc_&&!mountUseSystemUtc_->isChecked())synchronizeMountCoordinatesFrom("j2000");});
    mountClockTimer_=new QTimer(this);mountClockTimer_->setInterval(1000);connect(mountClockTimer_,&QTimer::timeout,this,[this](){const auto utc=QDateTime::currentDateTimeUtc();const auto local=QDateTime::currentDateTime();if(mountUseSystemUtc_&&mountUseSystemUtc_->isChecked())mountConversionUtc_->setDateTime(utc);const auto o=c_->profile().observer;const double lst=localSiderealTimeDeg(utc,o.longitudeDeg);if(mountSiteTimeStatus_)mountSiteTimeStatus_->setText(QString("System local: %1   UTC: %2   LST: %3° (%4 h)   site: %5°, %6°, %7 m").arg(local.toString("yyyy-MM-dd HH:mm:ss t"),utc.toString("yyyy-MM-dd HH:mm:ss")).arg(lst,0,'f',4).arg(lst/15.0,0,'f',4).arg(o.latitudeDeg,0,'f',6).arg(o.longitudeDeg,0,'f',6).arg(o.elevationM,0,'f',1));});mountClockTimer_->start();

    auto syncPair=[this](QDoubleSpinBox*a,QDoubleSpinBox*b,const QString&system){connect(a,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,system](double){synchronizeMountCoordinatesFrom(system);});connect(b,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,system](double){synchronizeMountCoordinatesFrom(system);});};
    syncPair(mountRa_,mountDec_,"j2000");syncPair(mountJNowRa_,mountJNowDec_,"jnow");syncPair(mountAz_,mountAlt_,"horizontal");syncPair(mountGalL_,mountGalB_,"galactic");
    synchronizeMountCoordinatesFrom("j2000");

    auto*presetRow=new QHBoxLayout;auto*polaris=new QPushButton("Polaris preset (J2000)");auto*currentPointing=new QPushButton("Use current mount pointing as target");auto*refreshTransforms=new QPushButton("Refresh coordinate transforms for current UTC");
    connect(polaris,&QPushButton::clicked,this,[this](){mountCoordinateSyncing_=true;{QSignalBlocker a(mountRa_),b(mountDec_);mountRa_->setValue(37.9545607);mountDec_->setValue(89.2641090);}mountCoordinateSyncing_=false;synchronizeMountCoordinatesFrom("j2000");appendLog("Target fields set to Polaris J2000. This does NOT Sync the mount until you press Sync explicitly.");});
    connect(currentPointing,&QPushButton::clicked,this,[this](){MountStatus st;QString e;if(!c_->mountStatus(st,&e)||!st.coordinateValid){showError(e.isEmpty()?"Mount does not currently report a valid sky coordinate":e);return;}const auto j=convertEquatorialFrame(st.coordinate,EquatorialFrame::J2000);mountCoordinateSyncing_=true;{QSignalBlocker a(mountRa_),b(mountDec_);mountRa_->setValue(j.raDeg);mountDec_->setValue(j.decDeg);}mountCoordinateSyncing_=false;synchronizeMountCoordinatesFrom("j2000");appendLog(QString("Target fields copied from current mount pointing: RA=%1 DEC=%2 J2000").arg(j.raDeg,0,'f',6).arg(j.decDeg,0,'f',6));});
    connect(refreshTransforms,&QPushButton::clicked,this,[this](){synchronizeMountCoordinatesFrom("j2000");});presetRow->addWidget(polaris);presetRow->addWidget(currentPointing);presetRow->addWidget(refreshTransforms);l->addLayout(presetRow);

    mountFrame_=new QComboBox;mountFrame_->addItem("J2000 / catalog",int(EquatorialFrame::J2000));mountFrame_->addItem("JNow / of-date",int(EquatorialFrame::JNow));
    auto*displayRow=new QHBoxLayout;displayRow->addWidget(new QLabel("Mount status equatorial display:"));displayRow->addWidget(mountFrame_);l->addLayout(displayRow);
    mountStatus_=new QLabel("Mount status: unavailable");mountStatus_->setWordWrap(true);l->addWidget(mountStatus_);auto*refresh=new QPushButton("Refresh mount status");connect(refresh,&QPushButton::clicked,this,&MainWindow::refreshMountStatus);l->addWidget(refresh);
    auto*syncWarning=new QLabel("MOUNT MODEL: native raw-axis mounts can restore the sky model automatically from a repeatable saved Home pose. Manual Sync is then optional and is best used later with a plate-solved field away from the pole to refine pointing. ASCOM owns its own coordinate model; by default OAL reads the site from ASCOM/EQMOD and uses that same location for all coordinate transforms.");syncWarning->setWordWrap(true);syncWarning->setStyleSheet("color:#a40;font-weight:bold");l->addWidget(syncWarning);
    auto*rawGotoLimit=dspin(0.1,180.0,c_->profile().mount.maxGotoAxisDeltaDeg,1);auto*rawGotoRow=new QHBoxLayout;rawGotoRow->addWidget(new QLabel("Native max sky GOTO separation, deg"));rawGotoRow->addWidget(rawGotoLimit);rawGotoRow->addWidget(new QLabel("(true angular distance; near-pole RA motor motion may be much larger)"));l->addLayout(rawGotoRow);connect(rawGotoLimit,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double v){auto p=c_->profile();if(std::abs(p.mount.maxGotoAxisDeltaDeg-v)<1e-9)return;p.mount.maxGotoAxisDeltaDeg=v;c_->setProfile(p);appendLog(QString("Native sky-separation GOTO safety limit set to %1°").arg(v,0,'f',1));});
    auto*slew=new QPushButton("Slew to synchronized target");auto*sync=new QPushButton("Sync current physical pointing to synchronized target");
    connect(slew,&QPushButton::clicked,this,[this](){QString e;const EquatorialCoord t{mountRa_->value(),mountDec_->value(),EquatorialFrame::J2000};if(!c_->slewMount(t,&e))showError(e);else{appendLog(QString("Slew accepted: RA=%1°, DEC=%2° J2000").arg(t.raDeg,0,'f',6).arg(t.decDeg,0,'f',6));refreshMountStatus();}});
    connect(sync,&QPushButton::clicked,this,[this](){QString e;const EquatorialCoord t{mountRa_->value(),mountDec_->value(),EquatorialFrame::J2000};if(!c_->syncMount(t,&e))showError(e);else{appendLog(QString("Mount sync OK: RA=%1°, DEC=%2° J2000 — asserted physical pointing anchor").arg(t.raDeg,0,'f',6).arg(t.decDeg,0,'f',6));refreshMountStatus();}});l->addWidget(slew);l->addWidget(sync);
    auto*syncSolved=new QPushButton("Sync mount to last successful plate solve");connect(syncSolved,&QPushButton::clicked,this,[this](){const auto sol=c_->lastSolve();if(!sol.success){showError("No successful plate solve is available for a safe Sync");return;}QString e;EquatorialCoord t{sol.raDeg,sol.decDeg,EquatorialFrame::J2000};if(!c_->syncMount(t,&e)){showError(e);return;}mountCoordinateSyncing_=true;{QSignalBlocker a(mountRa_),b(mountDec_);mountRa_->setValue(t.raDeg);mountDec_->setValue(t.decDeg);}mountCoordinateSyncing_=false;synchronizeMountCoordinatesFrom("j2000");appendLog(QString("Mount synced to solved field: RA=%1°, DEC=%2° J2000").arg(t.raDeg,0,'f',6).arg(t.decDeg,0,'f',6));refreshMountStatus();});l->addWidget(syncSolved);
    auto*abort=new QPushButton("ABORT MOUNT MOTION");connect(abort,&QPushButton::clicked,this,[this](){QString e;if(!c_->abortMountMotion(&e))showError(e);else{appendLog("Mount abort accepted");refreshMountStatus();}});l->addWidget(abort);
    auto*trackingRow=new QHBoxLayout;mountTracking_=new QCheckBox("Tracking");mountTrackingRate_=new QComboBox;mountTrackingRate_->addItem("Sidereal / stars","sidereal");mountTrackingRate_->addItem("Lunar / Moon","lunar");mountTrackingRate_->addItem("Solar / Sun","solar");trackingRow->addWidget(mountTracking_);trackingRow->addWidget(new QLabel("Rate"));trackingRow->addWidget(mountTrackingRate_);l->addLayout(trackingRow);
    connect(mountTracking_,&QCheckBox::toggled,this,[this](bool v){QString e;const TrackingRate rate=trackingRateFromString(mountTrackingRate_?mountTrackingRate_->currentData().toString():"sidereal");if(!c_->setMountTracking(v,rate,&e)){QSignalBlocker b(mountTracking_);mountTracking_->setChecked(!v);showError(e);}else{appendLog(QString("Mount tracking %1 rate=%2").arg(v?"ON":"OFF",trackingRateName(rate)));refreshMountStatus();}});
    connect(mountTrackingRate_,&QComboBox::currentIndexChanged,this,[this](int){if(mountTracking_&&mountTracking_->isChecked()){QString e;const auto rate=trackingRateFromString(mountTrackingRate_->currentData().toString());if(!c_->setMountTracking(true,rate,&e))showError(e);else appendLog("Tracking rate changed to "+trackingRateName(rate));}});
    mountParked_=new QCheckBox("Parked");connect(mountParked_,&QCheckBox::toggled,this,[this](bool v){QString e;if(!c_->parkMount(v,&e)){QSignalBlocker b(mountParked_);mountParked_->setChecked(!v);showError(e);}else{appendLog(v?"Mount parking slew started":"Mount unpark/parking-stop accepted");refreshMountStatus();}});l->addWidget(mountParked_);connect(mountFrame_,&QComboBox::currentIndexChanged,this,[this](int){refreshMountStatus();});
    auto*manualBox=new QGroupBox("Manual two-axis slew");auto*manualLayout=new QVBoxLayout(manualBox);auto*rateRow=new QHBoxLayout;rateRow->addWidget(new QLabel("Rate level (1 slow — 9 fast)"));mountManualRate_=ispin(1,9,4);rateRow->addWidget(mountManualRate_);manualLayout->addLayout(rateRow);auto*pad=new QGridLayout;
    auto manualButton=[this,pad](const QString&label,int row,int col,int a1,int a2){auto*b=new QPushButton(label);b->setMinimumHeight(34);connect(b,&QPushButton::pressed,this,[this,a1,a2](){QString e;if(!c_->manualMountSlew(a1,a2,mountManualRate_?mountManualRate_->value():4,&e))showError(e);});connect(b,&QPushButton::released,this,[this](){QString e;if(!c_->manualMountSlew(0,0,0,&e))appendLog("Manual slew stop warning: "+e);});pad->addWidget(b,row,col);};
    manualButton("↖",0,0,-1,+1);manualButton("Axis 2 +",0,1,0,+1);manualButton("↗",0,2,+1,+1);manualButton("Axis 1 −",1,0,-1,0);auto*manualStop=new QPushButton("STOP");manualStop->setMinimumHeight(34);connect(manualStop,&QPushButton::clicked,this,[this](){QString e;if(!c_->manualMountSlew(0,0,0,&e))showError(e);else appendLog("Manual mount slew stopped");});pad->addWidget(manualStop,1,1);manualButton("Axis 1 +",1,2,+1,0);manualButton("↙",2,0,-1,-1);manualButton("Axis 2 −",2,1,0,-1);manualButton("↘",2,2,+1,-1);manualLayout->addLayout(pad);manualLayout->addWidget(new QLabel("Press and hold. Native raw mounts use mechanical Axis 1/2 signs from the configured installation; STOP/ABORT remain available."));l->addWidget(manualBox);
    auto*homeBox=new QGroupBox("Repeatable mechanical Home / automatic sky model");auto*homeLayout=new QGridLayout(homeBox);
    auto*autoHome=new QCheckBox("Assume saved Home on connect (auto-restore coordinate model)");autoHome->setChecked(c_->profile().mount.autoHomeSync);
    auto*homeTolerance=dspin(0.1,15.0,c_->profile().mount.homeToleranceDeg,2);
    auto*setHome=new QPushButton("Set current mechanical axes as Home");
    auto*clearHome=new QPushButton("Clear saved Home");
    auto*homeInfo=new QLabel("Use this only when the mount is in the repeatable equatorial Home pose: counterweight axis down and telescope/DEC axis toward the celestial pole. On connect, if raw axes are within the tolerance, OAL restores the sky transform automatically. A later plate solve + Sync can refine pointing without being required every startup.");homeInfo->setWordWrap(true);
    homeLayout->addWidget(autoHome,0,0,1,3);homeLayout->addWidget(new QLabel("Home match tolerance, deg"),1,0);homeLayout->addWidget(homeTolerance,1,1);homeLayout->addWidget(setHome,2,0,1,2);homeLayout->addWidget(clearHome,2,2);homeLayout->addWidget(homeInfo,3,0,1,3);
    connect(autoHome,&QCheckBox::toggled,this,[this](bool on){auto p=c_->profile();p.mount.autoHomeSync=on;c_->setProfile(p);appendLog(QString("Automatic Home coordinate restore %1").arg(on?"enabled":"disabled"));});
    connect(homeTolerance,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double v){auto p=c_->profile();if(std::abs(p.mount.homeToleranceDeg-v)<1e-9)return;p.mount.homeToleranceDeg=v;c_->setProfile(p);});
    connect(setHome,&QPushButton::clicked,this,[this,autoHome](){MountStatus st;QString e;if(!c_->mountStatus(st,&e)){showError(e);return;}if(!st.axes.valid){showError("This backend does not expose mechanical axis coordinates");return;}auto p=c_->profile();p.mount.homeAxis1Deg=st.axes.axis1Deg;p.mount.homeAxis2Deg=st.axes.axis2Deg;p.mount.customHome=true;p.mount.autoHomeSync=true;p.mount.parkAxis1Deg=st.axes.axis1Deg;p.mount.parkAxis2Deg=st.axes.axis2Deg;p.mount.customPark=true;c_->setProfile(p);{QSignalBlocker b(autoHome);autoHome->setChecked(true);}appendLog(QString("Mechanical Home + native Park saved together: axis1=%1°, axis2=%2°; automatic Home restore enabled for future connects").arg(st.axes.axis1Deg,0,'f',4).arg(st.axes.axis2Deg,0,'f',4));});
    connect(clearHome,&QPushButton::clicked,this,[this,autoHome](){auto p=c_->profile();p.mount.customHome=false;p.mount.autoHomeSync=false;c_->setProfile(p);{QSignalBlocker b(autoHome);autoHome->setChecked(false);}appendLog("Mechanical Home calibration cleared; automatic Home restore disabled");});
    l->addWidget(homeBox);

    auto*orientationBox=new QGroupBox("Native raw-axis orientation calibration");auto*orientationLayout=new QVBoxLayout(orientationBox);auto*orientationInfo=new QLabel("These signs apply ONLY to native raw-axis EQDrive/SynScan geometry. Classic ASCOM owns its own motor/sky mapping, so changing these signs cannot fix an ASCOM East/West error. After a correct Home/Sync, test small native offsets first.");orientationInfo->setWordWrap(true);orientationLayout->addWidget(orientationInfo);auto*reverseA1=new QPushButton("Reverse Axis 1 mapping / tracking direction");auto*reverseA2=new QPushButton("Reverse Axis 2 mapping");auto nativeOnly=[this](){MountStatus st;QString e;if(c_->mountStatus(st,&e)&&st.geometryType=="ascom-classic"){showError("Axis sign mapping applies only to native raw-axis mounts. ASCOM/EQMOD owns its own axis mapping; fix site/time or the ASCOM driver configuration instead.");return false;}return true;};connect(reverseA1,&QPushButton::clicked,this,[this,nativeOnly](){if(!nativeOnly())return;auto p=c_->profile();p.mount.axis1Sign=p.mount.axis1Sign>=0?-1:1;c_->setProfile(p);appendLog(QString("Axis 1 mapping reversed to %1; native sky model will be restored from Home or refined by Sync").arg(p.mount.axis1Sign));});connect(reverseA2,&QPushButton::clicked,this,[this,nativeOnly](){if(!nativeOnly())return;auto p=c_->profile();p.mount.axis2Sign=p.mount.axis2Sign>=0?-1:1;c_->setProfile(p);appendLog(QString("Axis 2 mapping reversed to %1; native sky model will be restored from Home or refined by Sync").arg(p.mount.axis2Sign));});orientationLayout->addWidget(reverseA1);orientationLayout->addWidget(reverseA2);l->addWidget(orientationBox);
    auto*setPark=new QPushButton("Calibrate current physical pose as persistent Home / Park");connect(setPark,&QPushButton::clicked,this,[this](){QString e;if(!c_->setCurrentMountAsPark(&e)){showError(e);return;}appendLog("Persistent park calibration stored for the active backend. To make native and ASCOM identical, calibrate each backend once at this same physical pose without moving the mount between them.");refreshMountStatus();});auto*defaultPark=new QPushButton("Clear native OAL Park calibration");connect(defaultPark,&QPushButton::clicked,this,[this](){auto p=c_->profile();p.mount.customPark=false;c_->setProfile(p);appendLog("Native OAL Park calibration cleared. ASCOM driver park, if configured, is owned by that driver and is not erased by this button.");});l->addWidget(setPark);l->addWidget(defaultPark);
    auto*gs=new QPushButton("Set guide target from last solve");auto*gu=new QPushButton("Guide using last solve");auto*gx=new QPushButton("Stop guiding");connect(gs,&QPushButton::clicked,this,[this](){c_->startGuiding();});connect(gu,&QPushButton::clicked,this,[this](){c_->guideUsingLastSolve();});connect(gx,&QPushButton::clicked,this,[this](){c_->stopGuiding();});l->addWidget(gs);l->addWidget(gu);l->addWidget(gx);l->addStretch();refreshMountStatus();return w;
}

void MainWindow::synchronizeMountCoordinatesFrom(const QString &system){
    if(mountCoordinateSyncing_||!mountRa_||!mountDec_||!mountJNowRa_||!mountJNowDec_||!mountAz_||!mountAlt_||!mountGalL_||!mountGalB_)return;
    mountCoordinateSyncing_=true;const QDateTime utc=(mountUseSystemUtc_&&mountUseSystemUtc_->isChecked())?QDateTime::currentDateTimeUtc():(mountConversionUtc_?mountConversionUtc_->dateTime().toUTC():QDateTime::currentDateTimeUtc());auto observer=c_->profile().observer;if(mountSiteLat_){observer.latitudeDeg=mountSiteLat_->value();observer.longitudeDeg=mountSiteLon_->value();observer.elevationM=mountSiteElevation_->value();}EquatorialCoord j2000;
    if(system=="jnow")j2000=convertEquatorialFrame({mountJNowRa_->value(),mountJNowDec_->value(),EquatorialFrame::JNow},EquatorialFrame::J2000,utc);
    else if(system=="horizontal")j2000=horizontalToEquatorial({mountAz_->value(),mountAlt_->value()},observer,EquatorialFrame::J2000,utc);
    else if(system=="galactic")j2000=galacticToEquatorial({mountGalL_->value(),mountGalB_->value()},EquatorialFrame::J2000,utc);
    else j2000={mountRa_->value(),mountDec_->value(),EquatorialFrame::J2000};
    const auto jnow=convertEquatorialFrame(j2000,EquatorialFrame::JNow,utc);const auto hor=equatorialToHorizontal(j2000,observer,utc);const auto gal=equatorialToGalactic(j2000,utc);
    {QSignalBlocker a(mountRa_),b(mountDec_),c(mountJNowRa_),d(mountJNowDec_),e(mountAz_),f(mountAlt_),g(mountGalL_),h(mountGalB_);mountRa_->setValue(j2000.raDeg);mountDec_->setValue(j2000.decDeg);mountJNowRa_->setValue(jnow.raDeg);mountJNowDec_->setValue(jnow.decDeg);mountAz_->setValue(hor.azDeg);mountAlt_->setValue(hor.altDeg);mountGalL_->setValue(gal.lDeg);mountGalB_->setValue(gal.bDeg);}
    if(mountCoordEpoch_){const bool siteUnset=std::abs(observer.latitudeDeg)<1e-12&&std::abs(observer.longitudeDeg)<1e-12;mountCoordEpoch_->setText(QString("Conversions at %1 UTC; site lat=%2°, lon=%3°. Horizontal coordinates are time/location dependent.%4").arg(utc.toString("yyyy-MM-dd HH:mm:ss")).arg(observer.latitudeDeg,0,'f',5).arg(observer.longitudeDeg,0,'f',5).arg(siteUnset?" WARNING: observatory location is still 0,0.":""));}
    mountCoordinateSyncing_=false;
}

QWidget *MainWindow::buildFocusTab(){
    auto*w=new QWidget;auto*l=new QVBoxLayout(w);auto*f=new QFormLayout;focusPosition_=ispin(0,200000,20000);connect(focusPosition_,qOverload<int>(&QSpinBox::valueChanged),this,[this](int){focusTargetDirty_=true;});focusMode_=new QComboBox;focusMode_->addItems({"stars","scene","planet","bahtinov"});focusRange_=ispin(100,50000,1600);coarseStep_=ispin(1,10000,200);fineStep_=ispin(1,5000,40);focusFrames_=ispin(1,30,3);focusExposure_=dspin(0.0001,30.0,0.05,4);focusGain_=ispin(0,102400,100);focusMinStars_=ispin(1,100,3);
    f->addRow("Target position",focusPosition_);f->addRow("AF mode",focusMode_);f->addRow("AF starting exposure, s (Scene auto-meters)",focusExposure_);f->addRow("AF gain",focusGain_);f->addRow("Minimum stars (star mode)",focusMinStars_);f->addRow("Range",focusRange_);f->addRow("Coarse step",coarseStep_);f->addRow("Fine step",fineStep_);f->addRow("Frames / point",focusFrames_);l->addLayout(f);
    focuserStatus_=new QLabel("Focuser status: unavailable");focuserStatus_->setWordWrap(true);l->addWidget(focuserStatus_);
    auto*refresh=new QPushButton("Refresh focuser status");connect(refresh,&QPushButton::clicked,this,&MainWindow::refreshFocuserStatus);l->addWidget(refresh);
    auto*jogBox=new QGroupBox("Manual focus jog");auto*jogLayout=new QHBoxLayout(jogBox);focusJogStep_=ispin(1,10000,50);jogLayout->addWidget(new QLabel("Step"));jogLayout->addWidget(focusJogStep_);
    auto*jogMinusCoarse=new QPushButton("−−");auto*jogMinus=new QPushButton("−");auto*jogStop=new QPushButton("STOP");auto*jogPlus=new QPushButton("+");auto*jogPlusCoarse=new QPushButton("++");
    auto jog=[this](int multiplier){if(autofocusBusy_){showError("Cancel autofocus before manual focus jog");return;}FocuserStatus st;QString e;if(!c_->focuserStatus(st,&e)){showError(e);return;}const int delta=multiplier*(focusJogStep_?focusJogStep_->value():50);const int target=std::max(0,st.position+delta);if(!c_->moveFocuser(target,&e)){showError(e);return;}appendLog(QString("Focus jog: %1 -> %2 (%3%4)").arg(st.position).arg(target).arg(delta>=0?"+":"").arg(delta));if(focusMotionPollTimer_)focusMotionPollTimer_->start();};
    connect(jogMinusCoarse,&QPushButton::clicked,this,[jog](){jog(-10);});connect(jogMinus,&QPushButton::clicked,this,[jog](){jog(-1);});connect(jogPlus,&QPushButton::clicked,this,[jog](){jog(+1);});connect(jogPlusCoarse,&QPushButton::clicked,this,[jog](){jog(+10);});connect(jogStop,&QPushButton::clicked,this,[this](){QString e;if(!c_->haltFocuser(&e))showError(e);else appendLog("Manual focuser STOP");});
    for(auto*b:{jogMinusCoarse,jogMinus,jogStop,jogPlus,jogPlusCoarse}){b->setMinimumHeight(34);jogLayout->addWidget(b);}l->addWidget(jogBox);
    focusMoveButton_=new QPushButton("Move focuser");connect(focusMoveButton_,&QPushButton::clicked,this,[this](){QString e;const int p=focusPosition_->value();if(!c_->moveFocuser(p,&e))showError(e);else{focusTargetInitialized_=true;focusTargetDirty_=false;appendLog(QString("Focuser move accepted: %1").arg(p));refreshFocuserStatus();if(focusMotionPollTimer_)focusMotionPollTimer_->start();}});
    focusHaltButton_=new QPushButton("HALT focuser");connect(focusHaltButton_,&QPushButton::clicked,this,[this](){QString e;if(!c_->haltFocuser(&e))showError(e);else{appendLog("Focuser halt accepted");refreshFocuserStatus();}});
    focusMotionPollTimer_=new QTimer(this);focusMotionPollTimer_->setInterval(200);connect(focusMotionPollTimer_,&QTimer::timeout,this,[this](){if(!focuserStatus_)return;FocuserStatus st;QString e;if(!c_->focuserStatus(st,&e)){focusMotionPollTimer_->stop();return;}QString t=QString("Actual position: %1   Moving: %2").arg(st.position).arg(st.moving?"YES":"NO");if(st.temperatureC)t+=QString("   Temperature: %1 °C").arg(*st.temperatureC,0,'f',1);focuserStatus_->setText(t);if(focusPosition_&&!focusTargetDirty_){QSignalBlocker b(focusPosition_);focusPosition_->setValue(st.position);focusTargetInitialized_=true;}if(!st.moving)focusMotionPollTimer_->stop();});
    autofocusButton_=new QPushButton("Run autofocus");connect(autofocusButton_,&QPushButton::clicked,this,[this](){if(liveViewBusy_){showError("Stop Live View before starting autofocus; both operations require the main-camera lock.");return;}if(autofocusBusy_&&!autofocusOperationId_.isEmpty()){QString e;if(!c_->cancelOperation(autofocusOperationId_,&e))showError(e);else appendLog("Autofocus cancellation requested");return;}AutofocusRequest r;r.mode=focusMode_->currentText()=="scene"?AutofocusMode::Scene:focusMode_->currentText()=="planet"?AutofocusMode::Planet:focusMode_->currentText()=="bahtinov"?AutofocusMode::Bahtinov:AutofocusMode::Stars;r.rangeSteps=focusRange_->value();r.coarseStep=coarseStep_->value();r.fineStep=fineStep_->value();r.framesPerPosition=focusFrames_->value();r.exposureSec=focusExposure_->value();r.gain=focusGain_->value();r.minStars=focusMinStars_->value();QString e;auto id=c_->startAutofocus(r,&e);if(id.isEmpty()){showError(e);return;}autofocusOperationId_=id;setAutofocusBusy(true);appendLog("Autofocus operation started: "+id+" — camera and focuser locked");});
    l->addWidget(focusMoveButton_);l->addWidget(focusHaltButton_);l->addWidget(autofocusButton_);l->addStretch();return w;
}
QWidget *MainWindow::buildPolarTab(){
    auto*w=new QWidget;auto*l=new QVBoxLayout(w);const auto profile=c_->profile();
    auto*safe=new QGroupBox("Optional Polar-alignment safe sky region");auto*sf=new QFormLayout(safe);polarSafeEnabled_=new QCheckBox("Restrict Polar Alignment motion to this safe sky region");polarSafeEnabled_->setChecked(profile.polarMotionLimits.enabled);polarMinAz_=dspin(0,360,profile.polarMotionLimits.minAzDeg,1);polarMaxAz_=dspin(0,360,profile.polarMotionLimits.maxAzDeg,1);polarMinAlt_=dspin(-10,90,profile.polarMotionLimits.minAltDeg,1);polarMaxAlt_=dspin(-10,90,profile.polarMotionLimits.maxAltDeg,1);sf->addRow(polarSafeEnabled_);sf->addRow("Minimum azimuth, deg",polarMinAz_);sf->addRow("Maximum azimuth, deg",polarMaxAz_);sf->addRow("Minimum altitude, deg",polarMinAlt_);sf->addRow("Maximum altitude, deg",polarMaxAlt_);auto*apply=new QPushButton("Apply safe region");sf->addRow(apply);auto*safeNote=new QLabel("Optional. Leave this unchecked for an unrestricted mount/sky. Enable it for balconies, roofs, walls or other obstructions. Azimuth min > max means a wrapped interval through north (for example 300..60). When enabled, OAL samples the complete requested RA slew path, not only the destination, and rejects motion if any intermediate point leaves the allowed region.");safeNote->setWordWrap(true);sf->addRow(safeNote);l->addWidget(safe);
    auto updatePolarSafeControls=[this](){const bool on=polarSafeEnabled_->isChecked();polarMinAz_->setEnabled(on);polarMaxAz_->setEnabled(on);polarMinAlt_->setEnabled(on);polarMaxAlt_->setEnabled(on);};updatePolarSafeControls();connect(polarSafeEnabled_,&QCheckBox::toggled,this,[updatePolarSafeControls](bool){updatePolarSafeControls();});
    connect(apply,&QPushButton::clicked,this,[this](){if(polarMinAlt_->value()>polarMaxAlt_->value()){showError("Polar safe-region minimum altitude must not exceed maximum altitude");return;}auto p=c_->profile();p.polarMotionLimits.enabled=polarSafeEnabled_->isChecked();p.polarMotionLimits.minAzDeg=polarMinAz_->value();p.polarMotionLimits.maxAzDeg=polarMaxAz_->value();p.polarMotionLimits.minAltDeg=polarMinAlt_->value();p.polarMotionLimits.maxAltDeg=polarMaxAlt_->value();c_->setProfile(p);appendLog(QString("Polar safe region saved: %1 Az=%2..%3 Alt=%4..%5").arg(p.polarMotionLimits.enabled?"ON":"OFF").arg(p.polarMotionLimits.minAzDeg).arg(p.polarMotionLimits.maxAzDeg).arg(p.polarMotionLimits.minAltDeg).arg(p.polarMotionLimits.maxAltDeg));});
    polarStep_=dspin(-60,60,15,2);polarExposure_=dspin(0.001,30.0,1.0,3);polarAutoSamples_=ispin(2,5,3);polarReturnToStart_=new QCheckBox("Return to starting sky coordinate after measurement");polarSamples_=new QLabel("Samples: 0");polarResult_=new QLabel("No estimate");polarResult_->setWordWrap(true);
    auto*autoBox=new QGroupBox("Automatic Polar Alignment");auto*af=new QFormLayout(autoBox);af->addRow("RA step between solves, deg",polarStep_);af->addRow("Plate-solve exposure, s",polarExposure_);af->addRow("Solved positions",polarAutoSamples_);af->addRow(polarReturnToStart_);auto*runAuto=new QPushButton("Run automatic alignment");af->addRow(runAuto);auto*autoNote=new QLabel("Works with or without a safe-region constraint. OAL performs capture/solve → RA slew → capture/solve (2–5 positions) and estimates the polar-axis error. If the optional safe sky region is enabled, the complete sequence is preflighted and any path that leaves the allowed Az/Alt region is rejected before motion.");autoNote->setWordWrap(true);af->addRow(autoNote);l->addWidget(autoBox);
    connect(runAuto,&QPushButton::clicked,this,[this](){PolarAlignmentRunRequest r;r.raStepDeg=polarStep_->value();r.sampleCount=polarAutoSamples_->value();r.exposure.exposureSec=polarExposure_->value();r.exposure.binX=2;r.exposure.binY=2;r.searchRadiusDeg=20.0;r.returnToStart=polarReturnToStart_->isChecked();QString e;const QString id=c_->startPolarAlignment(r,&e);if(id.isEmpty())showError(e);else appendLog("Automatic Polar Alignment started: "+id);});
    auto*manual=new QGroupBox("Manual / diagnostic samples");auto*ml=new QVBoxLayout(manual);auto*clear=new QPushButton("Clear samples");auto*add=new QPushButton("Add last solved field");auto*slew=new QPushButton("Slew RA by step (safe-region aware)");auto*estimate=new QPushButton("Estimate RA axis");connect(clear,&QPushButton::clicked,c_,&ObservatoryController::clearPolarSamples);connect(add,&QPushButton::clicked,this,[this](){QString e;if(!c_->addPolarSample(&e))showError(e);});connect(slew,&QPushButton::clicked,this,[this](){QString e;if(!c_->slewPolarRaOffset(polarStep_->value(),&e))showError(e);else appendLog(QString("Polar-alignment RA slew accepted: %1 deg").arg(polarStep_->value()));});connect(estimate,&QPushButton::clicked,this,[this](){auto r=c_->estimatePolarAlignment();polarResult_->setText(QString("%1\nAxis RA %2°, DEC %3°\nTotal %4′\nAdjust altitude %5′, azimuth %6′").arg(r.message).arg(r.axisRaDeg).arg(r.axisDecDeg).arg(r.totalErrorArcmin).arg(r.altitudeAdjustmentArcmin).arg(r.azimuthAdjustmentArcmin));});ml->addWidget(new QLabel("Manual workflow: solve current field → Add sample → RA slew → solve → Add sample. When the optional safe region is enabled the complete slew path is constrained. Use at least two, preferably three positions."));ml->addWidget(clear);ml->addWidget(add);ml->addWidget(slew);ml->addWidget(estimate);l->addWidget(manual);l->addWidget(polarSamples_);l->addWidget(polarResult_);l->addStretch();return w;
}
QWidget *MainWindow::buildSchedulerTab(){
    auto*w=new QWidget;auto*l=new QVBoxLayout(w);
    auto*dsoBox=new QGroupBox("DSO FITS / RAW settings (also used per mosaic tile)");auto*f=new QFormLayout(dsoBox);
    targetName_=new QLineEdit("M42");targetRa_=dspin(0,360,83.8221,6);targetDec_=dspin(-90,90,-5.3911,6);auto*useCurrent=new QPushButton("Use current telescope pointing (J2000)");
    targetExposure_=dspin(0.001,3600,30,3);targetRepeats_=ispin(1,10000,10);targetRecenterBefore_=new QCheckBox("Solve/recenter before first science frame");targetRecenterBefore_->setChecked(true);targetRecenterEvery_=ispin(0,10000,0);targetRecenterTolerance_=dspin(0.1,120.0,2.0,2);targetAutofocusBefore_=new QCheckBox("Autofocus before first science frame");targetAutofocusBefore_->setChecked(true);targetAutofocusEvery_=ispin(0,10000,0);
    f->addRow("Name",targetName_);f->addRow("RA J2000",targetRa_);f->addRow("DEC J2000",targetDec_);f->addRow(useCurrent);f->addRow("Science exposure, s",targetExposure_);f->addRow("Science frames",targetRepeats_);f->addRow(targetRecenterBefore_);f->addRow("Recenter every N frames (0=off)",targetRecenterEvery_);f->addRow("Recenter tolerance, arcmin",targetRecenterTolerance_);f->addRow(targetAutofocusBefore_);f->addRow("Autofocus every N frames (0=off)",targetAutofocusEvery_);l->addWidget(dsoBox);

    auto*planetBox=new QGroupBox("Planetary high-speed SER settings");auto*pf=new QFormLayout(planetBox);
    planetExposure_=dspin(0.00005,10.0,0.005,6);planetGain_=ispin(0,10000,100);planetFps_=dspin(0.2,500.0,100.0,1);planetDuration_=dspin(0.5,86400.0,120.0,1);planetRuns_=ispin(1,10000,5);planetRoiW_=ispin(0,10000,640);planetRoiH_=ispin(0,10000,480);planetAutofocusBefore_=new QCheckBox("Planetary autofocus before first SER");planetAutofocusBefore_->setChecked(true);planetAutofocusEvery_=ispin(0,10000,0);planetRoiTracking_=new QCheckBox("Track target by moving hardware ROI");planetRoiTracking_->setChecked(true);planetMountCorrections_=new QCheckBox("Allow calibrated mount corrections when ROI drifts far from sensor centre (HIL-sensitive)");planetMountCorrections_->setChecked(false);planetTrackingRate_=new QComboBox;planetTrackingRate_->addItems(QStringList{"sidereal","lunar","solar"});
    pf->addRow("Exposure, s",planetExposure_);pf->addRow("Gain",planetGain_);pf->addRow("Target FPS",planetFps_);pf->addRow("SER duration, s",planetDuration_);pf->addRow("SER runs",planetRuns_);pf->addRow("ROI width (0=auto)",planetRoiW_);pf->addRow("ROI height (0=auto)",planetRoiH_);pf->addRow("Tracking rate",planetTrackingRate_);pf->addRow(planetAutofocusBefore_);pf->addRow("Autofocus every N SER runs (0=off)",planetAutofocusEvery_);pf->addRow(planetRoiTracking_);pf->addRow(planetMountCorrections_);l->addWidget(planetBox);

    auto*mosaicBox=new QGroupBox("Mosaic FITS block");auto*mf=new QFormLayout(mosaicBox);mosaicCols_=ispin(1,100,3);mosaicRows_=ispin(1,100,2);mosaicOverlap_=dspin(0,90,15,1);mosaicRotation_=dspin(-180,180,0,1);mosaicSerpentine_=new QCheckBox("Serpentine tile order (shorter slews)");mosaicSerpentine_->setChecked(true);mosaicRecenterEach_=new QCheckBox("Solve/recenter at the start of every tile");mosaicRecenterEach_->setChecked(true);mosaicAutofocusEach_=new QCheckBox("Autofocus at the start of every tile");mosaicAutofocusEach_->setChecked(false);mf->addRow("Columns",mosaicCols_);mf->addRow("Rows",mosaicRows_);mf->addRow("Overlap, %",mosaicOverlap_);mf->addRow("Sensor/grid rotation, deg",mosaicRotation_);mf->addRow(mosaicSerpentine_);mf->addRow(mosaicRecenterEach_);mf->addRow(mosaicAutofocusEach_);l->addWidget(mosaicBox);

    auto*timingBox=new QGroupBox("Selected block calendar / inter-block policy");auto*tf=new QFormLayout(timingBox);blockStartMode_=new QComboBox;blockStartMode_->addItems({"After previous / when calendar is armed","At date/time"});blockStartAt_=new QDateTimeEdit(QDateTime::currentDateTime().addSecs(60));blockStartAt_->setCalendarPopup(true);blockStartAt_->setDisplayFormat("yyyy-MM-dd HH:mm:ss t");blockStartAt_->setEnabled(false);blockParkAfter_=new QCheckBox("Park mount after this block while waiting for the next event");tf->addRow("Start",blockStartMode_);tf->addRow("Date/time",blockStartAt_);tf->addRow(blockParkAfter_);l->addWidget(timingBox);connect(blockStartMode_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int i){blockStartAt_->setEnabled(i==1);if(i==1&&blockStartAt_->dateTime()<=QDateTime::currentDateTime())blockStartAt_->setDateTime(QDateTime::currentDateTime().addSecs(60));});

    auto*note=new QLabel("v0.2.10.50 calendar scheduler: every block has its own optional UTC start time and park-after policy. The calendar is persisted on the node and can contain events months ahead. Mosaic tile centres are derived from the optical profile/main-sensor FOV and overlap. A running exposure is not resumed mid-frame after a crash; the calendar itself is restored.");note->setWordWrap(true);l->addWidget(note);
    targetList_=new QListWidget;l->addWidget(targetList_);

    auto applyCommon=[this](ObservationBlock &b){b.name=targetName_->text();b.coordinate={targetRa_->value(),targetDec_->value(),EquatorialFrame::J2000};b.startAtUtc=blockStartMode_->currentIndex()==1?blockStartAt_->dateTime().toUTC():QDateTime{};b.parkAfter=blockParkAfter_->isChecked();b.autoUnparkBefore=true;};
    auto fillDso=[this](DsoFitsBlock &d){d.exposure.exposureSec=targetExposure_->value();d.exposure.saveRaw=true;d.frameCount=targetRepeats_->value();d.recenter.beforeFirstFrame=targetRecenterBefore_->isChecked();d.recenter.everyNFrames=targetRecenterEvery_->value();d.recenter.toleranceArcmin=targetRecenterTolerance_->value();d.autofocus.beforeFirstFrame=targetAutofocusBefore_->isChecked();d.autofocus.everyNFrames=targetAutofocusEvery_->value();};
    auto makeDso=[applyCommon,fillDso](){ObservationBlock b;applyCommon(b);b.mode=ObservationMode::DsoFits;fillDso(b.dso);return b;};
    auto makePlanet=[this,applyCommon](){ObservationBlock b;applyCommon(b);b.mode=ObservationMode::PlanetarySer;b.planetary.stream.exposureSec=planetExposure_->value();b.planetary.stream.gain=planetGain_->value();b.planetary.stream.targetFps=planetFps_->value();b.planetary.serRuns=planetRuns_->value();b.planetary.durationSec=planetDuration_->value();b.planetary.roiWidth=planetRoiW_->value();b.planetary.roiHeight=planetRoiH_->value();b.planetary.autofocus.beforeFirstRun=planetAutofocusBefore_->isChecked();b.planetary.autofocus.everyNRuns=planetAutofocusEvery_->value();b.planetary.autofocus.request.mode=AutofocusMode::Planet;b.planetary.autofocus.request.exposureSec=planetExposure_->value();b.planetary.autofocus.request.gain=planetGain_->value();b.planetary.tracking.allowRoiShift=planetRoiTracking_->isChecked();b.planetary.tracking.mountCorrections=planetMountCorrections_->isChecked();b.planetary.trackingRate=trackingRateFromString(planetTrackingRate_->currentText());return b;};
    auto makeMosaic=[this,applyCommon,fillDso](){ObservationBlock b;applyCommon(b);b.mode=ObservationMode::MosaicFits;b.mosaic.columns=mosaicCols_->value();b.mosaic.rows=mosaicRows_->value();b.mosaic.overlapPercent=mosaicOverlap_->value();b.mosaic.rotationDeg=mosaicRotation_->value();b.mosaic.serpentine=mosaicSerpentine_->isChecked();b.mosaic.recenterEachTile=mosaicRecenterEach_->isChecked();b.mosaic.autofocusEachTile=mosaicAutofocusEach_->isChecked();fillDso(b.mosaic.tile);return b;};
    auto formatBlock=[](const ObservationBlock&b){const QString when=b.startAtUtc.isValid()?b.startAtUtc.toLocalTime().toString("yyyy-MM-dd HH:mm"):"after previous";const QString park=b.parkAfter?" PARK":"";if(b.mode==ObservationMode::DsoFits)return QString("%1  DSO %2 RA %3 DEC %4  %5s x%6%7").arg(when,b.name).arg(b.coordinate.raDeg).arg(b.coordinate.decDeg).arg(b.dso.exposure.exposureSec).arg(b.dso.frameCount).arg(park);if(b.mode==ObservationMode::MosaicFits)return QString("%1  MOSAIC %2 %3x%4 overlap %5%  tile %6s x%7%8").arg(when,b.name).arg(b.mosaic.columns).arg(b.mosaic.rows).arg(b.mosaic.overlapPercent).arg(b.mosaic.tile.exposure.exposureSec).arg(b.mosaic.tile.frameCount).arg(park);return QString("%1  SER %2 %3s @%4fps x%5 ROI %6x%7%8").arg(when,b.name).arg(b.planetary.durationSec).arg(b.planetary.stream.targetFps).arg(b.planetary.serRuns).arg(b.planetary.roiWidth).arg(b.planetary.roiHeight).arg(park);};
    auto refreshList=[this,formatBlock](){const int row=targetList_->currentRow();targetList_->clear();for(const auto&b:targets_)targetList_->addItem(formatBlock(b));if(!targets_.empty())targetList_->setCurrentRow(std::clamp(row,0,int(targets_.size())-1));};
    auto loadDso=[this](const DsoFitsBlock&d){targetExposure_->setValue(d.exposure.exposureSec);targetRepeats_->setValue(d.frameCount);targetRecenterBefore_->setChecked(d.recenter.beforeFirstFrame);targetRecenterEvery_->setValue(d.recenter.everyNFrames);targetRecenterTolerance_->setValue(d.recenter.toleranceArcmin);targetAutofocusBefore_->setChecked(d.autofocus.beforeFirstFrame);targetAutofocusEvery_->setValue(d.autofocus.everyNFrames);};
    auto loadBlock=[this,loadDso](const ObservationBlock&b){targetName_->setText(b.name);targetRa_->setValue(b.coordinate.raDeg);targetDec_->setValue(b.coordinate.decDeg);blockStartMode_->setCurrentIndex(b.startAtUtc.isValid()?1:0);if(b.startAtUtc.isValid())blockStartAt_->setDateTime(b.startAtUtc.toLocalTime());blockParkAfter_->setChecked(b.parkAfter);if(b.mode==ObservationMode::DsoFits)loadDso(b.dso);else if(b.mode==ObservationMode::MosaicFits){loadDso(b.mosaic.tile);mosaicCols_->setValue(b.mosaic.columns);mosaicRows_->setValue(b.mosaic.rows);mosaicOverlap_->setValue(b.mosaic.overlapPercent);mosaicRotation_->setValue(b.mosaic.rotationDeg);mosaicSerpentine_->setChecked(b.mosaic.serpentine);mosaicRecenterEach_->setChecked(b.mosaic.recenterEachTile);mosaicAutofocusEach_->setChecked(b.mosaic.autofocusEachTile);}else{planetExposure_->setValue(b.planetary.stream.exposureSec);planetGain_->setValue(b.planetary.stream.gain);planetFps_->setValue(b.planetary.stream.targetFps);planetDuration_->setValue(b.planetary.durationSec);planetRuns_->setValue(b.planetary.serRuns);planetRoiW_->setValue(b.planetary.roiWidth);planetRoiH_->setValue(b.planetary.roiHeight);planetAutofocusBefore_->setChecked(b.planetary.autofocus.beforeFirstRun);planetAutofocusEvery_->setValue(b.planetary.autofocus.everyNRuns);planetRoiTracking_->setChecked(b.planetary.tracking.allowRoiShift);planetMountCorrections_->setChecked(b.planetary.tracking.mountCorrections);planetTrackingRate_->setCurrentText(trackingRateName(b.planetary.trackingRate));}};
    connect(targetList_,&QListWidget::currentRowChanged,this,[this,loadBlock](int row){if(row>=0&&row<int(targets_.size()))loadBlock(targets_[size_t(row)]);});
    connect(useCurrent,&QPushButton::clicked,this,[this](){MountStatus st;QString e;if(!c_->mountStatus(st,&e)||!st.coordinateValid){showError(e.isEmpty()?"Mount does not currently report a valid sky coordinate":e);return;}const auto j=convertEquatorialFrame(st.coordinate,EquatorialFrame::J2000);targetRa_->setValue(j.raDeg);targetDec_->setValue(j.decDeg);appendLog(QString("Scheduler target set from current mount pointing: RA=%1 DEC=%2 J2000").arg(j.raDeg,0,'f',6).arg(j.decDeg,0,'f',6));});

    auto*buttons=new QGridLayout;auto*addDso=new QPushButton("Add DSO");auto*addPlanet=new QPushButton("Add planetary SER");auto*addMosaic=new QPushButton("Add mosaic");auto*update=new QPushButton("Update selected");auto*del=new QPushButton("Delete selected");auto*clear=new QPushButton("Clear calendar");auto*up=new QPushButton("Move up");auto*down=new QPushButton("Move down");auto*save=new QPushButton("Save calendar on node");auto*startPlan=new QPushButton("Arm / start calendar");auto*stop=new QPushButton("Stop calendar");buttons->addWidget(addDso,0,0);buttons->addWidget(addPlanet,0,1);buttons->addWidget(addMosaic,0,2);buttons->addWidget(update,1,0);buttons->addWidget(del,1,1);buttons->addWidget(clear,1,2);buttons->addWidget(up,2,0);buttons->addWidget(down,2,1);buttons->addWidget(save,3,0,1,3);buttons->addWidget(startPlan,4,0,1,2);buttons->addWidget(stop,4,2);l->addLayout(buttons);
    connect(addDso,&QPushButton::clicked,this,[this,makeDso,refreshList](){targets_.push_back(makeDso());refreshList();targetList_->setCurrentRow(int(targets_.size())-1);});connect(addPlanet,&QPushButton::clicked,this,[this,makePlanet,refreshList](){targets_.push_back(makePlanet());refreshList();targetList_->setCurrentRow(int(targets_.size())-1);});connect(addMosaic,&QPushButton::clicked,this,[this,makeMosaic,refreshList](){targets_.push_back(makeMosaic());refreshList();targetList_->setCurrentRow(int(targets_.size())-1);});
    connect(update,&QPushButton::clicked,this,[this,makeDso,makePlanet,makeMosaic,refreshList](){const int row=targetList_->currentRow();if(row<0||row>=int(targets_.size())){showError("Select a scheduler block to update");return;}const auto old=targets_[size_t(row)];ObservationBlock b=old.mode==ObservationMode::DsoFits?makeDso():old.mode==ObservationMode::MosaicFits?makeMosaic():makePlanet();b.id=old.id;targets_[size_t(row)]=b;refreshList();targetList_->setCurrentRow(row);});
    connect(del,&QPushButton::clicked,this,[this,refreshList](){const int row=targetList_->currentRow();if(row<0||row>=int(targets_.size()))return;targets_.erase(targets_.begin()+row);refreshList();});connect(clear,&QPushButton::clicked,this,[this,refreshList](){targets_.clear();refreshList();});connect(up,&QPushButton::clicked,this,[this,refreshList](){const int row=targetList_->currentRow();if(row<=0||row>=int(targets_.size()))return;std::swap(targets_[size_t(row)],targets_[size_t(row-1)]);refreshList();targetList_->setCurrentRow(row-1);});connect(down,&QPushButton::clicked,this,[this,refreshList](){const int row=targetList_->currentRow();if(row<0||row+1>=int(targets_.size()))return;std::swap(targets_[size_t(row)],targets_[size_t(row+1)]);refreshList();targetList_->setCurrentRow(row+1);});
    auto saveCalendar=[this](){ObservationPlan p;p.name="GUI observing calendar";p.blocks=targets_;QString e;if(!c_->setObservationPlan(p,&e)){showError(e.isEmpty()?"Add at least one observation block":e);return false;}appendLog(QString("Observation calendar saved on node: %1 block(s)").arg(targets_.size()));return true;};connect(save,&QPushButton::clicked,this,[saveCalendar](){saveCalendar();});connect(startPlan,&QPushButton::clicked,this,[this,saveCalendar](){if(!saveCalendar())return;QString e;if(!c_->startSession(&e))showError(e);else appendLog("Observation calendar armed; each block will honor its own start time");});connect(stop,&QPushButton::clicked,this,[this](){c_->stopSession();});
    targets_=c_->observationPlan().blocks;refreshList();return w;
}
QWidget *MainWindow::buildOperationsTab(){auto*w=new QWidget;auto*l=new QVBoxLayout(w);operationsList_=new QListWidget;l->addWidget(operationsList_);auto*refresh=new QPushButton("Refresh operations");cancelOperationButton_=new QPushButton("Cancel selected operation");connect(refresh,&QPushButton::clicked,this,[this](){for(const auto&v:c_->operations(false))updateOperation(v.toObject());});connect(cancelOperationButton_,&QPushButton::clicked,this,[this](){auto*item=operationsList_?operationsList_->currentItem():nullptr;if(!item)return;QString e;const QString id=item->data(Qt::UserRole).toString();if(!c_->cancelOperation(id,&e))showError(e);});l->addWidget(refresh);l->addWidget(cancelOperationButton_);l->addStretch();return w;}
QWidget *MainWindow::buildServerTab(){auto*w=new QWidget;auto*l=new QVBoxLayout(w);
    if(c_->isRemote()){auto*info=new QLabel(QString("This GUI is a thin client. Observatory algorithms and hardware control execute on:\n%1\n\nThe node service owns its HTTP/WebSocket lifecycle. Stellarium bridge settings below are applied remotely to that node.").arg(c_->endpointDescription()));info->setWordWrap(true);l->addWidget(info);}
    else {serverEnabled_=new QCheckBox("Enable OAL HTTP server");serverPort_=ispin(1,65535,8080);wsEnabled_=new QCheckBox("Enable event WebSocket");wsPort_=ispin(1,65535,8090);auto*f=new QFormLayout;f->addRow(serverEnabled_);f->addRow("HTTP port",serverPort_);f->addRow(wsEnabled_);f->addRow("WebSocket port",wsPort_);l->addLayout(f);auto*apply=new QPushButton("Apply OAL server settings");connect(apply,&QPushButton::clicked,this,[this](){if(!serverEnabled_->isChecked()){c_->stopOalServer();appendLog("OAL server stopped");return;}QString e;if(!c_->startOalServer(serverPort_->value(),wsEnabled_->isChecked(),wsPort_->value(),&e))showError(e);});l->addWidget(apply);}
    auto*sg=new QGroupBox("Stellarium Telescope Control bridge");auto*sf=new QFormLayout(sg);stellariumEnabled_=new QCheckBox("Enable Stellarium TCP bridge");stellariumPort_=ispin(1,65535,10000);stellariumEnabled_->setChecked(c_->stellariumRunning());if(c_->stellariumPort())stellariumPort_->setValue(c_->stellariumPort());sf->addRow(stellariumEnabled_);sf->addRow("TCP port",stellariumPort_);auto*sa=new QPushButton("Apply Stellarium settings");sf->addRow(sa);connect(sa,&QPushButton::clicked,this,[this](){if(!stellariumEnabled_->isChecked()){c_->stopStellariumServer();appendLog("Stellarium bridge stopped");return;}QString e;if(!c_->startStellariumServer(quint16(stellariumPort_->value()),&e))showError(e);else appendLog(QString("Stellarium bridge listening on TCP %1").arg(stellariumPort_->value()));});l->addWidget(sg);
    auto*note=new QLabel("Stellarium's standard Telescope Control protocol provides mount position and GOTO. Camera, focuser, autofocus, polar alignment and sessions remain controlled through OpenAstroLink.");note->setWordWrap(true);l->addWidget(note);l->addStretch();return w;}
QWidget *MainWindow::buildProfileTab(){auto*w=new QWidget;auto*l=new QVBoxLayout(w);auto p=c_->profile();
    auto*mainBox=new QGroupBox("Main imaging optical train");auto*f=new QFormLayout(mainBox);opticalDesign_=new QLineEdit(p.opticalDesign);aperture_=dspin(1,5000,p.apertureMm,2);obstruction_=dspin(0,5000,p.centralObstructionMm,2);focal_=dspin(1,100000,p.focalLengthMm,3);pixel_=dspin(0.1,100,p.pixelSizeUm,3);sensorW_=ispin(1,100000,p.sensorWidthPx);sensorH_=ispin(1,100000,p.sensorHeightPx);f->addRow("Optical design",opticalDesign_);f->addRow("Aperture / primary mirror mm",aperture_);f->addRow("Central obstruction mm",obstruction_);f->addRow("Effective focal length mm",focal_);f->addRow("Main camera pixel μm",pixel_);f->addRow("Main sensor width",sensorW_);f->addRow("Main sensor height",sensorH_);l->addWidget(mainBox);
    auto*guideBox=new QGroupBox("Guide optical train");auto*gf=new QFormLayout(guideBox);guideScopeName_=new QLineEdit(p.guideScopeName);guideAperture_=dspin(1,2000,p.guideApertureMm,2);guideFocal_=dspin(1,10000,p.guideFocalLengthMm,2);guidePixel_=dspin(0.1,100,p.guidePixelSizeUm,3);guideSensorW_=ispin(1,100000,p.guideSensorWidthPx);guideSensorH_=ispin(1,100000,p.guideSensorHeightPx);gf->addRow("Guide scope name",guideScopeName_);gf->addRow("Guide aperture mm",guideAperture_);gf->addRow("Guide focal length mm",guideFocal_);gf->addRow("Guide camera pixel μm",guidePixel_);gf->addRow("Guide sensor width",guideSensorW_);gf->addRow("Guide sensor height",guideSensorH_);l->addWidget(guideBox);
    auto*siteBox=new QGroupBox("Observatory location");auto*sf=new QFormLayout(siteBox);lat_=dspin(-90,90,p.observer.latitudeDeg,6);lon_=dspin(-180,180,p.observer.longitudeDeg,6);elevation_=dspin(-500,10000,p.observer.elevationM,1);sf->addRow("Latitude",lat_);sf->addRow("Longitude",lon_);sf->addRow("Elevation m",elevation_);l->addWidget(siteBox);
    auto*mountBox=new QGroupBox("Mount geometry / mechanical coordinates");auto*mf=new QFormLayout(mountBox);profileMountGeometry_=new QComboBox;profileMountGeometry_->addItem("German Equatorial (GEM)",int(MountGeometryType::GermanEquatorial));profileMountGeometry_->addItem("Fork Equatorial",int(MountGeometryType::ForkEquatorial));profileMountGeometry_->addItem("Alt-Azimuth",int(MountGeometryType::AltAzimuth));profileMountGeometry_->addItem("Alt-Azimuth + derotator",int(MountGeometryType::AltAzimuthDerotator));profileMountGeometry_->addItem("Equatorial platform",int(MountGeometryType::EquatorialPlatform));profileMountGeometry_->addItem("Custom two-axis",int(MountGeometryType::CustomTwoAxis));{const int i=profileMountGeometry_->findData(int(p.mount.type));if(i>=0)profileMountGeometry_->setCurrentIndex(i);}profileAxis1Sign_=new QComboBox;profileAxis1Sign_->addItem("+1 / normal",1);profileAxis1Sign_->addItem("-1 / reversed",-1);profileAxis1Sign_->setCurrentIndex(p.mount.axis1Sign>=0?0:1);profileAxis2Sign_=new QComboBox;profileAxis2Sign_->addItem("+1 / normal",1);profileAxis2Sign_->addItem("-1 / reversed",-1);profileAxis2Sign_->setCurrentIndex(p.mount.axis2Sign>=0?0:1);profilePierSide_=new QComboBox;profilePierSide_->addItems(QStringList{"east","west"});{const int i=profilePierSide_->findText(p.mount.preferredPierSide);if(i>=0)profilePierSide_->setCurrentIndex(i);}homeAxis1_=dspin(-360,360,p.mount.homeAxis1Deg,4);homeAxis2_=dspin(-360,360,p.mount.homeAxis2Deg,4);parkAxis1_=dspin(-360,360,p.mount.parkAxis1Deg,4);parkAxis2_=dspin(-360,360,p.mount.parkAxis2Deg,4);autoPierFlip_=new QCheckBox("Allow automatic pier flip (experimental)");autoPierFlip_->setChecked(p.mount.allowAutomaticPierFlip);maxGotoAxisDelta_=dspin(0.1,180.0,p.mount.maxGotoAxisDeltaDeg,1);mf->addRow("Geometry",profileMountGeometry_);mf->addRow("Axis 1 sign",profileAxis1Sign_);mf->addRow("Axis 2 sign",profileAxis2Sign_);mf->addRow("GEM pier branch after Sync",profilePierSide_);mf->addRow("Home axis 1 deg",homeAxis1_);mf->addRow("Home axis 2 deg",homeAxis2_);mf->addRow("Park axis 1 deg",parkAxis1_);mf->addRow("Park axis 2 deg",parkAxis2_);mf->addRow("Native max sky GOTO separation, deg",maxGotoAxisDelta_);mf->addRow(autoPierFlip_);l->addWidget(mountBox);
    auto*save=new QPushButton("Save profile");connect(save,&QPushButton::clicked,this,[this](){auto p=c_->profile();p.opticalDesign=opticalDesign_->text();p.apertureMm=aperture_->value();p.centralObstructionMm=obstruction_->value();p.focalLengthMm=focal_->value();p.pixelSizeUm=pixel_->value();p.sensorWidthPx=sensorW_->value();p.sensorHeightPx=sensorH_->value();p.guideScopeName=guideScopeName_->text();p.guideApertureMm=guideAperture_->value();p.guideFocalLengthMm=guideFocal_->value();p.guidePixelSizeUm=guidePixel_->value();p.guideSensorWidthPx=guideSensorW_->value();p.guideSensorHeightPx=guideSensorH_->value();p.observer={lat_->value(),lon_->value(),elevation_->value()};p.mount.type=MountGeometryType(profileMountGeometry_->currentData().toInt());p.mount.axis1Sign=profileAxis1Sign_->currentData().toInt();p.mount.axis2Sign=profileAxis2Sign_->currentData().toInt();p.mount.preferredPierSide=profilePierSide_->currentText();p.mount.homeAxis1Deg=homeAxis1_->value();p.mount.homeAxis2Deg=homeAxis2_->value();p.mount.parkAxis1Deg=parkAxis1_->value();p.mount.parkAxis2Deg=parkAxis2_->value();p.mount.allowAutomaticPierFlip=autoPierFlip_->isChecked();p.mount.maxGotoAxisDeltaDeg=maxGotoAxisDelta_?maxGotoAxisDelta_->value():15.0;c_->setProfile(p);appendLog(QString("Profile saved: main f/%1, %2 arcsec/px; guide f/%3, %4 arcsec/px").arg(p.focalRatio(),0,'f',2).arg(p.arcsecPerPixel(),0,'f',3).arg(p.guideFocalRatio(),0,'f',2).arg(p.guideArcsecPerPixel(),0,'f',3));});auto*systemLocation=new QPushButton("Use system location");connect(systemLocation,&QPushButton::clicked,c_,&ObservatoryController::requestSystemLocation);l->addWidget(save);l->addWidget(systemLocation);l->addStretch();return w;}
void MainWindow::refreshMountStatus(){
    if(!mountStatus_)return;
    MountStatus st;QString e;
    if(!c_->mountStatus(st,&e)){mountStatus_->setText("Mount status: unavailable"+(e.isEmpty()?QString():" — "+e));return;}
    const auto displayFrame=mountFrame_?EquatorialFrame(mountFrame_->currentData().toInt()):EquatorialFrame::J2000;
    const auto shown=st.coordinateValid?convertEquatorialFrame(st.coordinate,displayFrame):st.coordinate;
    QString text;
    if(st.coordinateValid)text=QString("RA %1°   DEC %2°   [%3]\n").arg(shown.raDeg,0,'f',6).arg(shown.decDeg,0,'f',6).arg(equatorialFrameName(displayFrame));
    else text="Sky coordinates: not synced yet\n";
    text+=QString("Geometry: %1   Tracking: %2   Slewing: %3   Parked: %4   Pier: %5")
        .arg(st.geometryType).arg(st.tracking?"ON":"OFF").arg(st.slewing?"YES":"NO").arg(st.parked?"YES":"NO").arg(st.pierSide);
    if(st.axes.valid)text+=QString("\nMechanical axes: Axis1=%1°   Axis2=%2°").arg(st.axes.axis1Deg,0,'f',6).arg(st.axes.axis2Deg,0,'f',6);
    if(!st.diagnostics.isEmpty()){const auto d=st.diagnostics;text+=QString("\nBackend diagnostics: alignmentMode=%1 trackingRate=%2 backendSite=(%3,%4,%5m) backendUTC=%6")
        .arg(d.value("alignmentMode").toInt(-1)).arg(d.value("trackingRate").toInt(-1)).arg(d.value("siteLatitude").toDouble(999.0),0,'f',6).arg(d.value("siteLongitude").toDouble(999.0),0,'f',6).arg(d.value("siteElevation").toDouble(-99999.0),0,'f',1).arg(d.value("utcDate").toString("n/a"));}
    mountStatus_->setText(text);
    if(mountTracking_){QSignalBlocker b(mountTracking_);mountTracking_->setChecked(st.tracking);}
    if(mountParked_){QSignalBlocker b(mountParked_);mountParked_->setChecked(st.parked);}
}
void MainWindow::updateMountStatusFromState(const QJsonObject&state){
    if(!mountStatus_)return;auto m=state.value("mount").toObject();if(m.isEmpty())return;
    const bool tracking=m.value("tracking").toBool();const bool parked=m.value("parked").toBool();
    const bool coordinateValid=!m.contains("coordinateValid")||m.value("coordinateValid").toBool();
    const auto sourceFrame=equatorialFrameFromString(m.value("coordinateFrame").toString("J2000"));
    const auto displayFrame=mountFrame_?EquatorialFrame(mountFrame_->currentData().toInt()):EquatorialFrame::J2000;
    const EquatorialCoord source{m.value("raDeg").toDouble(),m.value("decDeg").toDouble(),sourceFrame};
    const auto shown=coordinateValid?convertEquatorialFrame(source,displayFrame):source;
    QString text;
    if(coordinateValid)text=QString("RA %1°   DEC %2°   [%3]\n").arg(shown.raDeg,0,'f',6).arg(shown.decDeg,0,'f',6).arg(equatorialFrameName(displayFrame));
    else text="Sky coordinates: not synced yet\n";
    text+=QString("Geometry: %1   Tracking: %2   Slewing: %3   Parked: %4   Pier: %5")
        .arg(m.value("geometryType").toString("unknown")).arg(tracking?"ON":"OFF").arg(m.value("slewing").toBool()?"YES":"NO").arg(parked?"YES":"NO").arg(m.value("pierSide").toString("unknown"));
    const bool axesValid=m.value("axesValid").toBool(m.contains("axis1Deg")&&m.contains("axis2Deg"));
    if(axesValid)text+=QString("\nMechanical axes: Axis1=%1°   Axis2=%2°").arg(m.value("axis1Deg").toDouble(),0,'f',6).arg(m.value("axis2Deg").toDouble(),0,'f',6);
    const auto d=m.value("diagnostics").toObject();if(!d.isEmpty())text+=QString("\nBackend diagnostics: alignmentMode=%1 trackingRate=%2 backendSite=(%3,%4,%5m) backendUTC=%6")
        .arg(d.value("alignmentMode").toInt(-1)).arg(d.value("trackingRate").toInt(-1)).arg(d.value("siteLatitude").toDouble(999.0),0,'f',6).arg(d.value("siteLongitude").toDouble(999.0),0,'f',6).arg(d.value("siteElevation").toDouble(-99999.0),0,'f',1).arg(d.value("utcDate").toString("n/a"));
    mountStatus_->setText(text);
    if(mountTracking_){QSignalBlocker b(mountTracking_);mountTracking_->setChecked(tracking);}
    if(mountParked_){QSignalBlocker b(mountParked_);mountParked_->setChecked(parked);}
}
void MainWindow::refreshFocuserStatus(){
    if(!focuserStatus_)return;
    FocuserStatus st;QString e;
    if(!c_->focuserStatus(st,&e)){
        focuserStatus_->setText("Focuser status: unavailable"+(e.isEmpty()?QString():" — "+e));
        if(focusMotionPollTimer_)focusMotionPollTimer_->stop();
        return;
    }
    QString text=QString("Actual position: %1   Moving: %2").arg(st.position).arg(st.moving?"YES":"NO");
    if(st.temperatureC)text+=QString("   Temperature: %1 °C").arg(*st.temperatureC,0,'f',1);
    focuserStatus_->setText(text);
    if(focusPosition_&&!focusTargetDirty_){
        QSignalBlocker blocker(focusPosition_);
        focusPosition_->setValue(st.position);
        focusTargetInitialized_=true;
    }
    if(focusMotionPollTimer_){
        if(st.moving){if(!focusMotionPollTimer_->isActive())focusMotionPollTimer_->start();}
        else focusMotionPollTimer_->stop();
    }
}
void MainWindow::updateFocuserStatusFromState(const QJsonObject&state){if(!focuserStatus_)return;auto f=state.value("focuser").toObject();if(f.isEmpty()){focuserStatus_->setText("Focuser status: unavailable");return;}const int actual=f.value("position").toInt();QString t=QString("Actual position: %1   Moving: %2").arg(actual).arg(f.value("moving").toBool()?"YES":"NO");if(f.contains("temperatureC"))t+=QString("   Temperature: %1 °C").arg(f.value("temperatureC").toDouble(),0,'f',1);focuserStatus_->setText(t);if(focusPosition_&&!focusTargetDirty_){QSignalBlocker b(focusPosition_);focusPosition_->setValue(actual);focusTargetInitialized_=true;}}
void MainWindow::setCaptureBusy(bool busy){captureBusy_=busy;if(captureButton_)captureButton_->setText(busy?"Cancel exposure":"Capture");if(captureSolveButton_)captureSolveButton_->setEnabled(!busy);if(busy)statusBar()->showMessage("Exposure queued/running — camera locked by operation; GUI and mount remain responsive");else statusBar()->showMessage(QString("Core: %1 — %2").arg(c_->controlMode(),c_->endpointDescription()));}
void MainWindow::setAdaptiveSolveBusy(bool busy){adaptiveSolveBusy_=busy;if(adaptiveSolveButton_)adaptiveSolveButton_->setText(busy?"Cancel adaptive solve":"Adaptive urban capture + solve");if(captureButton_)captureButton_->setEnabled(!busy);if(captureSolveButton_)captureSolveButton_->setEnabled(!busy);if(solveButton_)solveButton_->setEnabled(!busy);if(busy)statusBar()->showMessage("Adaptive plate solve running — camera + solver locked; short frames are registered/stacked as needed");else statusBar()->showMessage(QString("Core: %1 — %2").arg(c_->controlMode(),c_->endpointDescription()));}
void MainWindow::setLiveViewBusy(bool busy){liveViewBusy_=busy;if(liveViewButton_)liveViewButton_->setText(busy?"Stop Live View":"Start Live View");if(sceneAutofocusButton_)sceneAutofocusButton_->setEnabled(!busy&&!autofocusBusy_);if(captureButton_)captureButton_->setEnabled(!busy);if(captureSolveButton_)captureSolveButton_->setEnabled(!busy);if(adaptiveSolveButton_)adaptiveSolveButton_->setEnabled(!busy);if(busy)statusBar()->showMessage("Live View running — camera locked for continuous preview; mount/focuser manual controls remain available");else statusBar()->showMessage(QString("Core: %1 — %2").arg(c_->controlMode(),c_->endpointDescription()));}
void MainWindow::setAutofocusBusy(bool busy){autofocusBusy_=busy;if(autofocusButton_)autofocusButton_->setText(busy?"Cancel autofocus":"Run autofocus");if(sceneAutofocusButton_)sceneAutofocusButton_->setEnabled(!busy&&!liveViewBusy_);if(focusMoveButton_)focusMoveButton_->setEnabled(!busy);if(busy)statusBar()->showMessage("Autofocus running — camera and focuser locked; mount controls remain available");else statusBar()->showMessage(QString("Core: %1 — %2").arg(c_->controlMode(),c_->endpointDescription()));}
void MainWindow::updateDeviceStatusFromState(const QJsonObject&state){
    // Native devices can appear after the GUI connected (USB hot-plug, serial
    // reset recovery, delayed vendor-SDK enumeration). Keep the comboboxes in
    // lock-step with the node catalogue carried by each state event instead of
    // freezing the list that happened to exist at GUI startup.
    refreshBackendComboIfChanged(cameraBackend_,c_->cameraBackends());
    refreshBackendComboIfChanged(guideCameraBackend_,c_->cameraBackends());
    refreshBackendComboIfChanged(mountBackend_,c_->mountBackends());
    refreshBackendComboIfChanged(focuserBackend_,c_->focuserBackends());
    struct Ui{QString type;QString role;QComboBox*backend;QLineEdit*endpoint;QLabel*status;};
    const Ui ui[]={{"camera","main",cameraBackend_,cameraEndpoint_,cameraDeviceStatus_},{"camera","guide",guideCameraBackend_,guideCameraEndpoint_,guideCameraDeviceStatus_},{"mount","main",mountBackend_,mountEndpoint_,mountDeviceStatus_},{"focuser","main",focuserBackend_,focuserEndpoint_,focuserDeviceStatus_}};
    const auto devices=state.value("devices").toArray();
    for(const auto &x:ui){if(!x.status)continue;QJsonObject found;for(const auto&v:devices){auto o=v.toObject();const QString role=o.value("role").toString("main");if(o.value("type").toString()==x.type&&role==x.role&&o.value("connected").toBool()){found=o;break;}}if(found.isEmpty()){x.status->setText("Disconnected");x.status->setStyleSheet("color:#a00");continue;}const QString backend=found.value("backend").toString();const QString endpoint=found.value("endpoint").toString();const QString name=found.value("name").toString();x.status->setText(QString("Connected — %1 (%2)").arg(name,backend));x.status->setStyleSheet("color:#080");if(x.backend){int i=x.backend->findText(backend);if(i>=0)x.backend->setCurrentIndex(i);}if(x.endpoint&&!endpoint.isNull())x.endpoint->setText(endpoint);}
}
void MainWindow::updateOperation(const QJsonObject&o){if(o.isEmpty())return;const QString id=o.value("id").toString();if(id.isEmpty())return;const QString state=o.value("state").toString();const QString kind=o.value("kind").toString();const int pct=int(o.value("progress").toDouble()*100.0);QStringList locks;for(const auto&v:o.value("resourceLocks").toArray())locks<<v.toString();const QString text=QString("%1  %2  %3  %4%  [%5]").arg(kind,id,state).arg(pct).arg(locks.join(","));if(operationsList_){QListWidgetItem*item=nullptr;for(int i=0;i<operationsList_->count();++i)if(operationsList_->item(i)->data(Qt::UserRole).toString()==id){item=operationsList_->item(i);break;}if(!item){item=new QListWidgetItem(operationsList_);item->setData(Qt::UserRole,id);}item->setText(text);}
    if(kind=="camera.live-view"&&(state=="queued"||state=="running")&&liveViewOperationId_.isEmpty()){liveViewOperationId_=id;setLiveViewBusy(true);}
    if(id==liveViewOperationId_&&(state=="succeeded"||state=="failed"||state=="cancelled")){if(state=="failed")appendLog("Live View failed: "+o.value("problem").toObject().value("message").toString());else appendLog("Live View stopped");setLiveViewBusy(false);liveViewOperationId_.clear();}
    if(kind=="camera.exposure"&&(state=="queued"||state=="running")&&captureOperationId_.isEmpty()){captureOperationId_=id;setCaptureBusy(true);}
    if(id==captureOperationId_&&(state=="succeeded"||state=="failed"||state=="cancelled")){const bool solveAfter=captureSolveRequested_;if(state=="failed")appendLog("Exposure failed: "+o.value("problem").toObject().value("message").toString());else if(state=="cancelled")appendLog("Exposure cancelled");else{const QString science=o.value("result").toObject().value("scienceFilePath").toString();if(!science.isEmpty())appendLog("Science/original file: "+science);}setCaptureBusy(false);captureOperationId_.clear();captureSolveRequested_=false;if(state=="succeeded"&&solveAfter){const QString frameId=o.value("result").toObject().value("frameId").toString();if(!frameId.isEmpty()&&c_->lastFrame().id==frameId)QTimer::singleShot(0,this,[this](){SolveHint h;h.raDeg=hintRa_->value();h.decDeg=hintDec_->value();h.searchRadiusDeg=hintRadius_->value();c_->solveLast(h);});else pendingSolveFrameId_=frameId;}}
    if(kind=="solver.adaptive"&&(state=="queued"||state=="running")&&adaptiveSolveOperationId_.isEmpty()){adaptiveSolveOperationId_=id;setAdaptiveSolveBusy(true);}
    if(id==adaptiveSolveOperationId_&&(state=="succeeded"||state=="failed"||state=="cancelled")){if(state=="failed")appendLog("Adaptive solve failed: "+o.value("problem").toObject().value("message").toString());else if(state=="cancelled")appendLog("Adaptive solve cancelled");else{auto r=o.value("result").toObject();appendLog(QString("Adaptive solve OK: attempt data retained, effective exposure %1 s, registered frames %2").arg(r.value("effectiveExposureSec").toDouble()).arg(r.value("registeredFrames").toInt()));}setAdaptiveSolveBusy(false);adaptiveSolveOperationId_.clear();}
    if(kind=="autofocus.run"&&(state=="queued"||state=="running")&&autofocusOperationId_.isEmpty()){autofocusOperationId_=id;setAutofocusBusy(true);}
    if(id==autofocusOperationId_&&(state=="succeeded"||state=="failed"||state=="cancelled")){setAutofocusBusy(false);autofocusOperationId_.clear();}
}
void MainWindow::appendLog(const QString&s){log_->append(QTime::currentTime().toString("HH:mm:ss ")+s);}void MainWindow::showError(const QString&s){appendLog("ERROR: "+s);QMessageBox::warning(this,"OpenAstroSuite",s);}void MainWindow::updateAstrometryOverlay(){if(lastImage_.isNull())return;QImage o=lastImage_.copy();QPainter p(&o);p.setPen(QPen(Qt::green,2));for(const auto&s:c_->lastSolve().imageStars)p.drawEllipse(QPointF(s.positionPx.x,s.positionPx.y),6,6);p.setPen(Qt::yellow);p.drawText(12,24,QString("RA %1  DEC %2  PA %3  matches %4").arg(c_->lastSolve().raDeg,0,'f',5).arg(c_->lastSolve().decDeg,0,'f',5).arg(c_->lastSolve().rotationDeg,0,'f',2).arg(c_->lastSolve().matchedStars));astroImage_->setPixmap(QPixmap::fromImage(o).scaled(astroImage_->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));}void MainWindow::updateStarMap(){starScene_->clear();starScene_->setSceneRect(0,0,600,200);starScene_->addRect(starScene_->sceneRect(),QPen(Qt::darkBlue),QBrush(QColor(3,3,16)));if(c_->lastFrame().image.empty())return;double sx=600.0/c_->lastFrame().image.cols,sy=200.0/c_->lastFrame().image.rows;for(const auto&s:c_->lastSolve().imageStars){double r=std::clamp(2.0+std::log1p(s.flux),2.0,8.0);starScene_->addEllipse(s.positionPx.x*sx-r,s.positionPx.y*sy-r,2*r,2*r,QPen(Qt::white),QBrush(Qt::white));}auto*t=starScene_->addText(QString("Solved center RA %1° DEC %2°").arg(c_->lastSolve().raDeg,0,'f',4).arg(c_->lastSolve().decDeg,0,'f',4));t->setDefaultTextColor(Qt::cyan);t->setPos(8,8);}
} // namespace oas
