#pragma once

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

namespace oal::synscan {

static constexpr double kTurn = 16777216.0; // 24 significant bits; low byte is zero on wire.

inline double normalize360(double degrees) {
    degrees = std::fmod(degrees, 360.0);
    return degrees < 0.0 ? degrees + 360.0 : degrees;
}

inline std::string encodePreciseAngle(double degrees) {
    const double normalized = normalize360(degrees);
    const std::uint32_t raw =
        std::uint32_t(std::llround(normalized / 360.0 * kTurn)) & 0xFFFFFFu;
    std::ostringstream stream;
    stream << std::uppercase << std::hex << std::setfill('0') << std::setw(6) << raw
           << "00";
    return stream.str();
}

inline std::optional<double> decodePreciseAngle(const std::string &word,
                                                 bool signedAngle = false) {
    if (word.size() != 8) return std::nullopt;
    std::uint32_t raw = 0;
    try {
        raw = std::stoul(word.substr(0, 6), nullptr, 16);
    } catch (...) {
        return std::nullopt;
    }
    double degrees = double(raw) / kTurn * 360.0;
    if (signedAngle && degrees > 180.0) degrees -= 360.0;
    return degrees;
}

inline std::string gotoRaDec(double raDeg, double decDeg) {
    return "r" + encodePreciseAngle(raDeg) + "," + encodePreciseAngle(decDeg);
}

inline std::string syncRaDec(double raDeg, double decDeg) {
    return "s" + encodePreciseAngle(raDeg) + "," + encodePreciseAngle(decDeg);
}

inline bool parsePreciseRaDec(const std::string &response, double &raDeg,
                              double &decDeg) {
    if (response.empty() || response.back() != '#') return false;
    const auto comma = response.find(',');
    if (comma == std::string::npos) return false;
    const auto ra = decodePreciseAngle(response.substr(0, comma), false);
    const auto dec = decodePreciseAngle(
        response.substr(comma + 1, response.size() - comma - 2), true);
    if (!ra || !dec || *dec < -90.0001 || *dec > 90.0001) return false;
    raDeg = *ra;
    decDeg = *dec;
    return true;
}

inline std::string echoProbe(char token = 'O') { return std::string("K") + token; }

} // namespace oal::synscan
