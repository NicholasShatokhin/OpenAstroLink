#pragma once
#include <QMainWindow>
#include <QImage>
#include <QPixmap>
#include <memory>

class QLabel;
class QLineEdit;
class QPushButton;
class StarView;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
public slots:
    void onCaptureFrame();       // з камери
    void onSolveFrame();         // plate solving
    void onMountSync();          // синкнути монту
    void onMountSlew();          // поїхати в RA/DEC
    void updateAstrometryImage(const QImage &img); // показати зірки поверх
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
};
