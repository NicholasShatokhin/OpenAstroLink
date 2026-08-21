#pragma once

#include <optional>
#include <string>

namespace oal::gemini {
inline std::string probeCommand() { return ":02#"; }
inline std::string positionCommand() { return ":00#"; }
inline std::string movingCommand() { return ":01#"; }
inline std::string firmwareCommand() { return ":04#"; }
inline std::string temperatureCommand() { return ":06#"; }
inline std::string maxPositionCommand() { return ":08#"; }
inline std::string moveAbsoluteCommand(int position) { return ":05" + std::to_string(position) + "#"; }

inline std::optional<int> parsePrefixedInt(const std::string &s, char prefix) {
    if (s.size() < 3 || s.front() != prefix || s.back() != '#') return std::nullopt;
    try { return std::stoi(s.substr(1, s.size()-2)); } catch (...) { return std::nullopt; }
}
inline std::optional<double> parsePrefixedDouble(const std::string &s, char prefix) {
    if (s.size() < 3 || s.front() != prefix || s.back() != '#') return std::nullopt;
    try { return std::stod(s.substr(1, s.size()-2)); } catch (...) { return std::nullopt; }
}
inline std::optional<bool> parseMoving(const std::string &s) {
    if (s == "I00#" || s == "I0#" || s == "00#" || s == "0#") return false;
    if (s == "I01#" || s == "I1#" || s == "01#" || s == "1#") return true;
    return std::nullopt;
}
inline bool isProbeResponse(const std::string &s) { return s == "EOK#"; }
} // namespace oal::gemini
