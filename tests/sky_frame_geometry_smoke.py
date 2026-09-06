#!/usr/bin/env python3
"""Numerical sanity checks for the tangent-plane footprint convention used by Sky Map."""
import math


def offset(ra_deg, dec_deg, east_deg, north_deg):
    D=math.pi/180.0; R=180.0/math.pi
    ra=ra_deg*D; dec=dec_deg*D
    c=(math.cos(dec)*math.cos(ra),math.cos(dec)*math.sin(ra),math.sin(dec))
    e=(-math.sin(ra),math.cos(ra),0.0)
    n=(-math.sin(dec)*math.cos(ra),-math.sin(dec)*math.sin(ra),math.cos(dec))
    v=[c[i]+math.tan(east_deg*D)*e[i]+math.tan(north_deg*D)*n[i] for i in range(3)]
    norm=math.sqrt(sum(x*x for x in v)); v=[x/norm for x in v]
    ra2=math.atan2(v[1],v[0])*R
    if ra2<0: ra2+=360
    return ra2, math.asin(max(-1,min(1,v[2])))*R

# At the equator a +1 degree east tangent offset should be almost +1 degree RA.
ra,dec=offset(100.0,0.0,1.0,0.0)
assert abs(ra-101.0)<1e-6 and abs(dec)<1e-6
# A +1 degree north offset should increase declination.
ra,dec=offset(100.0,0.0,0.0,1.0)
assert abs(ra-100.0)<1e-6 and abs(dec-1.0)<1e-6
# PA=90 means sensor +X points north: its half-width offset is north, not east.
pa=90*math.pi/180; sx=1.0; sy=0.0
east=sx*math.cos(pa)-sy*math.sin(pa); north=sx*math.sin(pa)+sy*math.cos(pa)
assert abs(east)<1e-12 and abs(north-1.0)<1e-12
# 3x2 mosaic with 15% overlap: envelope formula.
w,h=2.0,1.0; frac=.85
assert abs((w+2*w*frac)-5.4)<1e-12
assert abs((h+1*h*frac)-1.85)<1e-12
print('Sky frame geometry smoke: PASS')
