#pragma once

#include "core/interfaces.h"
#include <memory>
#include <utility>

namespace oas {

// GeminiAstro EAF compatibility profile.
//
// The current implementation intentionally uses the vendor-supported ASCOM
// Alpaca / INDI transports instead of assuming an undocumented USB serial
// command set.  This keeps the hardware profile explicit while allowing the
// transport to be selected per deployment.
class GeminiEafFocuser final : public IFocuser {
public:
    explicit GeminiEafFocuser(QString endpoint) : endpoint_(std::move(endpoint)) {}

    QString id() const override { return "gemini-eaf"; }
    QString displayName() const override { return "GeminiAstro EAF"; }
    QString backendName() const override { return "gemini-eaf"; }
    ConnectionState connectionState() const override;

    bool connectDevice(QString *error = nullptr) override;
    void disconnectDevice() override;
    bool status(FocuserStatus &status, QString *error = nullptr) override;
    bool moveAbsolute(int position, QString *error = nullptr) override;
    bool moveRelative(int delta, QString *error = nullptr) override;
    bool halt(QString *error = nullptr) override;

private:
    bool ensureTransport(QString *error);

    QString endpoint_;
    std::shared_ptr<IFocuser> transport_;
    ConnectionState state_{ConnectionState::Disconnected};
};

} // namespace oas
