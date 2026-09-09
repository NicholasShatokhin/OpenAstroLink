from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
checks=0

def need(rel,*tokens):
    global checks
    text=(ROOT/rel).read_text(encoding='utf-8')
    for token in tokens:
        assert token in text, f"{rel}: missing {token!r}"
        checks+=1

need('CMakeLists.txt','VERSION 0.2.10.59','src/gui/theme_manager.cpp','src/gui/theme_manager.h')
need('src/app/main.cpp','applyUiTheme(app,oas::loadUiTheme())')
need('src/gui/theme_manager.h','enum class UiTheme','NightVision','StrictNight')
need('src/gui/theme_manager.cpp','ui/theme','Night Vision','Strict Night Mode','QToolTip','strictNightMappedColor')
need('src/gui/main_window.cpp','Night vision mode','Ctrl+Shift+N','Black→red palette for monochrome/raw preview','ui/redMonoPreview','monochromeLike','blackToRedPreview','displayPreviewImage','Debayer must be off','setNightVisionMode')
need('src/gui/sky_map_widget.cpp','nightVisionMode_','strictNightMappedColor')
need('docs/NIGHT_VISION.md','Camera pixels are not recoloured','Strict Night Mode','presentation-only')
need('docs/MOBILE_QML_GUI.md','Qt Quick/QML','OAL 1.0','second presentation layer')
need('docs/uk/NIGHT_VISION.md','Night Vision','Strict Night Mode','не змінює')
need('docs/uk/MOBILE_QML_GUI.md','Qt Quick/QML','OAL 1.0')
print(f'PASS v0.2.10.59 Night Vision/QML roadmap: {checks} assertions')
