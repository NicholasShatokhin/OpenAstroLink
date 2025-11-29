#pragma once
#include <QString>
#include <vector>
#include <optional>

struct ScheduledTarget {
    QString name;
    double raDeg;
    double decDeg;
    // можна додати часові вікна
};

class Scheduler {
public:
    void addTarget(const ScheduledTarget &t) {
        targets_.push_back(t);
    }

    const std::vector<ScheduledTarget>& targets() const { return targets_; }

    // простіше: віддати наступну по списку
    std::optional<ScheduledTarget> nextTarget() {
        if (targets_.empty()) return std::nullopt;
        if (idx_ >= targets_.size()) idx_ = 0;
        return targets_[idx_++];
    }

private:
    std::vector<ScheduledTarget> targets_;
    std::size_t idx_ = 0;
};
