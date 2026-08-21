#pragma once
#include <string>

namespace oal::skywatcher_mc {
// EXPERIMENTAL foundation: Sky-Watcher publishes a lower-level Motor Controller Command Set in addition
// to the SynScan hand-controller protocol. This codec is intentionally kept
// separate from the RA/DEC mount driver: direct axis control requires a
// calibrated alignment/coordinate model before it is safe to expose as GOTO.
inline std::string command(char opcode, int axis, const std::string &payload={}) {
    return std::string(":") + opcode + char('0' + axis) + payload + "\r";
}
inline std::string instantStop(int axis) { return command('L', axis); }
inline std::string initDone(int axis) { return command('F', axis); }
} // namespace oal::skywatcher_mc
