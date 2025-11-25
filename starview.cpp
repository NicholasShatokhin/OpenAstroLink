#include "starview.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>

StarView::StarView(QWidget *parent)
    : QGraphicsView(parent),
      scene_(new QGraphicsScene(this))
{
    setScene(scene_);
    scene_->setSceneRect(-200, -200, 400, 400);
}

void StarView::setStars(const std::vector<StarPoint> &stars)
{
    scene_->clear();
    // фон
    scene_->addRect(scene_->sceneRect(), QPen(Qt::darkBlue), QBrush(Qt::black));
    for (const auto &s : stars) {
        double r = 3.0;
        auto *dot = scene_->addEllipse(s.pos.x()-r, s.pos.y()-r, 2*r, 2*r,
                                       QPen(Qt::yellow), QBrush(Qt::yellow));
        dot->setToolTip(QString("mag=%1").arg(s.mag));
    }
    // підпис центра поля (якщо є)
    auto *text = scene_->addText(QString("RA= %1°, DEC= %2°").arg(raDeg_).arg(decDeg_));
    text->setDefaultTextColor(Qt::white);
    text->setPos(scene_->sceneRect().topLeft() + QPointF(5,5));
}

void StarView::setSolvedCenter(double raDeg, double decDeg)
{
    raDeg_ = raDeg;
    decDeg_ = decDeg;
}
