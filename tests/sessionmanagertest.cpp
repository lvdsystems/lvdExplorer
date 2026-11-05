#include "core/session/sessionmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTest>

class SessionManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void savesAndLoadsNamedSessions();
    void recentSessionsAreMostRecentFirstAndDeduped();
    void recentSessionsAreCappedAtTen();
    void removingASessionForgetsItEverywhere();
    void lastSessionNameRoundTrips();

private:
    QString m_markerPath;
    QString m_dataDir;
};

void SessionManagerTest::initTestCase()
{
    // Isolated org/app name so nothing here touches the real app's saved
    // settings in the registry/.config -- same convention as
    // PortableSettingsTest.
    QCoreApplication::setOrganizationName(QStringLiteral("lvdExplorerTests"));
    QCoreApplication::setApplicationName(QStringLiteral("SessionManagerTest"));
    m_markerPath = QCoreApplication::applicationDirPath() + QStringLiteral("/lvdExplorer.ini");
    m_dataDir = QCoreApplication::applicationDirPath() + QStringLiteral("/lvdExplorer-data");
}

void SessionManagerTest::init()
{
    // Portable mode redirects both the session .json files and
    // portableAwareSettings()'s .ini (which recent/last-session-name live
    // in) under m_dataDir, next to this test binary, instead of the real
    // per-user session storage -- fresh for every test function.
    QFile::remove(m_markerPath);
    QDir(m_dataDir).removeRecursively();
    QFile marker(m_markerPath);
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();
    QVERIFY(SessionManager::isPortableMode());
}

void SessionManagerTest::cleanup()
{
    QFile::remove(m_markerPath);
    QDir(m_dataDir).removeRecursively();
}

void SessionManagerTest::savesAndLoadsNamedSessions()
{
    SessionData data;
    data.paneLayout = QStringLiteral("two-commander");
    QVERIFY(SessionManager::save(QStringLiteral("work"), data));
    QVERIFY(SessionManager::exists(QStringLiteral("work")));

    SessionData loaded;
    QVERIFY(SessionManager::load(QStringLiteral("work"), loaded));
    QCOMPARE(loaded.paneLayout, QStringLiteral("two-commander"));

    QCOMPARE(SessionManager::listNamedSessions(), QStringList{QStringLiteral("work")});
}

void SessionManagerTest::recentSessionsAreMostRecentFirstAndDeduped()
{
    SessionManager::noteSessionUsed(QStringLiteral("a"));
    SessionManager::noteSessionUsed(QStringLiteral("b"));
    SessionManager::noteSessionUsed(QStringLiteral("c"));
    QCOMPARE(SessionManager::recentSessionNames(),
             (QStringList{QStringLiteral("c"), QStringLiteral("b"), QStringLiteral("a")}));

    // Re-using an existing entry moves it back to the front instead of
    // duplicating it.
    SessionManager::noteSessionUsed(QStringLiteral("a"));
    QCOMPARE(SessionManager::recentSessionNames(),
             (QStringList{QStringLiteral("a"), QStringLiteral("c"), QStringLiteral("b")}));
}

void SessionManagerTest::recentSessionsAreCappedAtTen()
{
    for (int i = 0; i < 12; ++i)
        SessionManager::noteSessionUsed(QStringLiteral("session-%1").arg(i));

    const QStringList recents = SessionManager::recentSessionNames();
    QCOMPARE(recents.size(), 10);
    QCOMPARE(recents.first(), QStringLiteral("session-11")); // most recently used
    QVERIFY(!recents.contains(QStringLiteral("session-0")));
    QVERIFY(!recents.contains(QStringLiteral("session-1")));
}

void SessionManagerTest::removingASessionForgetsItEverywhere()
{
    SessionManager::save(QStringLiteral("temp"), SessionData());
    SessionManager::noteSessionUsed(QStringLiteral("temp"));
    SessionManager::setLastSessionName(QStringLiteral("temp"));

    QVERIFY(SessionManager::remove(QStringLiteral("temp")));
    QVERIFY(!SessionManager::exists(QStringLiteral("temp")));
    QVERIFY(!SessionManager::recentSessionNames().contains(QStringLiteral("temp")));
    QVERIFY(SessionManager::lastSessionName().isEmpty());
}

void SessionManagerTest::lastSessionNameRoundTrips()
{
    QVERIFY(SessionManager::lastSessionName().isEmpty());
    SessionManager::setLastSessionName(QStringLiteral("work"));
    QCOMPARE(SessionManager::lastSessionName(), QStringLiteral("work"));
    SessionManager::setLastSessionName(QString());
    QVERIFY(SessionManager::lastSessionName().isEmpty());
}

QTEST_GUILESS_MAIN(SessionManagerTest)
#include "sessionmanagertest.moc"
