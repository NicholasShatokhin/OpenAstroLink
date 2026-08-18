#include "backends/gemini_eaf_focuser.h"
#include "backends/alpaca_devices.h"
#ifdef OAS_HAVE_INDI
#include "backends/indi_devices.h"
#endif
#include <QUrl>

namespace oas {
namespace {
constexpr auto kAlpacaPrefix = "alpaca:";
constexpr auto kIndiPrefix = "indi:";
}

ConnectionState GeminiEafFocuser::connectionState() const {
    return transport_ ? transport_->connectionState() : state_;
}

bool GeminiEafFocuser::ensureTransport(QString *error) {
    if (transport_) return true;

    const QString ep = endpoint_.trimmed();
    if (ep.isEmpty()) {
        if (error) {
            *error = "GeminiAstro EAF endpoint is required. Use "
                     "alpaca:http://host:port/api/v1/focuser/0 or "
                     "indi:host:7624/Exact Device Name.";
        }
        return false;
    }

    if (ep.startsWith(kAlpacaPrefix, Qt::CaseInsensitive)) {
        const QString url = ep.mid(QString(kAlpacaPrefix).size()).trimmed();
        if (url.isEmpty()) {
            if (error) *error = "GeminiAstro EAF Alpaca endpoint is empty.";
            return false;
        }
        transport_ = std::make_shared<AlpacaFocuser>(QUrl(url));
        return true;
    }

    if (ep.startsWith("http://", Qt::CaseInsensitive) ||
        ep.startsWith("https://", Qt::CaseInsensitive)) {
        transport_ = std::make_shared<AlpacaFocuser>(QUrl(ep));
        return true;
    }

    if (ep.startsWith(kIndiPrefix, Qt::CaseInsensitive)) {
        const QString indiEndpoint = ep.mid(QString(kIndiPrefix).size()).trimmed();
        if (indiEndpoint.isEmpty()) {
            if (error) *error = "GeminiAstro EAF INDI endpoint is empty.";
            return false;
        }
#ifdef OAS_HAVE_INDI
        transport_ = std::make_shared<IndiFocuser>(indiEndpoint);
        return true;
#else
        if (error) {
            *error = "This OpenAstroSuite build has no INDI backend. Reconfigure "
                     "with -DOAS_ENABLE_INDI=ON or use an alpaca:<URL> endpoint.";
        }
        return false;
#endif
    }

#ifdef OAS_HAVE_INDI
    // Preserve the existing INDI endpoint grammar for convenience when INDI
    // support is compiled in.  Explicit prefixes are still recommended.
    transport_ = std::make_shared<IndiFocuser>(ep);
    return true;
#else
    if (error) {
        *error = "Unknown GeminiAstro EAF endpoint. Use alpaca:<URL>. "
                 "INDI endpoints require a build with -DOAS_ENABLE_INDI=ON.";
    }
    return false;
#endif
}

bool GeminiEafFocuser::connectDevice(QString *error) {
    if (!ensureTransport(error)) {
        state_ = ConnectionState::Error;
        return false;
    }
    if (!transport_->connectDevice(error)) {
        state_ = ConnectionState::Error;
        return false;
    }
    state_ = ConnectionState::Connected;
    return true;
}

void GeminiEafFocuser::disconnectDevice() {
    if (transport_) transport_->disconnectDevice();
    state_ = ConnectionState::Disconnected;
}

bool GeminiEafFocuser::status(FocuserStatus &s, QString *error) {
    if (!transport_) {
        if (error) *error = "GeminiAstro EAF is not connected.";
        return false;
    }
    const bool ok = transport_->status(s, error);
    if (ok) s.connection = connectionState();
    return ok;
}

bool GeminiEafFocuser::moveAbsolute(int position, QString *error) {
    if (!transport_) {
        if (error) *error = "GeminiAstro EAF is not connected.";
        return false;
    }
    return transport_->moveAbsolute(position, error);
}

bool GeminiEafFocuser::moveRelative(int delta, QString *error) {
    if (!transport_) {
        if (error) *error = "GeminiAstro EAF is not connected.";
        return false;
    }
    return transport_->moveRelative(delta, error);
}

bool GeminiEafFocuser::halt(QString *error) {
    if (!transport_) {
        if (error) *error = "GeminiAstro EAF is not connected.";
        return false;
    }
    return transport_->halt(error);
}

} // namespace oas
