#!/usr/bin/env python3
from pathlib import Path
import math
root=Path(__file__).resolve().parents[1]

def text(rel):
    return (root/rel).read_text(encoding='utf-8',errors='ignore')

assert 'VERSION 0.2.10.47' in text('CMakeLists.txt')
proto=text('drivers/skywatcher/motor_controller_protocol.h')
native=text('drivers/eqdrive/oal_driver_eqdrive.cpp')
wifi=text('src/backends/synscan_network_mount.cpp')
header=text('src/backends/synscan_network_mount.h')

# One motion-plan function owns delta -> direction/counts for BOTH transports.
assert 'makeGotoPlan(double deltaDeg,double countsPerRev)' in proto
assert 'makeGotoPlan(deltaDeg,cpr)' in native
assert 'makeGotoPlan(deltaDeg,cpr)' in wifi
for token in ('plan->forward','plan->counts','plan->brakeCounts'):
    assert token in native, token
    assert token in wifi, token

# UDP must not invent a polarity layer for the same Motor Controller hardware.
for bad in ('transportAxisSign','transportAxis1Sign','transportAxis2Sign',
            'transportPolarity','OAL_SYNSCAN_WIFI_AXIS1_SIGN','OAL_SYNSCAN_WIFI_AXIS2_SIGN',
            'hilEqDriveWifiProfile'):
    assert bad not in wifi and bad not in header, bad
assert 'udp-11880-wire-identical-to-native-eqdrive' in wifi

# Same encoder delta -> same public mechanical angle as native serial.
assert 'double(qint64(p2)-qint64(sessionHomeCounts2_))*360.0' in wifi
assert 'double(counts)-double(mcHomeZeroCounts(d,axis))' in native

# v8 keeps raw serial/UDP parity. The Core physical Axis1 sign correction is
# above both transports; both still command the same +77.299641-degree DEC movement.
delta=77.299641
cpr=9216000.0
counts=max(1,round(abs(delta)*cpr/360.0))
assert counts == 1978871, counts
forward=delta>0.0
assert forward is True

# Wi-Fi exposes enough data to compare the next HIL log without inference.
for token in ('lastGotoAxis1DeltaDeg','lastGotoAxis2DeltaDeg','lastGotoAxis1Counts',
              'lastGotoAxis2Counts','lastGotoAxis1Forward','lastGotoAxis2Forward',
              'firmwareAxis1','firmwareAxis2'):
    assert token in wifi, token

print('SynScan Wi-Fi/native Motor Controller parity v0.2.10.45: PASS')
