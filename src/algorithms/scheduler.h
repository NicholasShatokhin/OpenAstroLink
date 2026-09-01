#pragma once

#include "core/astro_types.h"
#include <QObject>
#include <optional>

namespace oas {

// Node-side scheduler state holder. Hardware work is intentionally submitted
// through ApplicationController/OperationManager so long actions keep using the
// same async resource-locking model as interactive commands.
class Scheduler final : public QObject {
    Q_OBJECT
public:
    explicit Scheduler(QObject *parent = nullptr) : QObject(parent) {}

    void setPlan(ObservationPlan plan);
    void setLegacyPlan(QString name, const std::vector<SessionTarget> &targets);

    const ObservationPlan &plan() const { return plan_; }
    SessionStatus status() const { return status_; }
    std::optional<ObservationBlock> currentBlock() const;

    bool start();
    void stop(const QString &reason = {});
    void fail(const QString &message);
    void setStep(const QString &step, const QString &operationId = {});
    void clearOperation();
    void markFrameCompleted();
    void advanceBlock();

signals:
    void statusChanged(const oas::SessionStatus &status);

private:
    void refreshCurrentBlockFields();
    void publish();

    ObservationPlan plan_;
    SessionStatus status_{};
};

} // namespace oas

Q_DECLARE_METATYPE(oas::SessionStatus)
