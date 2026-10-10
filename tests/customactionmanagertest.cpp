#include "ui/customactions/customactionmanager.h"

#include <QSettings>
#include <QSignalSpy>
#include <QTest>

class CustomActionManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void seedsOneDefaultNewFileActionOnFirstRun();
    void openWithMatchesCaseInsensitiveExtension();
    void disabledActionsAreExcluded();
    void removingAnActionDropsIt();
};

void CustomActionManagerTest::initTestCase()
{
    // Isolated org/app name so this never touches the real app's saved
    // custom actions in the registry/settings store.
    QCoreApplication::setOrganizationName(QStringLiteral("lvdExplorerTests"));
    QCoreApplication::setApplicationName(QStringLiteral("CustomActionManagerTest"));
}

void CustomActionManagerTest::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
}

void CustomActionManagerTest::cleanup()
{
    // CustomActionManager is a singleton that loads once from QSettings;
    // each test starts from whatever state the previous one left it in,
    // which is fine since every test below only adds/removes its own
    // actions rather than asserting on the full list.
}

void CustomActionManagerTest::seedsOneDefaultNewFileActionOnFirstRun()
{
    // First touch of the singleton in this test binary: nothing saved yet,
    // so it should have seeded exactly the one documented default.
    const QList<CustomAction> newFileActions = CustomActionManager::instance().newFileActions();
    QCOMPARE(newFileActions.size(), 1);
    QCOMPARE(newFileActions.first().kind, CustomAction::Kind::NewFile);
    QVERIFY(!newFileActions.first().newFileName.isEmpty());
    QVERIFY(CustomActionManager::instance().isEnabled());
}

void CustomActionManagerTest::openWithMatchesCaseInsensitiveExtension()
{
    CustomAction action;
    action.kind = CustomAction::Kind::OpenWith;
    action.name = QStringLiteral("Test 7-Zip");
    action.extensions = {QStringLiteral("zip"), QStringLiteral("7z")};
    action.programPath = QStringLiteral("C:/does/not/matter.exe");
    CustomActionManager::instance().addOrUpdate(action);

    const QList<CustomAction> lower = CustomActionManager::instance().openWithActionsFor(QStringLiteral("archive.zip"));
    const QList<CustomAction> upper = CustomActionManager::instance().openWithActionsFor(QStringLiteral("ARCHIVE.ZIP"));
    const QList<CustomAction> noMatch = CustomActionManager::instance().openWithActionsFor(QStringLiteral("notes.txt"));

    QVERIFY(!lower.isEmpty());
    QVERIFY(!upper.isEmpty());
    QCOMPARE(lower.first().name, QStringLiteral("Test 7-Zip"));
    QVERIFY(noMatch.isEmpty());

    CustomActionManager::instance().remove(lower.first().id);
}

void CustomActionManagerTest::disabledActionsAreExcluded()
{
    CustomAction action;
    action.kind = CustomAction::Kind::OpenWith;
    action.name = QStringLiteral("Disabled Opener");
    action.extensions = {QStringLiteral("foo")};
    action.programPath = QStringLiteral("C:/does/not/matter.exe");
    action.enabled = false;
    CustomActionManager::instance().addOrUpdate(action);

    QVERIFY(CustomActionManager::instance().openWithActionsFor(QStringLiteral("x.foo")).isEmpty());

    for (const CustomAction &stored : CustomActionManager::instance().actions()) {
        if (stored.name == QStringLiteral("Disabled Opener"))
            CustomActionManager::instance().remove(stored.id);
    }
}

void CustomActionManagerTest::removingAnActionDropsIt()
{
    CustomAction action;
    action.kind = CustomAction::Kind::NewFile;
    action.name = QStringLiteral("Temp Action");
    action.newFileName = QStringLiteral("temp.md");
    CustomActionManager::instance().addOrUpdate(action);

    QString id;
    for (const CustomAction &stored : CustomActionManager::instance().actions()) {
        if (stored.name == QStringLiteral("Temp Action"))
            id = stored.id;
    }
    QVERIFY(!id.isEmpty());

    QSignalSpy changedSpy(&CustomActionManager::instance(), &CustomActionManager::changed);
    CustomActionManager::instance().remove(id);
    QVERIFY(!changedSpy.isEmpty());

    for (const CustomAction &stored : CustomActionManager::instance().actions())
        QVERIFY(stored.id != id);
}

QTEST_GUILESS_MAIN(CustomActionManagerTest)
#include "customactionmanagertest.moc"
