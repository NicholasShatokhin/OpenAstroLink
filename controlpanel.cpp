#include "controlpanel.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>

ControlPanel::ControlPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);

    auto *btnCap  = new QPushButton("Зняти кадр", this);
    auto *btnSolve= new QPushButton("Solve", this);
    auto *btnNN   = new QPushButton("NN Solve", this);

    mountRaEdit_  = new QLineEdit("0:0:0", this);
    mountDecEdit_ = new QLineEdit("+00*00", this);
    auto *btnSync = new QPushButton("Sync mount", this);
    auto *btnSlew = new QPushButton("Slew", this);
    auto *btnNext = new QPushButton("Next target", this);

    lay->addWidget(btnCap);
    lay->addWidget(btnSolve);
    lay->addWidget(btnNN);
    lay->addWidget(new QLabel("RA:", this));
    lay->addWidget(mountRaEdit_);
    lay->addWidget(new QLabel("DEC:", this));
    lay->addWidget(mountDecEdit_);
    lay->addWidget(btnSync);
    lay->addWidget(btnSlew);
    lay->addWidget(btnNext);
    lay->addStretch(1);

    connect(btnCap,  &QPushButton::clicked, this, &ControlPanel::requestCapture);
    connect(btnSolve,&QPushButton::clicked, this, &ControlPanel::requestSolve);
    connect(btnNN,   &QPushButton::clicked, this, &ControlPanel::requestRunNN);
    connect(btnSync, &QPushButton::clicked, this, &ControlPanel::requestMountSync);
    connect(btnSlew, &QPushButton::clicked, this, [this]{
        emit requestMountSlew(mountRaEdit_->text(), mountDecEdit_->text());
    });
    connect(btnNext, &QPushButton::clicked, this, &ControlPanel::requestNextTarget);
}
