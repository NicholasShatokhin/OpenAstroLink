#pragma once

#include "core/astro_types.h"
#include <QObject>
#include <optional>
#include <QHash>
#include <deque>

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

    bool start(int blockIndex = 0);
    bool beginScheduled();
    void stop(const QString &reason = {});
    void fail(const QString &message);
    void setStep(const QString &step, const QString &operationId = {});
    void clearOperation();
    void markFrameCompleted();
    void advanceBlock();
    bool deferCurrentBlock();
    int pendingBlockCount() const { return int(pendingOrder_.size()); }
    // Compatibility hint for the existing linear crash-resume cursor.  When
    // Observable Sky reorders blocks at runtime this returns the earliest
    // unfinished plan index, so a crash may repeat a later completed block but
    // never silently skip a deferred unfinished one.  A durable per-block
    // completion journal remains an OAL 1.0 item.
    int resumeIndexHint() const;

signals:
    void statusChanged(const oas::SessionStatus &status);

private:
    void prepareCurrentBlockStart(bool firstBlock);
    void refreshCurrentBlockFields();
    void publish();

    ObservationPlan plan_;
    SessionStatus status_{};
    std::deque<int> pendingOrder_;
    QHash<int,int> runtimeProgress_;
};

} // namespace oas

Q_DECLARE_METATYPE(oas::SessionStatus)
