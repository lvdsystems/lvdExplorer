#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include "Version.h"
#include "mainwindow.h"
#include "ui/resources/iconfactory.h"
#include "ui/theme/thememanager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("lvdExplorer"));
    QApplication::setOrganizationName(QStringLiteral("lvdExplorer"));
    // Generated from project(lvdExplorer VERSION ...) in the root
    // CMakeLists.txt -- see app/Version.h.in.
    QApplication::setApplicationVersion(QStringLiteral(LVDEXPLORER_VERSION_STRING));
    QApplication::setWindowIcon(IconFactory::appIcon());

    ThemeManager::apply(ThemeManager::loadSaved());

    // Qt's own standard strings (dialog buttons like "Close", "Cancel",
    // message box text, etc.) live in Qt's *own* translation catalogs, not
    // ours -- install those first so app-specific strings loaded below
    // don't get overridden by them. Tries "qtbase_xx" (how the raw Qt SDK
    // ships them, found during local dev) then "qt_xx" (the consolidated
    // per-language catalog windeployqt generates alongside a qt.conf for
    // a *deployed* build) against the same resolved TranslationsPath, so
    // either layout works without separate code paths.
    QTranslator qtTranslator;
    const QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (qtTranslator.load(QLocale::system(), QStringLiteral("qtbase"), QStringLiteral("_"), qtTranslationsPath)
        || qtTranslator.load(QLocale::system(), QStringLiteral("qt"), QStringLiteral("_"), qtTranslationsPath))
        QApplication::installTranslator(&qtTranslator);

    // lvdExplorer's own strings: .qm files are embedded at build time
    // under ":/i18n/" (see app/CMakeLists.txt's qt_add_translations()
    // call). Falls back to the untranslated (English) source strings when
    // no match exists for the system locale.
    QTranslator translator;
    if (translator.load(QLocale::system(), QStringLiteral("lvdExplorer"), QStringLiteral("_"),
                         QStringLiteral(":/i18n")))
        QApplication::installTranslator(&translator);

    MainWindow window;
    window.show();

    return QApplication::exec();
}
