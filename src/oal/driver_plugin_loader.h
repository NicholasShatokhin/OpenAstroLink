#pragma once
#include <QObject>
#include <QJsonArray>
#include <memory>
#include <vector>

namespace oas {
class OalDriverPluginLoader final : public QObject {
    Q_OBJECT
public:
    explicit OalDriverPluginLoader(QObject *parent=nullptr);
    ~OalDriverPluginLoader() override;
    int scan(const QString &directory,QStringList *errors=nullptr);
    QJsonArray devices() const;
    QJsonObject invoke(const QString &driverId,const QString &deviceId,const QString &method,const QJsonObject &request,QString *error=nullptr);
private:
    struct Loaded;
    std::vector<std::unique_ptr<Loaded>> loaded_;
};
}
