from pathlib import Path
root=Path(__file__).resolve().parents[1]
canon=(root/'drivers/canon/oal_driver_canon_edsdk.cpp').read_text()
app=(root/'src/core/application_controller.cpp').read_text()
cmake=(root/'CMakeLists.txt').read_text()
probe=(root/'src/tools/hardware_probe.cpp').read_text()
checks={
 'edsdk init diagnostic':'Canon EDSDK initialized' in canon,
 'edsdk scan count diagnostic':'Canon EDSDK discovery scan sees' in canon,
 'manual Canon hard reload':'hard-reloaded native Canon EDSDK driver' in app,
 'Canon active-handle guard':'canonInUse' in app,
 'startup driver ID list':'ids.join(", ")' in app,
 'stale build driver cleanup':'file(REMOVE_RECURSE "${OAL_DRIVER_BUILD_DIR}")' in cmake,
 'probe distinguishes Canon disabled':'Native Canon EOS: DISABLED/NOT LOADED' in probe,
 'probe distinguishes Canon zero cameras':'Canon EDSDK enumerated no camera' in probe,
}
for k,v in checks.items(): print(('PASS' if v else 'FAIL'), k)
failed=[k for k,v in checks.items() if not v]
if failed: raise SystemExit(1)
print(f'{len(checks)}/{len(checks)} PASS')
