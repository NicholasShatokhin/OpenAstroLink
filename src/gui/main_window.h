#pragma once
#include "core/observatory_controller.h"
#include <QMainWindow>

class QComboBox; class QLineEdit; class QDoubleSpinBox; class QSpinBox; class QLabel; class QTextEdit; class QListWidget; class QGraphicsScene; class QCheckBox;

namespace oas {
class MainWindow final : public QMainWindow {
    Q_OBJECT
public: explicit MainWindow(ObservatoryController *controller,QWidget *parent=nullptr);
private:
    QWidget *buildDevicesTab(); QWidget *buildCaptureTab(); QWidget *buildMountTab(); QWidget *buildFocusTab(); QWidget *buildPolarTab(); QWidget *buildSchedulerTab(); QWidget *buildServerTab(); QWidget *buildProfileTab();
    void appendLog(const QString&); void showError(const QString&); void updateAstrometryOverlay(); void updateStarMap();
    ObservatoryController *c_{};
    QLabel *rawImage_{}; QLabel *astroImage_{}; QGraphicsScene *starScene_{}; QTextEdit *log_{};
    QImage lastImage_;
    QComboBox *cameraBackend_{}; QLineEdit *cameraEndpoint_{}; QComboBox *mountBackend_{}; QLineEdit *mountEndpoint_{}; QComboBox *focuserBackend_{}; QLineEdit *focuserEndpoint_{};
    QDoubleSpinBox *exposure_{}; QComboBox *solverBackend_{}; QLineEdit *catalogPath_{}; QLineEdit *modelPath_{}; QSpinBox *gain_{}; QDoubleSpinBox *hintRa_{}; QDoubleSpinBox *hintDec_{}; QDoubleSpinBox *hintRadius_{};
    QDoubleSpinBox *mountRa_{}; QDoubleSpinBox *mountDec_{};
    QSpinBox *focusPosition_{}; QComboBox *focusMode_{}; QSpinBox *focusRange_{}; QSpinBox *coarseStep_{}; QSpinBox *fineStep_{}; QSpinBox *focusFrames_{};
    QDoubleSpinBox *polarStep_{}; QLabel *polarSamples_{}; QLabel *polarResult_{};
    QLineEdit *targetName_{}; QDoubleSpinBox *targetRa_{}; QDoubleSpinBox *targetDec_{}; QDoubleSpinBox *targetExposure_{}; QSpinBox *targetRepeats_{}; QListWidget *targetList_{}; std::vector<SessionTarget> targets_;
    QCheckBox *serverEnabled_{}; QSpinBox *serverPort_{}; QCheckBox *wsEnabled_{}; QSpinBox *wsPort_{};
    QDoubleSpinBox *focal_{}; QDoubleSpinBox *pixel_{}; QSpinBox *sensorW_{}; QSpinBox *sensorH_{}; QDoubleSpinBox *lat_{}; QDoubleSpinBox *lon_{}; QDoubleSpinBox *elevation_{};
};
}
