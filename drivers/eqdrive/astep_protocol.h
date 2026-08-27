#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cmath>
#include <optional>

namespace oal::eqdrive {

struct Status {
    quint16 state{0};
    double axis1Deg{0.0};
    double axis2Deg{0.0};
    double speed1DegPerHour{0.0};
    double speed2DegPerHour{0.0};
    double unixTime{0.0};
    bool gotoActive1{false};
    bool gotoActive2{false};
    bool driverEnabled1{false};
    bool driverEnabled2{false};
};

inline QByteArray line(const QByteArray &command) {
    QByteArray out = command;
    if (!out.endsWith('\r')) out.append('\r');
    return out;
}

inline QByteArray trimLine(QByteArray value) {
    while (value.endsWith('\r') || value.endsWith('\n')) value.chop(1);
    return value.trimmed();
}

inline bool responseOk(const QByteArray &reply, const QByteArray &prefix) {
    const QByteArray t = trimLine(reply);
    return t == prefix + " OK" || t == prefix || t.endsWith(" OK");
}

inline std::optional<Status> parseStatus(const QByteArray &reply) {
    const QStringList fields = QString::fromLatin1(trimLine(reply)).split(' ', Qt::SkipEmptyParts);
    if (fields.size() < 7 || fields[0] != QStringLiteral("St")) return std::nullopt;
    bool okState=false, ok1=false, ok2=false, okS1=false, okS2=false, okTime=false;
    const quint16 state = fields[1].toUShort(&okState, 16);
    const double p1 = fields[2].toDouble(&ok1);
    const double p2 = fields[3].toDouble(&ok2);
    const double s1 = fields[4].toDouble(&okS1);
    const double s2 = fields[5].toDouble(&okS2);
    const double tm = fields[6].toDouble(&okTime);
    if (!okState || !ok1 || !ok2 || !okS1 || !okS2) return std::nullopt;
    Status s;
    s.state=state; s.axis1Deg=p1; s.axis2Deg=p2; s.speed1DegPerHour=s1; s.speed2DegPerHour=s2;
    s.unixTime=okTime?tm:0.0;
    // ASTEP documents Axis1 in the high byte (bits 15..8) and Axis2 in
    // the low byte (bits 7..0). Goto is bit 0 inside each axis byte and
    // Drv is bit 4 inside each axis byte.
    s.gotoActive1=(state & 0x0100u)!=0; s.gotoActive2=(state & 0x0001u)!=0;
    s.driverEnabled1=(state & 0x1000u)!=0; s.driverEnabled2=(state & 0x0010u)!=0;
    return s;
}

inline std::optional<std::pair<double,double>> parsePosition(const QByteArray &reply) {
    const QStringList f=QString::fromLatin1(trimLine(reply)).split(' ',Qt::SkipEmptyParts);
    if (f.size()<3 || f[0]!=QStringLiteral("Pos")) return std::nullopt;
    bool a=false,b=false; const double p1=f[1].toDouble(&a),p2=f[2].toDouble(&b);
    if(!a||!b)return std::nullopt; return std::pair<double,double>{p1,p2};
}

inline std::optional<std::pair<double,double>> parseSpeed(const QByteArray &reply) {
    const QStringList f=QString::fromLatin1(trimLine(reply)).split(' ',Qt::SkipEmptyParts);
    if (f.size()<3 || f[0]!=QStringLiteral("Speed")) return std::nullopt;
    bool a=false,b=false; const double s1=f[1].toDouble(&a),s2=f[2].toDouble(&b);
    if(!a||!b)return std::nullopt; return std::pair<double,double>{s1,s2};
}

inline QString firmwareText(const QByteArray &reply) {
    const QByteArray t=trimLine(reply);
    if (t.startsWith("FWB ") || t.startsWith("FWA ")) return QString::fromUtf8(t.mid(4));
    return QString::fromUtf8(t);
}

inline bool looksLikeEqDrive(const QByteArray &reply) {
    const QByteArray t=trimLine(reply).toLower();
    return (t.startsWith("fwb ") || t.startsWith("fwa ")) &&
           (t.contains("eqd") || t.contains("eqdrive"));
}

inline std::pair<int,int> parseAxisDirections(const QByteArray &reply) {
    const QStringList f=QString::fromLatin1(trimLine(reply)).split(' ',Qt::SkipEmptyParts);
    if(f.size()<14 || f[0]!=QStringLiteral("Cg"))return {1,1};
    auto sign=[](const QString&s){return s.startsWith('-')?-1:1;};
    return {sign(f[1]),sign(f[13])};
}

inline double wrap360(double x){x=std::fmod(x,360.0);return x<0?x+360.0:x;}
inline double wrap180(double x){x=wrap360(x);return x>180.0?x-360.0:x;}

} // namespace oal::eqdrive
