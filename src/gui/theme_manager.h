#pragma once

#include <QColor>
#include <QString>

class QApplication;

namespace oas {

enum class UiTheme { Normal = 0, NightVision = 1, StrictNight = 2 };

UiTheme loadUiTheme();
void saveUiTheme(UiTheme theme);
QString uiThemeName(UiTheme theme);
void applyUiTheme(QApplication &app, UiTheme theme);
bool isStrictNight(UiTheme theme);
QColor strictNightMappedColor(const QColor &dayColor);

} // namespace oas
