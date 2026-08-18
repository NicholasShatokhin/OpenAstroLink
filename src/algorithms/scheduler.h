#pragma once
#include "core/astro_types.h"
#include <QObject>

namespace oas {
class Scheduler : public QObject {
    Q_OBJECT
public:
    explicit Scheduler(QObject *parent=nullptr):QObject(parent){}
    void setPlan(QString name,std::vector<SessionTarget> targets);
    SessionStatus status() const{return status_;}
    std::optional<SessionTarget> currentTarget() const;
    bool start();void stop();void markFrameCompleted();void advanceTarget();
signals:void statusChanged(const oas::SessionStatus&);
private:std::vector<SessionTarget> targets_;SessionStatus status_{};
};
}
Q_DECLARE_METATYPE(oas::SessionStatus)
