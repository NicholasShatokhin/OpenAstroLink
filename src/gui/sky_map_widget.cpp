#include "gui/sky_map_widget.h"

#include <QLineF>
#include <QMouseEvent>
#include <QPolygonF>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace oas {
namespace {
constexpr double kPi = 3.14159265358979323846;

QString normalizedName(const QString &text) {
    return text.trimmed().toCaseFolded();
}

QColor starColor(double magnitude) {
    const int value = std::clamp(int(255.0 - std::max(0.0, magnitude) * 15.0), 150, 255);
    return QColor(value, value, std::min(255, value + 20));
}
} // namespace

SkyMapWidget::SkyMapWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(460, 360);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip("Drag to pan, mouse wheel to zoom, click to select, double-click to slew");
}

void SkyMapWidget::setObserver(const ObserverLocation &observer) { observer_ = observer; update(); }
void SkyMapWidget::setUtc(const QDateTime &utc) { utc_ = utc.toUTC(); update(); }
void SkyMapWidget::setMountCoordinate(const EquatorialCoord &coord, bool valid) { if(valid) mountCoordinate_ = coord; else mountCoordinate_.reset(); update(); }
void SkyMapWidget::setSolvedCoordinate(const EquatorialCoord &coord, bool valid) { if(valid) solvedCoordinate_ = coord; else solvedCoordinate_.reset(); update(); }
void SkyMapWidget::setFovDegrees(double widthDeg, double heightDeg) { fovWidthDeg_ = std::max(0.0, widthDeg); fovHeightDeg_ = std::max(0.0, heightDeg); update(); }
void SkyMapWidget::setShowLabels(bool enabled) { showLabels_ = enabled; update(); }
void SkyMapWidget::setShowDsos(bool enabled) { showDsos_ = enabled; update(); }
void SkyMapWidget::setShowConstellations(bool enabled) { showConstellations_ = enabled; update(); }

bool SkyMapWidget::hasSelection() const { return selectedIndex_ >= 0 && selectedIndex_ < int(catalog().size()); }
QString SkyMapWidget::selectedName() const { return hasSelection() ? catalog()[size_t(selectedIndex_)].name : QString{}; }
QString SkyMapWidget::selectedKind() const { return hasSelection() ? catalog()[size_t(selectedIndex_)].kind : QString{}; }
double SkyMapWidget::selectedMagnitude() const { return hasSelection() ? catalog()[size_t(selectedIndex_)].magnitude : 99.0; }
EquatorialCoord SkyMapWidget::selectedCoordinate() const {
    if(!hasSelection()) return {};
    const auto &o = catalog()[size_t(selectedIndex_)];
    return {o.raDeg, o.decDeg, EquatorialFrame::J2000};
}
HorizontalCoord SkyMapWidget::selectedHorizontal() const { return hasSelection() ? equatorialToHorizontal(selectedCoordinate(), observer_, utc_) : HorizontalCoord{}; }

bool SkyMapWidget::selectObjectByName(const QString &text) {
    const QString wanted = normalizedName(text);
    if(wanted.isEmpty()) return false;
    const auto &objects = catalog();
    int exact = -1;
    int prefix = -1;
    int contains = -1;
    for(int i = 0; i < int(objects.size()); ++i) {
        const QString n = normalizedName(objects[size_t(i)].name);
        if(n == wanted) { exact = i; break; }
        if(prefix < 0 && n.startsWith(wanted)) prefix = i;
        if(contains < 0 && n.contains(wanted)) contains = i;
    }
    const int chosen = exact >= 0 ? exact : prefix >= 0 ? prefix : contains;
    if(chosen < 0) return false;
    setSelectedIndex(chosen);
    return true;
}

void SkyMapWidget::focusSelected() {
    if(!hasSelection()) return;
    const auto h = selectedHorizontal();
    if(h.altDeg < -5.0) return;
    zoom_ = std::max(zoom_, 2.0);
    const QPointF before = projectHorizontal(h);
    panOffset_ += rect().center() - before;
    update();
}

void SkyMapWidget::resetView() {
    zoom_ = 1.0;
    panOffset_ = {};
    update();
}

double SkyMapWidget::skyRadius() const {
    return std::max(20.0, 0.5 * std::min(width(), height()) - 30.0) * zoom_;
}
QPointF SkyMapWidget::skyCenter() const { return QPointF(width() * 0.5, height() * 0.5) + panOffset_; }

QPointF SkyMapWidget::projectHorizontal(const HorizontalCoord &horizontal) const {
    const double altitude = std::clamp(horizontal.altDeg, -90.0, 90.0);
    const double radial = (90.0 - altitude) / 90.0 * skyRadius();
    const double az = horizontal.azDeg * kPi / 180.0;
    const QPointF c = skyCenter();
    return {c.x() + radial * std::sin(az), c.y() - radial * std::cos(az)};
}

std::optional<int> SkyMapWidget::nearestObject(const QPointF &point, double radiusPx) const {
    const auto &objects = catalog();
    double best = radiusPx;
    int bestIndex = -1;
    for(int i = 0; i < int(objects.size()); ++i) {
        const auto &o = objects[size_t(i)];
        if(o.kind == "DSO" && !showDsos_) continue;
        const auto h = equatorialToHorizontal({o.raDeg, o.decDeg, EquatorialFrame::J2000}, observer_, utc_);
        if(h.altDeg < 0.0) continue;
        const double d = QLineF(point, projectHorizontal(h)).length();
        if(d < best) { best = d; bestIndex = i; }
    }
    if(bestIndex < 0) return std::nullopt;
    return bestIndex;
}

void SkyMapWidget::setSelectedIndex(int index, bool emitSignal) {
    if(index < 0 || index >= int(catalog().size())) return;
    selectedIndex_ = index;
    update();
    if(emitSignal) {
        const auto &o = catalog()[size_t(index)];
        emit selectionChanged(o.name, o.raDeg, o.decDeg);
    }
}

void SkyMapWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(3, 5, 14));

    const QPointF c = skyCenter();
    const double r = skyRadius();
    p.setPen(QPen(QColor(60, 90, 120), 1.2));
    p.setBrush(QColor(5, 10, 24));
    p.drawEllipse(c, r, r);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(42, 62, 85), 1.0, Qt::DashLine));
    for(double alt : {30.0, 60.0}) {
        const double rr = (90.0 - alt) / 90.0 * r;
        p.drawEllipse(c, rr, rr);
    }
    for(double az : {0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0}) {
        const double a = az * kPi / 180.0;
        const QPointF e{c.x() + r * std::sin(a), c.y() - r * std::cos(a)};
        p.drawLine(c, e);
    }

    p.setPen(QColor(130, 180, 220));
    QFont directionFont = p.font();
    directionFont.setBold(true);
    p.setFont(directionFont);
    const auto drawDir = [&](const QString &text, double az) {
        const double a = az * kPi / 180.0;
        const QPointF q{c.x() + (r + 15.0) * std::sin(a), c.y() - (r + 15.0) * std::cos(a)};
        p.drawText(QRectF(q.x() - 14.0, q.y() - 10.0, 28.0, 20.0), Qt::AlignCenter, text);
    };
    drawDir("N", 0); drawDir("E", 90); drawDir("S", 180); drawDir("W", 270);
    directionFont.setBold(false);
    p.setFont(directionFont);

    const auto &objects = catalog();
    if(showConstellations_) {
        p.setPen(QPen(QColor(55, 78, 115), 1.0));
        for(const auto &seg : constellationSegments()) {
            if(seg.a < 0 || seg.b < 0 || seg.a >= int(objects.size()) || seg.b >= int(objects.size())) continue;
            const auto &a = objects[size_t(seg.a)]; const auto &b = objects[size_t(seg.b)];
            const auto ha = equatorialToHorizontal({a.raDeg,a.decDeg,EquatorialFrame::J2000}, observer_, utc_);
            const auto hb = equatorialToHorizontal({b.raDeg,b.decDeg,EquatorialFrame::J2000}, observer_, utc_);
            if(ha.altDeg >= 0.0 && hb.altDeg >= 0.0) p.drawLine(projectHorizontal(ha), projectHorizontal(hb));
        }
    }

    for(int i = int(objects.size()) - 1; i >= 0; --i) {
        const auto &o = objects[size_t(i)];
        if(o.kind == "DSO" && !showDsos_) continue;
        const auto h = equatorialToHorizontal({o.raDeg,o.decDeg,EquatorialFrame::J2000}, observer_, utc_);
        if(h.altDeg < 0.0) continue;
        const QPointF q = projectHorizontal(h);
        if(!rect().adjusted(-20,-20,20,20).contains(q.toPoint())) continue;
        if(o.kind == "DSO") {
            p.setPen(QPen(QColor(80, 210, 220), i == selectedIndex_ ? 2.5 : 1.2));
            p.setBrush(Qt::NoBrush);
            const double rr = i == selectedIndex_ ? 6.5 : 4.5;
            p.drawEllipse(q, rr, rr * 0.72);
        } else {
            const double rr = std::clamp(4.8 - o.magnitude * 0.95, 1.2, 5.5);
            p.setPen(Qt::NoPen);
            p.setBrush(starColor(o.magnitude));
            p.drawEllipse(q, rr, rr);
            if(i == selectedIndex_) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor(255, 205, 70), 2.0));
                p.drawEllipse(q, rr + 5.0, rr + 5.0);
            }
        }
        if(showLabels_ && (i == selectedIndex_ || o.kind == "DSO" || o.magnitude <= 1.8)) {
            p.setPen(i == selectedIndex_ ? QColor(255, 215, 90) : QColor(180, 200, 220));
            p.drawText(q + QPointF(7.0, -5.0), o.name);
        }
    }

    const auto drawMarker = [&](const std::optional<EquatorialCoord> &coord, const QColor &color, const QString &label, bool cross) {
        if(!coord) return;
        const auto h = equatorialToHorizontal(*coord, observer_, utc_);
        if(h.altDeg < 0.0) return;
        const QPointF q = projectHorizontal(h);
        p.setPen(QPen(color, 2.0));
        p.setBrush(Qt::NoBrush);
        if(cross) { p.drawLine(q + QPointF(-8,0), q + QPointF(8,0)); p.drawLine(q + QPointF(0,-8), q + QPointF(0,8)); p.drawEllipse(q, 5, 5); }
        else { QPolygonF d; d << q + QPointF(0,-7) << q + QPointF(7,0) << q + QPointF(0,7) << q + QPointF(-7,0); p.drawPolygon(d); }
        p.drawText(q + QPointF(10,-9), label);
    };
    drawMarker(mountCoordinate_, QColor(255, 90, 90), "Telescope", true);
    drawMarker(solvedCoordinate_, QColor(80, 230, 130), "Solved", false);

    if(mountCoordinate_ && fovWidthDeg_ > 0.0 && fovHeightDeg_ > 0.0) {
        const auto h = equatorialToHorizontal(*mountCoordinate_, observer_, utc_);
        if(h.altDeg >= 0.0) {
            const QPointF q = projectHorizontal(h);
            const double pxPerDeg = r / 90.0;
            p.setPen(QPen(QColor(255, 120, 120, 170), 1.2, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(q, std::max(2.0, 0.5 * fovWidthDeg_ * pxPerDeg), std::max(2.0, 0.5 * fovHeightDeg_ * pxPerDeg));
        }
    }

    p.setPen(QColor(120, 145, 170));
    p.drawText(10, height() - 12, QString("UTC %1   site %2°, %3°   zoom %4×")
               .arg(utc_.toString("yyyy-MM-dd HH:mm:ss"))
               .arg(observer_.latitudeDeg,0,'f',3).arg(observer_.longitudeDeg,0,'f',3).arg(zoom_,0,'f',1));
}

void SkyMapWidget::mousePressEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        dragStart_ = event->position().toPoint();
        panStart_ = panOffset_;
        dragging_ = true;
        movedDuringDrag_ = false;
    }
    QWidget::mousePressEvent(event);
}
void SkyMapWidget::mouseMoveEvent(QMouseEvent *event) {
    if(dragging_ && (event->buttons() & Qt::LeftButton)) {
        const QPoint delta = event->position().toPoint() - dragStart_;
        if(delta.manhattanLength() > 4) movedDuringDrag_ = true;
        if(movedDuringDrag_) { panOffset_ = panStart_ + QPointF(delta); update(); }
    }
    QWidget::mouseMoveEvent(event);
}
void SkyMapWidget::mouseReleaseEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        if(!movedDuringDrag_) {
            if(const auto nearest = nearestObject(event->position(), 13.0)) setSelectedIndex(*nearest);
        }
    }
    QWidget::mouseReleaseEvent(event);
}
void SkyMapWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        if(const auto nearest = nearestObject(event->position(), 16.0)) {
            setSelectedIndex(*nearest);
            const auto &o = catalog()[size_t(*nearest)];
            emit objectActivated(o.name, o.raDeg, o.decDeg);
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}
void SkyMapWidget::wheelEvent(QWheelEvent *event) {
    const double factor = event->angleDelta().y() > 0 ? 1.18 : 1.0 / 1.18;
    zoom_ = std::clamp(zoom_ * factor, 0.75, 4.5);
    update();
    event->accept();
}

int SkyMapWidget::objectIndex(const QString &name) {
    const QString wanted = normalizedName(name);
    const auto &objects = catalog();
    for(int i = 0; i < int(objects.size()); ++i) if(normalizedName(objects[size_t(i)].name) == wanted) return i;
    return -1;
}

const std::vector<SkyMapWidget::SkyObject> &SkyMapWidget::catalog() {
    static const std::vector<SkyObject> objects = {
        {"Sirius","Star",101.2872,-16.7161,-1.46},{"Canopus","Star",95.9879,-52.6957,-0.74},{"Arcturus","Star",213.9153,19.1824,-0.05},
        {"Vega","Star",279.2347,38.7837,0.03},{"Capella","Star",79.1723,45.9979,0.08},{"Rigel","Star",78.6345,-8.2016,0.12},
        {"Procyon","Star",114.8255,5.2250,0.34},{"Betelgeuse","Star",88.7929,7.4071,0.42},{"Achernar","Star",24.4286,-57.2368,0.46},
        {"Hadar","Star",210.9559,-60.3730,0.61},{"Acrux","Star",186.6496,-63.0991,0.76},{"Altair","Star",297.6958,8.8683,0.77},
        {"Aldebaran","Star",68.9800,16.5093,0.85},{"Antares","Star",247.3519,-26.4320,0.96},{"Spica","Star",201.2983,-11.1613,0.98},
        {"Pollux","Star",116.3289,28.0262,1.14},{"Fomalhaut","Star",344.4128,-29.6222,1.16},{"Deneb","Star",310.3579,45.2803,1.25},
        {"Regulus","Star",152.0936,11.9672,1.35},{"Adhara","Star",104.6565,-28.9721,1.50},{"Castor","Star",113.6494,31.8883,1.58},
        {"Shaula","Star",263.4022,-37.1038,1.62},{"Bellatrix","Star",81.2828,6.3497,1.64},{"Elnath","Star",81.5729,28.6075,1.65},
        {"Miaplacidus","Star",138.3008,-69.7172,1.67},{"Alnilam","Star",84.0534,-1.2019,1.69},{"Alnair","Star",332.0583,-46.9609,1.74},
        {"Alnitak","Star",85.1897,-1.9426,1.74},{"Alioth","Star",193.5073,55.9598,1.76},{"Dubhe","Star",165.9319,61.7510,1.79},
        {"Kaus Australis","Star",283.8163,-34.3846,1.79},{"Mirfak","Star",51.0807,49.8612,1.79},{"Alkaid","Star",206.8852,49.3133,1.85},
        {"Menkalinan","Star",89.8822,44.9474,1.90},{"Atria","Star",252.1662,-69.0277,1.91},{"Alhena","Star",99.4279,16.3993,1.93},
        {"Peacock","Star",306.4119,-56.7351,1.94},{"Polaris","Star",37.9546,89.2641,1.98},{"Mirzam","Star",95.6749,-17.9559,1.98},
        {"Hamal","Star",31.7934,23.4624,2.00},{"Diphda","Star",10.8968,-17.9866,2.04},{"Saiph","Star",86.9391,-9.6696,2.07},
        {"Mizar","Star",200.9814,54.9254,2.23},{"Mintaka","Star",83.0017,-0.2991,2.23},{"Schedar","Star",10.1268,56.5373,2.24},
        {"Caph","Star",2.2945,59.1498,2.28},{"Merak","Star",165.4603,56.3824,2.37},{"Phecda","Star",178.4577,53.6948,2.44},
        {"Gamma Cas","Star",14.1771,60.7167,2.47},{"Sadr","Star",305.5571,40.2567,2.23},{"Gienah Cyg","Star",311.5528,33.9703,2.46},
        {"Ruchbah","Star",21.4540,60.2353,2.68},{"Delta Cyg","Star",296.2437,45.1308,2.87},{"Albireo","Star",292.6803,27.9597,3.05},
        {"Sulafat","Star",284.7359,32.6896,3.25},{"Megrez","Star",183.8565,57.0326,3.31},{"Segin","Star",28.5989,63.6701,3.35},
        {"Sheliak","Star",282.5199,33.3627,3.52},
        {"M31 Andromeda Galaxy","DSO",10.6847,41.2690,3.44},{"M42 Orion Nebula","DSO",83.8221,-5.3911,4.00},{"M45 Pleiades","DSO",56.7500,24.1167,1.60},
        {"M13 Hercules Cluster","DSO",250.4235,36.4613,5.80},{"M57 Ring Nebula","DSO",283.3962,33.0292,8.80},{"M27 Dumbbell Nebula","DSO",299.9017,22.7210,7.50},
        {"M51 Whirlpool Galaxy","DSO",202.4696,47.1952,8.40},{"M81 Bode Galaxy","DSO",148.8882,69.0653,6.90},{"M82 Cigar Galaxy","DSO",148.9685,69.6797,8.40},
        {"M101 Pinwheel Galaxy","DSO",210.8023,54.3489,7.90},{"M8 Lagoon Nebula","DSO",270.9250,-24.3800,6.00},{"M20 Trifid Nebula","DSO",270.6500,-23.0300,6.30}
    };
    return objects;
}

const std::vector<SkyMapWidget::ConstellationSegment> &SkyMapWidget::constellationSegments() {
    static const std::vector<ConstellationSegment> segments = [] {
        std::vector<ConstellationSegment> s;
        auto add = [&](const char *a, const char *b) { const int ia = objectIndex(a), ib = objectIndex(b); if(ia >= 0 && ib >= 0) s.push_back({ia,ib}); };
        // Summer Triangle / Cygnus / Lyra
        add("Vega","Deneb"); add("Deneb","Altair"); add("Altair","Vega");
        add("Deneb","Sadr"); add("Sadr","Albireo"); add("Sadr","Gienah Cyg"); add("Sadr","Delta Cyg");
        add("Vega","Sheliak"); add("Vega","Sulafat"); add("Sheliak","Sulafat");
        // Orion
        add("Betelgeuse","Bellatrix"); add("Bellatrix","Mintaka"); add("Mintaka","Alnilam"); add("Alnilam","Alnitak"); add("Alnitak","Saiph"); add("Saiph","Rigel"); add("Rigel","Bellatrix"); add("Betelgeuse","Alnitak");
        // Big Dipper
        add("Dubhe","Merak"); add("Merak","Phecda"); add("Phecda","Megrez"); add("Megrez","Alioth"); add("Alioth","Mizar"); add("Mizar","Alkaid"); add("Megrez","Dubhe");
        // Cassiopeia W
        add("Caph","Schedar"); add("Schedar","Gamma Cas"); add("Gamma Cas","Ruchbah"); add("Ruchbah","Segin");
        return s;
    }();
    return segments;
}

} // namespace oas
