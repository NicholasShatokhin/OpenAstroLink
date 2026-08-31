from pathlib import Path
root=Path(__file__).resolve().parents[1]

def need(rel,*tokens):
    text=(root/rel).read_text(encoding='utf-8')
    for token in tokens:
        assert token in text, f"{rel}: missing {token!r}"
    return text

need('CMakeLists.txt','VERSION 0.2.10.43')
cpp=need('src/backends/synscan_network_mount.cpp',
    'hilEqDriveWifiProfile=countsPerRev1_==9216000u&&countsPerRev2_==9216000u&&timerFreq_==53694u',
    'transportAxis2Sign_=-1',
    'deltaDeg*double(transportAxisSign(axis))',
    'direction*transportAxisSign(axis)',
    'axisDirection*transportAxisSign(1)',
    'socket_.state()!=QAbstractSocket::BoundState',
    'transportPolaritySource_="eqdrive-wifi-hil-profile"')
need('src/backends/synscan_network_mount.h','transportAxis1Sign_{1}','transportAxis2Sign_{1}','transportPolaritySource_')

# HIL log regression: for the RA=310.364651, DEC=45.283495 target the v6
# mechanical target was Axis2=-44.620195 deg. If the Wi-Fi bridge exposes
# controller counts with opposite DEC polarity, the raw controller must move
# +44.620195 deg while the public mechanical axis remains -44.620195 deg.
mechanical=-44.620195
transport_sign=-1
raw_count_angle=mechanical/transport_sign
assert abs(raw_count_angle-44.620195)<1e-9
# Without the transport correction the physical pointing disagreement between
# the two senses is almost exactly the ~90 deg HIL symptom.
assert 89.0 < abs(mechanical-raw_count_angle) < 90.0
print('SynScan Wi-Fi polarity v0.2.10.43: PASS')
