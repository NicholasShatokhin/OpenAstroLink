#include "core/operation_manager.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QEventLoop>
#include <QEvent>
#include <QMutexLocker>
#include <QList>
#include <QUuid>
#include <QThread>
#include <algorithm>
#include <exception>

namespace oas {

bool OperationContext::isCancellationRequested() const {
    return cancelFlag_ && cancelFlag_->load(std::memory_order_relaxed);
}

void OperationContext::reportProgress(double progress, const QString &phase, const QJsonObject &detail) const {
    if (progress_) progress_(std::clamp(progress, 0.0, 1.0), phase, detail);
}

OperationManager::OperationManager(QObject *parent) : QObject(parent) {
    pool_.setMaxThreadCount(std::max(2, QThread::idealThreadCount()));
}

OperationManager::~OperationManager() { shutdown(); }
void OperationManager::shutdown() {
    {
        QMutexLocker lock(&mutex_);
        if(shuttingDown_) return;
        shuttingDown_ = true;
        for (auto it = records_.begin(); it != records_.end(); ++it) {
            if (it->state == "running" || it->state == "queued") it->cancelFlag->store(true, std::memory_order_relaxed);
        }
    }
    while(!pool_.waitForDone(25)){
        if(QCoreApplication::instance())QCoreApplication::processEvents(QEventLoop::AllEvents,25);
    }
    // A worker may post its final queued signal immediately before leaving the
    // pool. Drain that tail while the main event dispatcher is still valid.
    if(QCoreApplication::instance()){
        QCoreApplication::processEvents(QEventLoop::AllEvents,25);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    }
}

QString OperationManager::nowUtc() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QJsonObject OperationManager::toJson(const Record &r) {
    QJsonArray locks; for (const auto &x : r.resources) locks.append(x);
    QJsonObject o{{"id",r.id},{"kind",r.kind},{"state",r.state},{"progress",r.progress},
                  {"createdAtUtc",r.createdAtUtc},{"updatedAtUtc",r.updatedAtUtc},
                  {"cancelSupported",r.cancelSupported},{"cancelRequested",r.cancelRequested},
                  {"resourceLocks",locks}};
    if (!r.phase.isEmpty()) o["phase"] = r.phase;
    if (!r.progressDetail.isEmpty()) o["progressDetail"] = r.progressDetail;
    if (!r.startedAtUtc.isEmpty()) o["startedAtUtc"] = r.startedAtUtc;
    if (!r.finishedAtUtc.isEmpty()) o["finishedAtUtc"] = r.finishedAtUtc;
    o["result"] = r.result;
    o["problem"] = r.problem.isEmpty() ? QJsonValue::Null : QJsonValue(r.problem);
    return o;
}

QString OperationManager::submit(const QString &kind, const QStringList &resources, bool cancelSupported, Task task) {
    Record r;
    r.id = "op-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    r.kind = kind;
    r.resources = resources;
    r.cancelSupported = cancelSupported;
    r.createdAtUtc = r.updatedAtUtc = nowUtc();
    r.task = std::move(task);
    {
        QMutexLocker lock(&mutex_);
        records_.insert(r.id, r);
        order_.append(r.id);
    }
    emit operationChanged(toJson(r));
    tryStartQueued();
    return r.id;
}

bool OperationManager::cancel(const QString &id, QString *error) {
    QJsonObject changed;
    bool queuedCancelled = false;
    {
        QMutexLocker lock(&mutex_);
        auto it = records_.find(id);
        if (it == records_.end()) { if (error) *error = "Operation not found"; return false; }
        if (!it->cancelSupported) { if (error) *error = "Operation does not support cancellation"; return false; }
        if (it->state == "succeeded" || it->state == "failed" || it->state == "cancelled") return true;
        it->cancelRequested = true;
        it->cancelFlag->store(true, std::memory_order_relaxed);
        it->updatedAtUtc = nowUtc();
        if (it->state == "queued") {
            it->state = "cancelled";
            it->finishedAtUtc = it->updatedAtUtc;
            it->progressDetail = {{"message","Cancelled before execution"}};
            queuedCancelled = true;
        }
        changed = toJson(*it);
    }
    emit operationChanged(changed);
    if (queuedCancelled) tryStartQueued();
    return true;
}

QJsonObject OperationManager::operationJson(const QString &id) const {
    QMutexLocker lock(&mutex_);
    auto it = records_.constFind(id);
    return it == records_.cend() ? QJsonObject{} : toJson(*it);
}

QJsonArray OperationManager::operationsJson(bool activeOnly) const {
    QMutexLocker lock(&mutex_);
    QJsonArray out;
    for (auto it = order_.crbegin(); it != order_.crend(); ++it) {
        auto r = records_.constFind(*it); if (r == records_.cend()) continue;
        if (activeOnly && r->state != "queued" && r->state != "running") continue;
        out.append(toJson(*r));
    }
    return out;
}

bool OperationManager::isResourceLocked(const QString &resource, QString *ownerOperationId) const {
    QMutexLocker lock(&mutex_);
    auto it = lockOwners_.constFind(resource);
    if (it == lockOwners_.cend()) return false;
    if (ownerOperationId) *ownerOperationId = *it;
    return true;
}

bool OperationManager::resourcesAvailable(const QStringList &resources, QString *ownerOperationId) const {
    QMutexLocker lock(&mutex_);
    for (const auto &resource : resources) {
        auto it = lockOwners_.constFind(resource);
        if (it != lockOwners_.cend()) { if (ownerOperationId) *ownerOperationId = *it; return false; }
    }
    return true;
}

QJsonObject OperationManager::locksJson() const {
    QMutexLocker lock(&mutex_);
    QJsonObject o;
    for (auto it = lockOwners_.cbegin(); it != lockOwners_.cend(); ++it) o[it.key()] = it.value();
    return o;
}

void OperationManager::tryStartQueued() {
    struct Launch { QString id; Task task; std::shared_ptr<std::atomic_bool> cancel; QJsonObject json; };
    QList<Launch> launches;
    {
        QMutexLocker lock(&mutex_);
        if (shuttingDown_) return;
        for (const auto &id : order_) {
            auto it = records_.find(id);
            if (it == records_.end() || it->state != "queued") continue;
            bool free = true;
            for (const auto &resource : it->resources) if (lockOwners_.contains(resource)) { free = false; break; }
            if (!free) continue;
            for (const auto &resource : it->resources) lockOwners_[resource] = id;
            it->state = "running";
            it->startedAtUtc = it->updatedAtUtc = nowUtc();
            it->phase = "starting";
            launches.append({id,it->task,it->cancelFlag,toJson(*it)});
            it->task = {};
        }
    }
    for (const auto &x : launches) {
        emit operationChanged(x.json);
        pool_.start([this,id=x.id,task=x.task,cancel=x.cancel](){ runOperation(id, task, cancel); });
    }
}

void OperationManager::runOperation(const QString &id, Task task, std::shared_ptr<std::atomic_bool> cancelFlag) {
    OperationContext ctx;
    ctx.cancelFlag_ = cancelFlag;
    ctx.progress_ = [this,id](double p,const QString&phase,const QJsonObject&detail){ updateProgress(id,p,phase,detail); };
    OperationOutcome outcome;
    try {
        if (cancelFlag->load(std::memory_order_relaxed)) outcome.cancelled = true;
        else if (task) outcome = task(ctx);
        else { outcome.problem = {{"code","NO_TASK"},{"message","Operation task is unavailable"}}; }
    } catch (const std::exception &e) {
        outcome.problem = {{"code","OPERATION_EXCEPTION"},{"message",QString::fromUtf8(e.what())}};
    } catch (...) {
        outcome.problem = {{"code","OPERATION_EXCEPTION"},{"message","Unknown operation exception"}};
    }
    if (cancelFlag->load(std::memory_order_relaxed) && !outcome.success) outcome.cancelled = true;
    finish(id, outcome);
}

void OperationManager::updateProgress(const QString &id, double progress, const QString &phase, const QJsonObject &detail) {
    QJsonObject changed;
    {
        QMutexLocker lock(&mutex_);
        auto it = records_.find(id); if (it == records_.end() || it->state != "running") return;
        it->progress = std::clamp(progress,0.0,1.0);
        it->phase = phase;
        it->progressDetail = detail;
        it->updatedAtUtc = nowUtc();
        changed = toJson(*it);
    }
    emit operationChanged(changed);
}

void OperationManager::finish(const QString &id, const OperationOutcome &outcome) {
    QJsonObject changed;
    {
        QMutexLocker lock(&mutex_);
        auto it = records_.find(id); if (it == records_.end()) return;
        if (outcome.cancelled) it->state = "cancelled";
        else if (outcome.success) it->state = "succeeded";
        else it->state = "failed";
        it->result = outcome.result;
        it->problem = outcome.problem;
        if (!outcome.success && !outcome.cancelled && it->problem.isEmpty()) it->problem = {{"code","OPERATION_FAILED"},{"message","Operation failed"}};
        if (outcome.success) it->progress = 1.0;
        it->updatedAtUtc = it->finishedAtUtc = nowUtc();
        for (const auto &resource : it->resources) if (lockOwners_.value(resource) == id) lockOwners_.remove(resource);
        changed = toJson(*it);
    }
    emit operationChanged(changed);
    tryStartQueued();
}

void OperationManager::emitSnapshot(const QString &id) {
    auto o = operationJson(id); if (!o.isEmpty()) emit operationChanged(o);
}

} // namespace oas
