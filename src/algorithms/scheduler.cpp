#include "algorithms/scheduler.h"

#include <QDateTime>
#include <QUuid>
#include <utility>
#include <algorithm>

namespace oas {

namespace {
int mosaicTileCount(const ObservationBlock &b) {
    return b.mode == ObservationMode::MosaicFits ? std::max(1,b.mosaic.columns)*std::max(1,b.mosaic.rows) : 0;
}
int mosaicFramesPerTile(const ObservationBlock &b) {
    return b.mode == ObservationMode::MosaicFits ? std::max(1,b.mosaic.tile.frameCount) : 1;
}
}

void Scheduler::setPlan(ObservationPlan plan) {
    plan_ = std::move(plan);
    if (plan_.id.trimmed().isEmpty())
        plan_.id = "plan-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    // v0.2.10.48 compatibility: the old plan-wide start time now belongs to
    // the first block. All new plans should use per-block startAtUtc.
    if(plan_.startAtUtc.isValid()&&!plan_.blocks.empty()&&!plan_.blocks.front().startAtUtc.isValid())
        plan_.blocks.front().startAtUtc=plan_.startAtUtc;
    for (int i = 0; i < int(plan_.blocks.size()); ++i) {
        auto &b = plan_.blocks[size_t(i)];
        if (b.id.trimmed().isEmpty()) b.id = QString("block-%1").arg(i + 1);
        if (b.name.trimmed().isEmpty()) b.name = b.id;
        if (b.mode == ObservationMode::DsoFits) {
            b.dso.frameCount = std::max(1, b.dso.frameCount);
            b.dso.exposure.exposureSec = std::max(0.0001, b.dso.exposure.exposureSec);
            b.dso.exposure.binX = std::max(1, b.dso.exposure.binX);
            b.dso.exposure.binY = std::max(1, b.dso.exposure.binY);
            b.dso.exposure.saveRaw = true;
        } else if(b.mode == ObservationMode::PlanetarySer) {
            b.planetary.serRuns = std::max(1,b.planetary.serRuns);
            b.planetary.durationSec = std::clamp(b.planetary.durationSec,0.5,86400.0);
            b.planetary.pauseSec = std::clamp(b.planetary.pauseSec,0.0,3600.0);
            b.planetary.stream.exposureSec = std::clamp(b.planetary.stream.exposureSec,0.00005,10.0);
            b.planetary.stream.binX = std::max(1,b.planetary.stream.binX);
            b.planetary.stream.binY = std::max(1,b.planetary.stream.binY);
            b.planetary.stream.recordSer = true;
            b.planetary.autofocus.request.mode = AutofocusMode::Planet;
            b.planetary.tracking.roiShiftThresholdPx = std::max(4,b.planetary.tracking.roiShiftThresholdPx);
            b.planetary.tracking.mountCorrectionThresholdPx = std::max(b.planetary.tracking.roiShiftThresholdPx+4,b.planetary.tracking.mountCorrectionThresholdPx);
            b.planetary.tracking.lostTargetFrames = std::max(1,b.planetary.tracking.lostTargetFrames);
        } else {
            b.mosaic.columns=std::clamp(b.mosaic.columns,1,100);
            b.mosaic.rows=std::clamp(b.mosaic.rows,1,100);
            b.mosaic.overlapPercent=std::clamp(b.mosaic.overlapPercent,0.0,90.0);
            b.mosaic.tile.frameCount=std::max(1,b.mosaic.tile.frameCount);
            b.mosaic.tile.exposure.exposureSec=std::max(0.0001,b.mosaic.tile.exposure.exposureSec);
            b.mosaic.tile.exposure.binX=std::max(1,b.mosaic.tile.exposure.binX);
            b.mosaic.tile.exposure.binY=std::max(1,b.mosaic.tile.exposure.binY);
            b.mosaic.tile.exposure.saveRaw=true;
        }
    }

    status_ = {};
    status_.name = plan_.name;
    status_.blockCount = int(plan_.blocks.size());
    status_.targetCount = status_.blockCount;
    status_.state = plan_.blocks.empty() ? "idle" : "ready";
    status_.currentStep = status_.state;
    refreshCurrentBlockFields();
    publish();
}

void Scheduler::setLegacyPlan(QString name, const std::vector<SessionTarget> &targets) {
    ObservationPlan plan;
    plan.name = std::move(name);
    plan.blocks.reserve(targets.size());
    for (int i = 0; i < int(targets.size()); ++i)
        plan.blocks.push_back(observationBlockFromLegacy(targets[size_t(i)], i));
    setPlan(std::move(plan));
}

std::optional<ObservationBlock> Scheduler::currentBlock() const {
    if (status_.blockIndex < 0 || status_.blockIndex >= int(plan_.blocks.size())) return {};
    return plan_.blocks[size_t(status_.blockIndex)];
}

void Scheduler::prepareCurrentBlockStart(bool firstBlock) {
    status_.scheduledStartUtc={};
    const auto block=currentBlock();
    if(!block)return;
    const QDateTime now=QDateTime::currentDateTimeUtc();
    const bool future=block->startAtUtc.isValid()&&block->startAtUtc.toUTC()>now.addMSecs(250);
    if(future){
        status_.scheduledStartUtc=block->startAtUtc.toUTC();
        status_.state="scheduled";
        status_.currentStep="waiting-block-start";
    }else{
        status_.state="running";
        // The first due block additionally owns interactive-camera cleanup.
        status_.currentStep=firstBlock?"waiting-camera":"prepare-block";
    }
}

bool Scheduler::start(int blockIndex) {
    if (plan_.blocks.empty() || status_.active) return false;
    blockIndex=std::clamp(blockIndex,0,int(plan_.blocks.size())-1);
    status_.id = "session-" + QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmsszzz");
    status_.active = true;
    status_.blockIndex = blockIndex;
    status_.targetIndex = blockIndex;
    status_.completedFrames = 0;
    status_.currentBlockCompletedFrames = 0;
    status_.currentOperationId.clear();
    status_.lastError.clear();
    refreshCurrentBlockFields();
    prepareCurrentBlockStart(true);
    publish();
    return true;
}

bool Scheduler::beginScheduled() {
    if (!status_.active || status_.state != "scheduled") return false;
    status_.scheduledStartUtc={};
    status_.state = "running";
    status_.currentStep = "waiting-camera";
    publish();
    return true;
}

void Scheduler::stop(const QString &reason) {
    status_.active = false;
    status_.state = "stopped";
    status_.currentStep = "stopped";
    status_.currentOperationId.clear();
    status_.scheduledStartUtc = {};
    if (!reason.isEmpty()) status_.lastError = reason;
    publish();
}

void Scheduler::fail(const QString &message) {
    status_.active = false;
    status_.state = "failed";
    status_.currentStep = "failed";
    status_.currentOperationId.clear();
    status_.scheduledStartUtc = {};
    status_.lastError = message;
    publish();
}

void Scheduler::setStep(const QString &step, const QString &operationId) {
    status_.currentStep = step;
    status_.currentOperationId = operationId;
    publish();
}

void Scheduler::clearOperation() {
    if (status_.currentOperationId.isEmpty()) return;
    status_.currentOperationId.clear();
    publish();
}

void Scheduler::markFrameCompleted() {
    ++status_.completedFrames;
    ++status_.currentBlockCompletedFrames;
    refreshCurrentBlockFields();
    publish();
}

void Scheduler::advanceBlock() {
    if (!status_.active) return;
    if (status_.blockIndex + 1 < status_.blockCount) {
        ++status_.blockIndex;
        status_.targetIndex = status_.blockIndex;
        status_.currentBlockCompletedFrames = 0;
        status_.currentOperationId.clear();
        refreshCurrentBlockFields();
        prepareCurrentBlockStart(false);
    } else {
        status_.active = false;
        status_.state = "completed";
        status_.currentStep = "completed";
        status_.currentOperationId.clear();
        status_.scheduledStartUtc={};
        refreshCurrentBlockFields();
    }
    publish();
}

void Scheduler::refreshCurrentBlockFields() {
    status_.targetIndex = status_.blockIndex;
    status_.targetCount = status_.blockCount;
    status_.currentTileIndex=0;status_.currentTileCount=0;
    if (status_.blockIndex >= 0 && status_.blockIndex < int(plan_.blocks.size())) {
        const auto &b = plan_.blocks[size_t(status_.blockIndex)];
        status_.currentBlockId = b.id;
        status_.currentBlockName = b.name;
        if(b.mode==ObservationMode::MosaicFits){
            status_.currentTileCount=mosaicTileCount(b);
            status_.currentTileIndex=std::min(status_.currentTileCount-1,status_.currentBlockCompletedFrames/mosaicFramesPerTile(b));
        }
    } else {
        status_.currentBlockId.clear();
        status_.currentBlockName.clear();
    }
}

void Scheduler::publish() { emit statusChanged(status_); }

} // namespace oas
