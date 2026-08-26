#pragma once
#include "core/observatory_controller.h"
#include <QMainWindow>

class QComboBox; class QLineEdit; class QDoubleSpinBox; class QSpinBox; class QLabel; class QTextEdit; class QListWidget; class QGraphicsScene; class QCheckBox; class QPushButton; class QTabWidget;

namespace oas {
class MainWindow final : public QMainWindow {
    Q_OBJECT
public: explicit MainWindow(ObservatoryController *controller,QWidget *parent=nullptr);
private:
    QWidget *buildDevicesTab(); QWidget *buildCaptureTab(); QWidget *buildMountTab(); QWidget *buildFocusTab(); QWidget *buildPolarTab(); QWidget *buildSchedulerTab(); QWidget *buildOperationsTab(); QWidget *buildServerTab(); QWidget *buildProfileTab();
    void appendLog(const QString&); void showError(const QString&); void updateAstrometryOverlay(); void updateStarMap();
    void refreshMountStatus(); void refreshFocuserStatus(); void setAutofocusBusy(bool busy); void setCaptureBusy(bool busy); void setAdaptiveSolveBusy(bool busy);
    void updateMountStatusFromState(const QJsonObject &state); void updateFocuserStatusFromState(const QJsonObject &state); void updateDeviceStatusFromState(const QJsonObject &state); void updateOperation(const QJsonObject &operation);
    ObservatoryController *c_{};
    QLabel *rawImage_{}; QLabel *astroImage_{}; QGraphicsScene *starScene_{}; QTextEdit *log_{};
    QImage lastImage_;
    QComboBox *cameraBackend_{}; QLineEdit *cameraEndpoint_{}; QLabel *cameraDeviceStatus_{};
    QComboBox *guideCameraBackend_{}; QLineEdit *guideCameraEndpoint_{}; QLabel *guideCameraDeviceStatus_{};
    QComboBox *mountBackend_{}; QLineEdit *mountEndpoint_{}; QLabel *mountDeviceStatus_{};
    QComboBox *focuserBackend_{}; QLineEdit *focuserEndpoint_{}; QLabel *focuserDeviceStatus_{};
    QComboBox *nativeSerialDriver_{}; QComboBox *nativeSerialPort_{};
    QDoubleSpinBox *exposure_{}; QComboBox *solverBackend_{}; QLineEdit *catalogPath_{}; QLineEdit *modelPath_{}; QSpinBox *gain_{}; QDoubleSpinBox *hintRa_{}; QDoubleSpinBox *hintDec_{}; QDoubleSpinBox *hintRadius_{};
    QSpinBox *solveBin_{}; QSpinBox *solveStackFrames_{}; QSpinBox *solveMinStars_{}; QDoubleSpinBox *solveBaseExposure_{}; QDoubleSpinBox *solveMaxExposure_{};
    QPushButton *captureButton_{}; QPushButton *captureSolveButton_{}; QPushButton *solveButton_{}; QPushButton *adaptiveSolveButton_{}; QPushButton *motionButton_{}; QTabWidget *tabs_{}; bool captureBusy_{false}; bool captureSolveRequested_{false}; QString captureOperationId_; QString pendingSolveFrameId_; bool adaptiveSolveBusy_{false}; QString adaptiveSolveOperationId_;
    QDoubleSpinBox *mountRa_{}; QDoubleSpinBox *mountDec_{}; QLabel *mountStatus_{}; QCheckBox *mountTracking_{}; QCheckBox *mountParked_{};
    QSpinBox *focusPosition_{}; QComboBox *focusMode_{}; QSpinBox *focusRange_{}; QSpinBox *coarseStep_{}; QSpinBox *fineStep_{}; QSpinBox *focusFrames_{}; QLabel *focuserStatus_{};
    QPushButton *focusMoveButton_{}; QPushButton *focusHaltButton_{}; QPushButton *autofocusButton_{}; bool focusTargetDirty_{false}; bool focusTargetInitialized_{false}; bool autofocusBusy_{false}; QString autofocusOperationId_;
    QDoubleSpinBox *polarStep_{}; QLabel *polarSamples_{}; QLabel *polarResult_{};
    QListWidget *operationsList_{}; QPushButton *cancelOperationButton_{};
    QLineEdit *targetName_{}; QDoubleSpinBox *targetRa_{}; QDoubleSpinBox *targetDec_{}; QDoubleSpinBox *targetExposure_{}; QSpinBox *targetRepeats_{}; QListWidget *targetList_{}; std::vector<SessionTarget> targets_;
    QCheckBox *serverEnabled_{}; QSpinBox *serverPort_{}; QCheckBox *wsEnabled_{}; QSpinBox *wsPort_{};
    QCheckBox *stellariumEnabled_{}; QSpinBox *stellariumPort_{};
    QLineEdit *opticalDesign_{}; QDoubleSpinBox *aperture_{}; QDoubleSpinBox *obstruction_{};
    QDoubleSpinBox *focal_{}; QDoubleSpinBox *pixel_{}; QSpinBox *sensorW_{}; QSpinBox *sensorH_{};
    QLineEdit *guideScopeName_{}; QDoubleSpinBox *guideAperture_{}; QDoubleSpinBox *guideFocal_{}; QDoubleSpinBox *guidePixel_{}; QSpinBox *guideSensorW_{}; QSpinBox *guideSensorH_{};
    QDoubleSpinBox *lat_{}; QDoubleSpinBox *lon_{}; QDoubleSpinBox *elevation_{};
};
}
