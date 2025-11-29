#pragma once
#include <QMainWindow>
#include <QImage>
#include <QPixmap>
#include <memory>

class QLabel;
class QLineEdit;
class QPushButton;
class StarView;
class ControlPanel;
class Guiding;
class Scheduler;
class StarCatalog;
class NeuralSolver;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void onCaptureFrame();       // з камери
    void onSolveFrame();         // plate solving
    void onMountSync();          // синкнути монту
    void onMountSlew();          // поїхати в RA/DEC
    void updateAstrometryImage(const QImage &img); // показати зірки поверх
    void onNextTarget();
    void onRunNN();

signals:
    void solvedRaDec(double raDeg, double decDeg);

private:
    QLabel *rawImageLabel_;      // фото з камери
    QLabel *astrometryLabel_;    // фото з розпізнаними зірками
    StarView *starView_;         // зоряна мапа

    QLineEdit *latEdit_;
    QLineEdit *lonEdit_;
    QLineEdit *focalEdit_;
    QLineEdit *pixelEdit_;
    QLineEdit *mountRaEdit_;
    QLineEdit *mountDecEdit_;

    ControlPanel *ctrl_;

    std::unique_ptr<Guiding> guiding_;
    std::unique_ptr<Scheduler> scheduler_;
    std::unique_ptr<StarCatalog> catalog_;
    std::unique_ptr<NeuralSolver> nn_;

    double lastSolvedRaDeg_ = 0.0, lastSolvedDecDeg_ = 0.0;
};
