#include "myfocuserpro2_protocol.h"
#include "synscan_protocol.h"
#include "motor_controller_protocol.h"

#include <cassert>
#include <cmath>
#include <iostream>

static bool near(double a, double b, double eps = 1e-4) {
    return std::abs(a - b) <= eps;
}

int main() {
    using namespace oal;

    assert(gemini::probeCommand() == ":02#");
    assert(gemini::positionCommand() == ":00#");
    assert(gemini::movingCommand() == ":01#");
    assert(gemini::moveAbsoluteCommand(1100) == ":051100#");
    assert(gemini::isProbeResponse("EOK#"));
    assert(gemini::parsePrefixedInt("P3080#", 'P').value() == 3080);
    assert(near(gemini::parsePrefixedDouble("Z21.500#", 'Z').value(), 21.5));
    assert(gemini::parseMoving("I00#").value() == false);
    assert(gemini::parseMoving("I01#").value() == true);

    assert(synscan::encodePreciseAngle(0.0) == "00000000");
    assert(synscan::encodePreciseAngle(180.0) == "80000000");
    const auto negFive = synscan::decodePreciseAngle(synscan::encodePreciseAngle(-5.0), true);
    assert(negFive && near(*negFive, -5.0, 3e-5));

    double ra = 0.0, dec = 0.0;
    assert(synscan::parsePreciseRaDec("34AB0500,12CE0500#", ra, dec));
    assert(ra >= 0.0 && ra < 360.0);
    assert(dec >= -90.0 && dec <= 90.0);
    assert(synscan::gotoRaDec(180.0, -5.0).rfind("r80000000,", 0) == 0);
    assert(synscan::syncRaDec(180.0, -5.0).rfind("s80000000,", 0) == 0);
    assert(synscan::echoProbe('O') == "KO");

    assert(skywatcher_mc::instantStop(1) == ":L1\r");
    assert(skywatcher_mc::initDone(2) == ":F2\r");

    std::cout << "Native telescope protocol smoke tests passed.\n";
    return 0;
}
