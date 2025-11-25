#include "mainwindow.h"
#include "starview.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QImage>
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    auto *central = new QWidget(this);
    setCentralWidget(central);

    // ліва частина: зоряна мапа + зображення
    starView_ = new StarView(this);

    rawImageLabel_ = new QLabel(this);
    rawImageLabel_->setMinimumSize(320, 240);
    rawImageLabel_->setStyleSheet("background: #222;");

    astrometryLabel_ = new QLabel(this);
    astrometryLabel_->setMinimumSize(320, 240);
    astrometryLabel_->setStyleSheet("background: #222;");

    auto *imagesLayout = new QHBoxLayout;
    imagesLayout->addWidget(rawImageLabel_, 1);
    imagesLayout->addWidget(astrometryLabel_, 1);

    auto *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(starView_, 2);
    leftLayout->addLayout(imagesLayout, 1);

    // права панель: параметри і керування монтуванням
    auto *paramsBox = new QGroupBox("Параметри", this);
    latEdit_ = new QLineEdit("50.4501", this);
    lonEdit_ = new QLineEdit("30.5234", this);
    focalEdit_ = new QLineEdit("400", this);
    pixelEdit_ = new QLineEdit("4.8", this);

    auto *paramsLayout = new QVBoxLayout;
    paramsLayout->addWidget(new QLabel("Lat, deg:"));
    paramsLayout->addWidget(latEdit_);
    paramsLayout->addWidget(new QLabel("Lon, deg:"));
    paramsLayout->addWidget(lonEdit_);
    paramsLayout->addWidget(new QLabel("Focal, mm:"));
    paramsLayout->addWidget(focalEdit_);
    paramsLayout->addWidget(new QLabel("Pixel, um:"));
    paramsLayout->addWidget(pixelEdit_);
    paramsBox->setLayout(paramsLayout);

    auto *mountBox = new QGroupBox("Монтування", this);
    mountRaEdit_ = new QLineEdit("0:0:0", this);
    mountDecEdit_ = new QLineEdit("+00*00", this);
    auto *btnMountSync = new QPushButton("Sync to solved", this);
    auto *btnMountSlew = new QPushButton("Slew", this);

    auto *mountLayout = new QVBoxLayout;
    mountLayout->addWidget(new QLabel("RA (hh:mm:ss):"));
    mountLayout->addWidget(mountRaEdit_);
    mountLayout->addWidget(new QLabel("DEC (+dd*mm):"));
    mountLayout->addWidget(mountDecEdit_);
    mountLayout->addWidget(btnMountSync);
    mountLayout->addWidget(btnMountSlew);
    mountBox->setLayout(mountLayout);

    auto *btnCapture = new QPushButton("Зняти кадр", this);
    auto *btnSolve   = new QPushButton("Solve", this);

    auto *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(paramsBox);
    rightLayout->addWidget(mountBox);
    rightLayout->addWidget(btnCapture);
    rightLayout->addWidget(btnSolve);
    rightLayout->addStretch(1);

    auto *mainLayout = new QHBoxLayout;
    mainLayout->addLayout(leftLayout, 3);
    mainLayout->addLayout(rightLayout, 1);

    central->setLayout(mainLayout);

    // сигнали
    connect(btnCapture, &QPushButton::clicked, this, &MainWindow::onCaptureFrame);
    connect(btnSolve,   &QPushButton::clicked, this, &MainWindow::onSolveFrame);
    connect(btnMountSync, &QPushButton::clicked, this, &MainWindow::onMountSync);
    connect(btnMountSlew, &QPushButton::clicked, this, &MainWindow::onMountSlew);

    setWindowTitle("Astro Solver GUI");
    resize(1200, 700);
}

void MainWindow::onCaptureFrame()
{
    // TODO: тут викликаєш свій CaptureDevice (QHY/Canon)
    // зараз просто зальємо чорне
    QImage img(640, 480, QImage::Format_RGB888);
    img.fill(Qt::black);
    rawImageLabel_->setPixmap(QPixmap::fromImage(img).scaled(rawImageLabel_->size(), Qt::KeepAspectRatio));
}

void MainWindow::onSolveFrame()
{
    // TODO: тут викликаєш свій PlateSolver і StarDetector
    // для прикладу — просто покажемо 3 зірки і центр
    std::vector<StarPoint> stars = {
        {QPointF(-50, -20), 1.0},
        {QPointF(30,  40),  1.0},
        {QPointF(80, -60),  1.0},
    };
    starView_->setSolvedCenter(37.95, 89.25);
    starView_->setStars(stars);

    // і намалюємо на "астрометрії" ті ж точки
    QImage img(640, 480, QImage::Format_RGB888);
    img.fill(Qt::black);
    QPainter p(&img);
    p.setPen(Qt::green);
    for (auto &s : stars) {
        QPoint pt(320 + s.pos.x(), 240 + s.pos.y());
        p.drawEllipse(pt, 4, 4);
    }
    p.end();
    astrometryLabel_->setPixmap(QPixmap::fromImage(img).scaled(astrometryLabel_->size(), Qt::KeepAspectRatio));
}

void MainWindow::onMountSync()
{
    // TODO: тут робиш mount.syncTo(ra_deg, dec_deg);
    // з GUI у нас поки тільки рядки
    // ти можеш зберігати останній solved RA/DEC у змінних цього класу і тут його відправляти
}

void MainWindow::onMountSlew()
{
    // TODO: парсиш mountRaEdit_, mountDecEdit_ і робиш mount.slew(...)
}

void MainWindow::updateAstrometryImage(const QImage &img)
{
    astrometryLabel_->setPixmap(QPixmap::fromImage(img).scaled(astrometryLabel_->size(), Qt::KeepAspectRatio));
}
