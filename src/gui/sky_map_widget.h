#pragma once

#include "core/astro_types.h"
#include "core/equatorial_frames.h"
#include <QDateTime>
#include <QPoint>
#include <QPointF>
#include <QWidget>
#include <optional>
#include <vector>

namespace oas {

class SkyMapWidget final : public QWidget {
    Q_OBJECT
public:
    explicit SkyMapWidget(QWidget *parent = nullptr);

    void setObserver(const ObserverLocation &observer);
    void setUtc(const QDateTime &utc);
    void setMountCoordinate(const EquatorialCoord &coord, bool valid);
    void setSolvedCoordinate(const EquatorialCoord &coord, bool valid);
    void setFovDegrees(double widthDeg, double heightDeg);
    void setShowLabels(bool enabled);
    void setShowDsos(bool enabled);
    void setShowConstellations(bool enabled);

    bool selectObjectByName(const QString &text);
    void focusSelected();
    void resetView();

    bool hasSelection() const;
    QString selectedName() const;
    EquatorialCoord selectedCoordinate() const;
    HorizontalCoord selectedHorizontal() const;
    QString selectedKind() const;
    double selectedMagnitude() const;

signals:
    void selectionChanged(const QString &name, double raDeg, double decDeg);
    void objectActivated(const QString &name, double raDeg, double decDeg);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct SkyObject {
        QString name;
        QString kind;
        double raDeg{0.0};
        double decDeg{0.0};
        double magnitude{99.0};
    };
    struct ConstellationSegment { int a{-1}; int b{-1}; };

    static const std::vector<SkyObject> &catalog();
    static const std::vector<ConstellationSegment> &constellationSegments();
    static int objectIndex(const QString &name);

    QPointF projectHorizontal(const HorizontalCoord &horizontal) const;
    std::optional<int> nearestObject(const QPointF &point, double radiusPx) const;
    void setSelectedIndex(int index, bool emitSignal = true);
    double skyRadius() const;
    QPointF skyCenter() const;

    ObserverLocation observer_{};
    QDateTime utc_{QDateTime::currentDateTimeUtc()};
    std::optional<EquatorialCoord> mountCoordinate_;
    std::optional<EquatorialCoord> solvedCoordinate_;
    double fovWidthDeg_{0.0};
    double fovHeightDeg_{0.0};
    bool showLabels_{true};
    bool showDsos_{true};
    bool showConstellations_{true};
    int selectedIndex_{-1};
    double zoom_{1.0};
    QPointF panOffset_{};
    QPoint dragStart_{};
    QPointF panStart_{};
    bool dragging_{false};
    bool movedDuringDrag_{false};
};

} // namespace oas
