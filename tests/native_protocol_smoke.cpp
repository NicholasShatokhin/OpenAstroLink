#include "myfocuserpro2_protocol.h"
#include "synscan_protocol.h"
#include "motor_controller_protocol.h"
#include "astep_protocol.h"

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


    const auto eqst = eqdrive::parseStatus("St 1111 144.567494 352.327494 15.000000 0.000000 1423170002.598\r");
    assert(eqst);
    assert(eqst->gotoActive1 && eqst->gotoActive2);
    assert(eqst->driverEnabled1 && eqst->driverEnabled2);
    assert(near(eqst->axis1Deg, 144.567494));
    const auto eqpos = eqdrive::parsePosition("Pos 23.56 2.345 OK\r");
    assert(eqpos && near(eqpos->first, 23.56) && near(eqpos->second, 2.345));
    const auto eqspeed = eqdrive::parseSpeed("Speed 15.1 0.17 OK\r");
    assert(eqspeed && near(eqspeed->first, 15.1) && near(eqspeed->second, 0.17));
    assert(eqdrive::responseOk("Slew OK\r", "Slew"));
    assert(eqdrive::line("St") == "St\r");
    const auto eqdirs = eqdrive::parseAxisDirections("Cg - 1.0 450.000000 800.000000 200 0.50 0 0 0 15 0 0 + 1.0 162.500000 800.000000 200 0.50 0 0 0 15 0 0\r");
    assert(eqdirs.first == -1 && eqdirs.second == 1);

    assert(skywatcher_mc::instantStop(1) == ":L1\r");
    assert(skywatcher_mc::initDone(2) == ":F2\r");
    assert(skywatcher_mc::encodeU24(0x123456u) == "563412");
    assert(skywatcher_mc::decodeU24("563412").value() == 0x123456u);
    assert(skywatcher_mc::decodePosition("=000080\r").value() == 0);
    assert(skywatcher_mc::getVersion(1) == ":e1\r");
    assert(skywatcher_mc::getCountsPerRev(2) == ":a2\r");
    assert(skywatcher_mc::setMotionMode(1, true, false, false) == ":G121\r"); // reverse
    assert(skywatcher_mc::setMotionMode(1, true, false, true) == ":G120\r");  // forward
    const auto mcStopped = skywatcher_mc::parseStatus("=001\r");
    assert(mcStopped.valid && !mcStopped.running && mcStopped.initialized && mcStopped.gotoMode);
    const auto mcGotoRunning = skywatcher_mc::parseStatus("=011\r");
    assert(mcGotoRunning.valid && mcGotoRunning.running && mcGotoRunning.gotoMode && mcGotoRunning.initialized);
    const auto mcSlewRunning = skywatcher_mc::parseStatus("=111\r");
    assert(mcSlewRunning.valid && mcSlewRunning.running && !mcSlewRunning.gotoMode && mcSlewRunning.initialized);

    const auto gotoPos = skywatcher_mc::makeGotoPlan(44.620195, 9216000.0);
    const auto gotoNeg = skywatcher_mc::makeGotoPlan(-44.620195, 9216000.0);
    assert(gotoPos && gotoNeg);
    assert(gotoPos->counts == gotoNeg->counts);
    assert(gotoPos->brakeCounts == gotoNeg->brakeCounts);
    assert(gotoPos->forward && !gotoNeg->forward);
    assert(gotoNeg->counts == 1142277u);

    std::cout << "Native telescope protocol smoke tests passed.\n";
    return 0;
}
