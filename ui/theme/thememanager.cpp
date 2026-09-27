#include "ui/theme/thememanager.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyle>

namespace {

QString g_originalStyleName;
QPalette g_originalPalette;
bool g_capturedOriginal = false;

void captureOriginalOnce()
{
    if (g_capturedOriginal)
        return;
    g_originalStyleName = QApplication::style()->objectName();
    g_originalPalette = QApplication::palette();
    g_capturedOriginal = true;
}

QPalette darkPalette()
{
    QPalette palette;
    const QColor windowColor(0x2B, 0x2B, 0x2B);
    const QColor baseColor(0x22, 0x22, 0x22);
    const QColor textColor(0xE0, 0xE0, 0xE0);
    const QColor disabledText(0x7A, 0x7A, 0x7A);
    const QColor highlight(0x3D, 0x6F, 0xA5);

    palette.setColor(QPalette::Window, windowColor);
    palette.setColor(QPalette::WindowText, textColor);
    palette.setColor(QPalette::Base, baseColor);
    palette.setColor(QPalette::AlternateBase, windowColor);
    palette.setColor(QPalette::ToolTipBase, windowColor);
    palette.setColor(QPalette::ToolTipText, textColor);
    palette.setColor(QPalette::Text, textColor);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Button, windowColor);
    palette.setColor(QPalette::ButtonText, textColor);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, highlight);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    return palette;
}

QString themeSettingName(ThemeManager::Theme theme)
{
    switch (theme) {
    case ThemeManager::Theme::Light:
        return QStringLiteral("light");
    case ThemeManager::Theme::Dark:
        return QStringLiteral("dark");
    case ThemeManager::Theme::System:
    default:
        return QStringLiteral("system");
    }
}

} // namespace

namespace ThemeManager {

void apply(Theme theme)
{
    captureOriginalOnce();

    switch (theme) {
    case Theme::System:
        QApplication::setStyle(g_originalStyleName);
        QApplication::setPalette(g_originalPalette);
        break;
    case Theme::Light:
        QApplication::setStyle(QStringLiteral("Fusion"));
        QApplication::setPalette(QApplication::style()->standardPalette());
        break;
    case Theme::Dark:
        QApplication::setStyle(QStringLiteral("Fusion"));
        QApplication::setPalette(darkPalette());
        break;
    }
}

Theme loadSaved()
{
    const QSettings settings;
    const QString name = settings.value(QStringLiteral("theme"), QStringLiteral("system")).toString();
    if (name == QStringLiteral("light"))
        return Theme::Light;
    if (name == QStringLiteral("dark"))
        return Theme::Dark;
    return Theme::System;
}

void save(Theme theme)
{
    QSettings settings;
    settings.setValue(QStringLiteral("theme"), themeSettingName(theme));
}

} // namespace ThemeManager
