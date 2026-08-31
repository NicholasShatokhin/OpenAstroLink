from pathlib import Path

s = Path("src/gui/main_window.cpp").read_text(encoding="utf-8")
assert "connect(mountClockTimer_,&QTimer::timeout" in s
assert "mountClockTimer_->timeout()" not in s
assert "&QTimer::timeout()" not in s
print("GUI QTimer Qt6/MSVC hotfix: PASS (3 assertions)")
