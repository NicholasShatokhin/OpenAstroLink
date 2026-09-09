#include "gui/theme_manager.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QtGlobal>

namespace oas {

UiTheme loadUiTheme() {
    QSettings settings("OpenAstroLink", "OpenAstroSuiteGui");
    const int value = settings.value("ui/theme", int(UiTheme::Normal)).toInt();
    if(value == int(UiTheme::NightVision)) return UiTheme::NightVision;
    if(value == int(UiTheme::StrictNight)) return UiTheme::StrictNight;
    return UiTheme::Normal;
}

void saveUiTheme(UiTheme theme) {
    QSettings settings("OpenAstroLink", "OpenAstroSuiteGui");
    settings.setValue("ui/theme", int(theme));
}

QString uiThemeName(UiTheme theme) {
    switch(theme) {
    case UiTheme::NightVision: return QStringLiteral("Night Vision");
    case UiTheme::StrictNight: return QStringLiteral("Strict Night Mode");
    case UiTheme::Normal: default: return QStringLiteral("Normal");
    }
}

bool isStrictNight(UiTheme theme) { return theme == UiTheme::StrictNight; }

QColor strictNightMappedColor(const QColor &dayColor) {
    const int alpha = dayColor.alpha();
    const int luminance = qBound(0, int(0.2126 * dayColor.red() + 0.7152 * dayColor.green() + 0.0722 * dayColor.blue()), 255);
    // Keep semantic contrast, but collapse hue to red-only so no green/blue
    // light leaks into the observer's dark-adapted field of view.
    const int red = qBound(28, int(0.88 * luminance + 38.0), 255);
    return QColor(red, 0, 0, alpha);
}

void applyUiTheme(QApplication &app, UiTheme theme) {
    if(theme == UiTheme::Normal) {
        app.setStyleSheet(QString());
        if(app.style()) app.setPalette(app.style()->standardPalette());
        return;
    }

    QPalette p;
    p.setColor(QPalette::Window, QColor(7, 0, 0));
    p.setColor(QPalette::WindowText, QColor(230, 54, 54));
    p.setColor(QPalette::Base, QColor(3, 0, 0));
    p.setColor(QPalette::AlternateBase, QColor(16, 0, 0));
    p.setColor(QPalette::ToolTipBase, QColor(18, 0, 0));
    p.setColor(QPalette::ToolTipText, QColor(255, 92, 92));
    p.setColor(QPalette::Text, QColor(230, 54, 54));
    p.setColor(QPalette::Button, QColor(19, 0, 0));
    p.setColor(QPalette::ButtonText, QColor(235, 62, 62));
    p.setColor(QPalette::BrightText, QColor(255, 118, 118));
    p.setColor(QPalette::Link, QColor(255, 74, 74));
    p.setColor(QPalette::Highlight, QColor(82, 0, 0));
    p.setColor(QPalette::HighlightedText, QColor(255, 150, 150));
    p.setColor(QPalette::PlaceholderText, QColor(116, 36, 36));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(86, 26, 26));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(86, 26, 26));
    app.setPalette(p);

    const QString strictExtra = theme == UiTheme::StrictNight
        ? QStringLiteral("QProgressBar::chunk { background:#980000; } QHeaderView::section { color:#ff4646; }")
        : QStringLiteral("QProgressBar::chunk { background:#b02424; }");
    app.setStyleSheet(QStringLiteral(R"QSS(
        QWidget { background-color:#070000; color:#e63636; selection-background-color:#520000; selection-color:#ff9696; }
        QMainWindow, QDialog, QMessageBox { background-color:#070000; }
        QMenuBar, QMenu, QToolTip { background:#100000; color:#f04444; border:1px solid #4d0000; }
        QMenuBar::item:selected, QMenu::item:selected { background:#3c0000; color:#ff7777; }
        QGroupBox { border:1px solid #4a1010; margin-top:0.8em; padding-top:0.35em; }
        QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; color:#ef4545; }
        QPushButton, QComboBox, QLineEdit, QSpinBox, QDoubleSpinBox, QDateTimeEdit, QTextEdit, QListWidget, QTabWidget::pane {
            background:#100000; color:#e94242; border:1px solid #541414; }
        QPushButton:hover, QComboBox:hover, QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateTimeEdit:focus { border-color:#a52b2b; }
        QPushButton:pressed, QTabBar::tab:selected { background:#3a0000; color:#ff7272; }
        QPushButton:disabled, QComboBox:disabled, QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { color:#6c2020; border-color:#321010; }
        QTabBar::tab { background:#100000; color:#c93636; border:1px solid #441010; padding:5px 9px; }
        QCheckBox::indicator, QRadioButton::indicator { width:13px; height:13px; }
        QCheckBox::indicator:unchecked, QRadioButton::indicator:unchecked { border:1px solid #7a2323; background:#050000; }
        QCheckBox::indicator:checked, QRadioButton::indicator:checked { border:1px solid #c33a3a; background:#8e0000; }
        QScrollBar:vertical, QScrollBar:horizontal { background:#080000; border:0; }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background:#5b1212; min-width:16px; min-height:16px; }
        QScrollBar::add-line, QScrollBar::sub-line { background:#100000; }
        QProgressBar { background:#080000; color:#ef4545; border:1px solid #4a1010; text-align:center; }
        QStatusBar { background:#090000; color:#c93434; }
        QLabel#oasPreviewSurface, QGraphicsView#oasStarMap, QLabel#oasHistogramSurface { background:#020000; color:#9e2e2e; border:1px solid #3c0b0b; }
    )QSS") + strictExtra);
}

} // namespace oas
