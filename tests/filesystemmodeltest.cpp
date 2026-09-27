#include "core/fsmodel/filesystemmodel.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class FileSystemModelTest : public QObject
{
    Q_OBJECT

private slots:
    void rowsAreDragAndDropEnabled();
    void autoRefreshesWhenFileAddedExternally();
    void autoRefreshesWhenFileModifiedExternally();
};

void FileSystemModelTest::rowsAreDragAndDropEnabled()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile file(dir.filePath(QStringLiteral("a.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    FileSystemModel model;
    QSignalSpy finishedSpy(&model, &FileSystemModel::scanFinished);
    model.setRootPath(dir.path());
    QVERIFY(finishedSpy.wait(5000));

    QCOMPARE(model.rowCount(), 1);
    const QModelIndex index = model.index(0, FileSystemModel::NameColumn);
    const Qt::ItemFlags flags = model.flags(index);

    // Regression test: QAbstractItemView's shouldStartDrag() only starts a
    // drag when the pressed index has ItemIsDragEnabled. Without it, Qt
    // silently falls back to its other default behavior for
    // ExtendedSelection -- extending the selection under the cursor --
    // which is exactly the "drag multiselects instead" bug this covers.
    QVERIFY(flags & Qt::ItemIsDragEnabled);
    QVERIFY(flags & Qt::ItemIsDropEnabled);
    QVERIFY(flags & Qt::ItemIsSelectable);
    QVERIFY(flags & Qt::ItemIsEnabled);
}

void FileSystemModelTest::autoRefreshesWhenFileAddedExternally()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile file(dir.filePath(QStringLiteral("a.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    FileSystemModel model;
    QSignalSpy finishedSpy(&model, &FileSystemModel::scanFinished);
    model.setRootPath(dir.path());
    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(model.rowCount(), 1);

    // Simulates "another tool" touching the folder -- nothing in
    // lvdExplorer itself calls refresh() here. Regression test for the
    // reported bug: without QFileSystemWatcher wired up, this second
    // scanFinished would never arrive.
    QFile newFile(dir.filePath(QStringLiteral("b.txt")));
    QVERIFY(newFile.open(QIODevice::WriteOnly));
    newFile.close();

    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(model.rowCount(), 2);
}

void FileSystemModelTest::autoRefreshesWhenFileModifiedExternally()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("a.txt"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArrayLiteral("short"));
    file.close();

    FileSystemModel model;
    QSignalSpy finishedSpy(&model, &FileSystemModel::scanFinished);
    model.setRootPath(dir.path());
    QVERIFY(finishedSpy.wait(5000));

    const QModelIndex index = model.index(0, FileSystemModel::NameColumn);
    QCOMPARE(model.sizeOf(index), qint64(5));

    // Same file, no rename -- only its content/size changes in place.
    // Directory-level watching alone doesn't reliably catch this on every
    // platform, which is why the model also watches each listed file
    // directly (see FileSystemModel::syncWatchedEntries()).
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArrayLiteral("a much longer replacement"));
    file.close();

    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.sizeOf(model.index(0, FileSystemModel::NameColumn)), qint64(25));
}

QTEST_MAIN(FileSystemModelTest)
#include "filesystemmodeltest.moc"
