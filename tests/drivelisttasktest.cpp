#include "core/fsmodel/drivelisttask.h"

#include <QDir>
#include <QSignalSpy>
#include <QStorageInfo>
#include <QTest>
#include <QThreadPool>

class DriveListTaskTest : public QObject
{
    Q_OBJECT

private slots:
    void listsAtLeastTheCurrentVolume();
};

void DriveListTaskTest::listsAtLeastTheCurrentVolume()
{
    auto *task = new DriveListTask;
    QSignalSpy finishedSpy(task, &DriveListTask::finished);
    QVector<DriveEntry> drives;
    connect(task, &DriveListTask::finished, this,
            [&drives](const QVector<DriveEntry> &result) { drives = result; });

    // Regression test for the "Drives dropdown hangs on an unreachable
    // network share" bug: this just confirms the task runs to completion
    // and returns a sane list via QThreadPool (the actual async plumbing
    // the fix relies on) -- it can't reproduce an unreachable mapped
    // drive in CI, but proves the enumeration itself is correct and the
    // background-task wiring works.
    QThreadPool::globalInstance()->start(task);
    QVERIFY(finishedSpy.wait(5000));

    // Whatever volume this test binary is actively running from must be
    // ready, so the list should never come back empty on a real machine.
    QVERIFY(!drives.isEmpty());

    const QString currentRoot = QStorageInfo(QDir::currentPath()).rootPath();
    bool foundCurrent = false;
    for (const DriveEntry &drive : drives) {
        QVERIFY(!drive.rootPath.isEmpty());
        QVERIFY(!drive.label.isEmpty());
        if (QStorageInfo(drive.rootPath).rootPath() == currentRoot)
            foundCurrent = true;
    }
    QVERIFY(foundCurrent);
}

QTEST_GUILESS_MAIN(DriveListTaskTest)
#include "drivelisttasktest.moc"
