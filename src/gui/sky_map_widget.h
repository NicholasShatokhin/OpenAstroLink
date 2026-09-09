#pragma once

#include "core/astro_types.h"
#include "core/equatorial_frames.h"
#include <QDateTime>
#include <QColor>
#include <QPainter>
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
    void setSolvedFrame(const SkyFrame &frame);
    void setMainPlannedFrame(const SkyFrame &frame);
    void setGuidePlannedFrame(const SkyFrame &frame);
    void setShowSolvedFrame(bool enabled);
    void setShowMainFrame(bool enabled);
    void setShowGuideFrame(bool enabled);
    void setShowFrameLabels(bool enabled);
    void setPlannerMosaic(const SkyFrame &mainFrame, int columns, int rows, double overlapPercent, bool enabled);
    SkyFrame solvedFrame() const { return solvedFrame_; }
    SkyFrame mainPlannedFrame() const { return mainPlannedFrame_; }
    SkyFrame guidePlannedFrame() const { return guidePlannedFrame_; }
    SkyFrame plannerEnvelopeFrame() const;
    void setShowLabels(bool enabled);
    void setShowDsos(bool enabled);
    void setShowConstellations(bool enabled);
    void setNightVisionMode(bool enabled,bool strict=false) { if(nightVisionMode_!=enabled||strictNightMode_!=strict){ nightVisionMode_=enabled; strictNightMode_=strict; update(); } }

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
    struct PlannerGrid {
        bool enabled{false};
        SkyFrame mainFrame{};
        int columns{1};
        int rows{1};
        double overlapPercent{15.0};
    };

    static const std::vector<SkyObject> &catalog();
    static const std::vector<ConstellationSegment> &constellationSegments();
    static int objectIndex(const QString &name);

    QPointF projectHorizontal(const HorizontalCoord &horizontal) const;
    std::optional<HorizontalCoord> unprojectHorizontal(const QPointF &point) const;
    std::optional<int> nearestObject(const QPointF &point, double radiusPx) const;
    void setSelectedIndex(int index, bool emitSignal = true);
    bool setCustomSelectionAtPoint(const QPointF &point, bool emitSignal = true);
    double skyRadius() const;
    QPointF skyCenter() const;
    std::vector<EquatorialCoord> frameCorners(const SkyFrame &frame) const;
    void drawSkyFrame(QPainter &p, const SkyFrame &frame, const QColor &color, Qt::PenStyle style, const QString &fallbackLabel) const;
    std::vector<SkyFrame> plannerTiles() const;

    ObserverLocation observer_{};
    QDateTime utc_{QDateTime::currentDateTimeUtc()};
    std::optional<EquatorialCoord> mountCoordinate_;
    std::optional<EquatorialCoord> solvedCoordinate_;
    SkyFrame solvedFrame_{};
    SkyFrame mainPlannedFrame_{};
    SkyFrame guidePlannedFrame_{};
    PlannerGrid planner_{};
    bool showSolvedFrame_{true};
    bool showMainFrame_{true};
    bool showGuideFrame_{false};
    bool showFrameLabels_{true};
    bool showLabels_{true};
    bool showDsos_{true};
    bool showConstellations_{true};
    bool nightVisionMode_{false};
    bool strictNightMode_{false};
    int selectedIndex_{-1};
    std::optional<EquatorialCoord> customSelection_;
    double zoom_{1.0};
    QPointF panOffset_{};
    QPoint dragStart_{};
    QPointF panStart_{};
    bool dragging_{false};
    bool movedDuringDrag_{false};
};

} // namespace oas
