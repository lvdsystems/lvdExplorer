#include "core/session/portablesettings.h"
#include "core/session/sessionmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QSettings>
#include <QTest>

class PortableSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void fallsBackToNativeFormatWhenNotPortable();
    void redirectsToLocalIniFileWhenPortable();
};

void PortableSettingsTest::initTestCase()
{
    // Isolated org/app name so the non-portable fallback path never
    // touches the real app's saved settings in the registry/.config.
    QCoreApplication::setOrganizationName(QStringLiteral("lvdExplorerTests"));
    QCoreApplication::setApplicationName(QStringLiteral("PortableSettingsTest"));
}

void PortableSettingsTest::fallsBackToNativeFormatWhenNotPortable()
{
    QVERIFY(!SessionManager::isPortableMode());
    const QSettings settings = portableAwareSettings();
    QCOMPARE(settings.format(), QSettings::NativeFormat);
}

void PortableSettingsTest::redirectsToLocalIniFileWhenPortable()
{
    const QString markerPath = QCoreApplication::applicationDirPath() + QStringLiteral("/lvdExplorer.ini");
    const QString dataDir = QCoreApplication::applicationDirPath() + QStringLiteral("/lvdExplorer-data");

    // The marker lives next to this test binary -- the same directory
    // every other test executable built alongside it runs from, and each
    // of those also calls SessionManager::isPortableMode(). Guarantees
    // cleanup even if a QVERIFY/QCOMPARE below fails and returns early,
    // so a broken assertion here can't leave portable mode "on" for every
    // other test that happens to run afterward.
    auto cleanup = qScopeGuard([&] {
        QFile::remove(markerPath);
        QDir(dataDir).removeRecursively();
    });

    QFile::remove(markerPath);
    QDir(dataDir).removeRecursively();

    QFile marker(markerPath);
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();

    QVERIFY(SessionManager::isPortableMode());
    QCOMPARE(SessionManager::dataDirectory(), dataDir);

    {
        QSettings settings = portableAwareSettings();
        QCOMPARE(settings.format(), QSettings::IniFormat);
        QCOMPARE(settings.fileName(), dataDir + QStringLiteral("/settings.ini"));
        settings.setValue(QStringLiteral("probe"), QStringLiteral("value"));
    }

    // Round-trips through a fresh instance -- proves it's actually
    // persisted to disk at the expected path, not just held in memory.
    QSettings reopened = portableAwareSettings();
    QCOMPARE(reopened.value(QStringLiteral("probe")).toString(), QStringLiteral("value"));
}

QTEST_GUILESS_MAIN(PortableSettingsTest)
#include "portablesettingstest.moc"
