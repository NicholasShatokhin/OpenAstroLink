#pragma once
#include <QWidget>

class QLineEdit;
class QPushButton;

class ControlPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ControlPanel(QWidget *parent = nullptr);

signals:
    void requestCapture();
    void requestSolve();
    void requestMountSync();
    void requestMountSlew(const QString &ra, const QString &dec);
    void requestNextTarget();
    void requestRunNN();

private:
    QLineEdit *mountRaEdit_;
    QLineEdit *mountDecEdit_;
};
