#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPointF>
#include <vector>

struct StarPoint {
    QPointF pos;
    double mag;
};

class StarView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit StarView(QWidget *parent = nullptr);
public slots:
    void setStars(const std::vector<StarPoint> &stars);
    void setSolvedCenter(double raDeg, double decDeg);
private:
    QGraphicsScene *scene_;
    double raDeg_ = 0.0;
    double decDeg_ = 0.0;
};
