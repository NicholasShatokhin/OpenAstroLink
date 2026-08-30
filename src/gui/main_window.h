#pragma once
class QTimer;
#include "core/observatory_controller.h"
#include <QMainWindow>
#include <QHash>

class QComboBox; class QLineEdit; class QDoubleSpinBox; class QSpinBox; class QLabel; class QTextEdit; class QListWidget; class QGraphicsScene; class QCheckBox; class QPushButton; class QTabWidget;

namespace oas {
class MainWindow final : public QMainWindow {
    Q_OBJECT
public: explicit MainWindow(ObservatoryController *controller,QWidget *parent=nullptr);
private:
    QWidget *buildDevicesTab(); QWidget *buildLiveFinderTab(); QWidget *buildCaptureTab(); QWidget *buildMountTab(); QWidget *buildFocusTab(); QWidget *buildPolarTab(); QWidget *buildSchedulerTab(); QWidget *buildOperationsTab(); QWidget *buildServerTab(); QWidget *buildProfileTab();
    void appendLog(const QString&); void showError(const QString&); void updateAstrometryOverlay(); void updateStarMap(); void updateHistogram(const QImage &image); void renderCameraFrame(const QImage &image); void updateFinderWizardText();
    void refreshMountStatus(); void refreshFocuserStatus(); void synchronizeMountCoordinatesFrom(const QString &system); void setAutofocusBusy(bool busy); void setCaptureBusy(bool busy); void setAdaptiveSolveBusy(bool busy); void setLiveViewBusy(bool busy);
    void updateMountStatusFromState(const QJsonObject &state); void updateFocuserStatusFromState(const QJsonObject &state); void updateDeviceStatusFromState(const QJsonObject &state); void updateOperation(const QJsonObject &operation);
    ObservatoryController *c_{};
    QLabel *rawImage_{}; QLabel *astroImage_{}; QGraphicsScene *starScene_{}; QTextEdit *log_{};
    QImage lastImage_;
    QComboBox *cameraBackend_{}; QLineEdit *cameraEndpoint_{}; QLabel *cameraDeviceStatus_{};
    QComboBox *guideCameraBackend_{}; QLineEdit *guideCameraEndpoint_{}; QLabel *guideCameraDeviceStatus_{};
    QComboBox *mountBackend_{}; QLineEdit *mountEndpoint_{}; QLabel *mountDeviceStatus_{}; QHash<QString,QString> mountEndpointsByBackend_; QString lastMountBackend_;
    QComboBox *focuserBackend_{}; QLineEdit *focuserEndpoint_{}; QLabel *focuserDeviceStatus_{};
    QComboBox *nativeSerialDriver_{}; QComboBox *nativeSerialPort_{};
    QDoubleSpinBox *liveExposure_{}; QSpinBox *liveGain_{}; QSpinBox *liveBin_{}; QDoubleSpinBox *liveFps_{};
    QCheckBox *liveAutoStretch_{}; QCheckBox *liveCrosshair_{}; QCheckBox *liveHighlight_{}; QCheckBox *liveDebayer_{}; QComboBox *liveBayerPattern_{}; QLabel *liveTargetStatus_{}; QLabel *finderWizardText_{};
    QPushButton *liveViewButton_{}; QPushButton *sceneAutofocusButton_{}; QPushButton *finderWizardButton_{}; QPushButton *finderWizardNextButton_{};
    bool liveViewBusy_{false}; QString liveViewOperationId_; int finderWizardStep_{0};
    QDoubleSpinBox *exposure_{}; QComboBox *solverBackend_{}; QLineEdit *catalogPath_{}; QLineEdit *modelPath_{}; QSpinBox *gain_{}; QDoubleSpinBox *hintRa_{}; QDoubleSpinBox *hintDec_{}; QDoubleSpinBox *hintRadius_{};
    QSpinBox *solveBin_{}; QSpinBox *solveStackFrames_{}; QSpinBox *solveMinStars_{}; QDoubleSpinBox *solveBaseExposure_{}; QDoubleSpinBox *solveMaxExposure_{};
    QCheckBox *saveScience_{}; QCheckBox *histogramEnabled_{}; QCheckBox *histogramAutoExposure_{}; QLabel *histogramView_{}; QLabel *histogramStats_{}; QDoubleSpinBox *histogramTarget_{}; QPushButton *histogramApplyButton_{}; double histogramSuggestedExposure_{0.0};
    QPushButton *captureButton_{}; QPushButton *captureSolveButton_{}; QPushButton *solveButton_{}; QPushButton *adaptiveSolveButton_{}; QPushButton *motionButton_{}; QTabWidget *tabs_{}; bool captureBusy_{false}; bool captureSolveRequested_{false}; QString captureOperationId_; QString pendingSolveFrameId_; bool adaptiveSolveBusy_{false}; QString adaptiveSolveOperationId_;
    QDoubleSpinBox *mountRa_{}; QDoubleSpinBox *mountDec_{}; QComboBox *mountFrame_{};
    QDoubleSpinBox *mountJNowRa_{}; QDoubleSpinBox *mountJNowDec_{}; QDoubleSpinBox *mountAz_{}; QDoubleSpinBox *mountAlt_{}; QDoubleSpinBox *mountGalL_{}; QDoubleSpinBox *mountGalB_{}; QLabel *mountCoordEpoch_{}; bool mountCoordinateSyncing_{false};
    QLabel *mountStatus_{}; QCheckBox *mountTracking_{}; QCheckBox *mountParked_{}; QSpinBox *mountManualRate_{};
    QSpinBox *focusPosition_{}; QComboBox *focusMode_{}; QSpinBox *focusJogStep_{}; QSpinBox *focusRange_{}; QSpinBox *coarseStep_{}; QSpinBox *fineStep_{}; QSpinBox *focusFrames_{}; QDoubleSpinBox *focusExposure_{}; QSpinBox *focusGain_{}; QSpinBox *focusMinStars_{}; QLabel *focuserStatus_{};
    QPushButton *focusMoveButton_{}; QPushButton *focusHaltButton_{}; QPushButton *autofocusButton_{}; QTimer *focusMotionPollTimer_{}; bool focusTargetDirty_{false}; bool focusTargetInitialized_{false}; bool autofocusBusy_{false}; QString autofocusOperationId_;
    QDoubleSpinBox *polarStep_{}; QLabel *polarSamples_{}; QLabel *polarResult_{};
    QListWidget *operationsList_{}; QPushButton *cancelOperationButton_{};
    QLineEdit *targetName_{}; QDoubleSpinBox *targetRa_{}; QDoubleSpinBox *targetDec_{}; QDoubleSpinBox *targetExposure_{}; QSpinBox *targetRepeats_{}; QListWidget *targetList_{}; std::vector<SessionTarget> targets_;
    QCheckBox *serverEnabled_{}; QSpinBox *serverPort_{}; QCheckBox *wsEnabled_{}; QSpinBox *wsPort_{};
    QCheckBox *stellariumEnabled_{}; QSpinBox *stellariumPort_{};
    QLineEdit *opticalDesign_{}; QDoubleSpinBox *aperture_{}; QDoubleSpinBox *obstruction_{};
    QDoubleSpinBox *focal_{}; QDoubleSpinBox *pixel_{}; QSpinBox *sensorW_{}; QSpinBox *sensorH_{};
    QLineEdit *guideScopeName_{}; QDoubleSpinBox *guideAperture_{}; QDoubleSpinBox *guideFocal_{}; QDoubleSpinBox *guidePixel_{}; QSpinBox *guideSensorW_{}; QSpinBox *guideSensorH_{};
    QDoubleSpinBox *lat_{}; QDoubleSpinBox *lon_{}; QDoubleSpinBox *elevation_{};
    QComboBox *profileMountGeometry_{}; QComboBox *profileAxis1Sign_{}; QComboBox *profileAxis2Sign_{}; QComboBox *profilePierSide_{};
    QDoubleSpinBox *homeAxis1_{}; QDoubleSpinBox *homeAxis2_{}; QDoubleSpinBox *parkAxis1_{}; QDoubleSpinBox *parkAxis2_{}; QCheckBox *autoPierFlip_{};
};
}
