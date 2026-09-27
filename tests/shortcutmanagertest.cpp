#include "ui/shortcuts/shortcutmanager.h"

#include <QAction>
#include <QSettings>
#include <QTest>

class ShortcutManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void sameIdSharedAcrossMultipleActions();
    void conflictDetection();
    void resetRestoresDefaultAndClearsOverride();
};

void ShortcutManagerTest::initTestCase()
{
    // Isolated org/app name so this never touches the real app's saved
    // shortcuts in the registry/settings store.
    QCoreApplication::setOrganizationName(QStringLiteral("lvdExplorerTests"));
    QCoreApplication::setApplicationName(QStringLiteral("ShortcutManagerTest"));
}

void ShortcutManagerTest::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
}

void ShortcutManagerTest::sameIdSharedAcrossMultipleActions()
{
    // Mirrors the real scenario: the same logical shortcut (e.g. "Rename")
    // exists as one QAction per open pane.
    QAction actionA;
    QAction actionB;
    ShortcutManager::instance().registerAction(&actionA, QStringLiteral("test.shared"), QStringLiteral("Shared"),
                                                QKeySequence(QStringLiteral("Ctrl+1")));
    ShortcutManager::instance().registerAction(&actionB, QStringLiteral("test.shared"), QStringLiteral("Shared"),
                                                QKeySequence(QStringLiteral("Ctrl+1")));

    QCOMPARE(actionA.shortcut(), QKeySequence(QStringLiteral("Ctrl+1")));
    QCOMPARE(actionB.shortcut(), QKeySequence(QStringLiteral("Ctrl+1")));

    ShortcutManager::instance().setShortcut(QStringLiteral("test.shared"), QKeySequence(QStringLiteral("Ctrl+2")));

    QCOMPARE(actionA.shortcut(), QKeySequence(QStringLiteral("Ctrl+2")));
    QCOMPARE(actionB.shortcut(), QKeySequence(QStringLiteral("Ctrl+2")));
}

void ShortcutManagerTest::conflictDetection()
{
    QAction actionX;
    QAction actionY;
    ShortcutManager::instance().registerAction(&actionX, QStringLiteral("test.x"), QStringLiteral("Test X"),
                                                QKeySequence(QStringLiteral("Ctrl+Shift+X")));
    ShortcutManager::instance().registerAction(&actionY, QStringLiteral("test.y"), QStringLiteral("Test Y"),
                                                QKeySequence(QStringLiteral("Ctrl+Shift+Y")));

    QVERIFY(!ShortcutManager::instance().isInUseByOther(QStringLiteral("test.y"),
                                                         QKeySequence(QStringLiteral("Ctrl+Shift+Z"))));
    QVERIFY(ShortcutManager::instance().isInUseByOther(QStringLiteral("test.y"),
                                                        QKeySequence(QStringLiteral("Ctrl+Shift+X"))));
    // Same id as the sequence's own owner is not "in use by *other*".
    QVERIFY(!ShortcutManager::instance().isInUseByOther(QStringLiteral("test.x"),
                                                         QKeySequence(QStringLiteral("Ctrl+Shift+X"))));
}

void ShortcutManagerTest::resetRestoresDefaultAndClearsOverride()
{
    QAction action;
    const QKeySequence defaultSeq(QStringLiteral("Ctrl+Shift+R"));
    ShortcutManager::instance().registerAction(&action, QStringLiteral("test.reset"), QStringLiteral("Test Reset"),
                                                defaultSeq);

    ShortcutManager::instance().setShortcut(QStringLiteral("test.reset"), QKeySequence(QStringLiteral("Ctrl+Shift+T")));
    QCOMPARE(action.shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+T")));

    ShortcutManager::instance().resetToDefault(QStringLiteral("test.reset"));
    QCOMPARE(action.shortcut(), defaultSeq);
    QCOMPARE(ShortcutManager::instance().shortcutFor(QStringLiteral("test.reset")), defaultSeq);
}

QTEST_MAIN(ShortcutManagerTest)
#include "shortcutmanagertest.moc"
