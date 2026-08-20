#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QThreadPool>
#include <QStringList>
#include <atomic>
#include <functional>
#include <memory>

namespace oas {

struct OperationOutcome {
    bool success{false};
    bool cancelled{false};
    QJsonValue result{QJsonValue::Null};
    QJsonObject problem;
};

class OperationContext {
public:
    bool isCancellationRequested() const;
    void reportProgress(double progress, const QString &phase = {}, const QJsonObject &detail = {}) const;

private:
    friend class OperationManager;
    std::shared_ptr<std::atomic_bool> cancelFlag_;
    std::function<void(double,const QString&,const QJsonObject&)> progress_;
};

class OperationManager final : public QObject {
    Q_OBJECT
public:
    using Task = std::function<OperationOutcome(OperationContext&)>;

    explicit OperationManager(QObject *parent = nullptr);
    ~OperationManager() override;
    void shutdown();

    QString submit(const QString &kind, const QStringList &resources, bool cancelSupported, Task task);
    bool cancel(const QString &id, QString *error = nullptr);
    QJsonObject operationJson(const QString &id) const;
    QJsonArray operationsJson(bool activeOnly = false) const;

    bool isResourceLocked(const QString &resource, QString *ownerOperationId = nullptr) const;
    bool resourcesAvailable(const QStringList &resources, QString *ownerOperationId = nullptr) const;
    QJsonObject locksJson() const;

signals:
    void operationChanged(const QJsonObject &operation);

private:
    struct Record {
        QString id;
        QString kind;
        QString state{"queued"};
        double progress{0.0};
        QString phase;
        QJsonObject progressDetail;
        QString createdAtUtc;
        QString updatedAtUtc;
        QString startedAtUtc;
        QString finishedAtUtc;
        bool cancelSupported{false};
        bool cancelRequested{false};
        QStringList resources;
        QJsonValue result{QJsonValue::Null};
        QJsonObject problem;
        std::shared_ptr<std::atomic_bool> cancelFlag{std::make_shared<std::atomic_bool>(false)};
        Task task;
    };

    static QString nowUtc();
    static QJsonObject toJson(const Record &record);
    void tryStartQueued();
    void runOperation(const QString &id, Task task, std::shared_ptr<std::atomic_bool> cancelFlag);
    void updateProgress(const QString &id, double progress, const QString &phase, const QJsonObject &detail);
    void finish(const QString &id, const OperationOutcome &outcome);
    void emitSnapshot(const QString &id);

    mutable QMutex mutex_;
    QHash<QString,Record> records_;
    QStringList order_;
    QHash<QString,QString> lockOwners_;
    QThreadPool pool_;
    bool shuttingDown_{false};
};

} // namespace oas
