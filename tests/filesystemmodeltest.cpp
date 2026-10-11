#include "core/fsmodel/filesystemmodel.h"

#include <QDir>
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
    void wildcardFilterMatchesWholeName();
    void folderSizesAreOffByDefaultAndComputeOnDemand();
    void folderSizesAreClearedWhenDisabled();
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

void FileSystemModelTest::wildcardFilterMatchesWholeName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    for (const QString &name : {QStringLiteral("report.txt"), QStringLiteral("report_final.txt"),
                                 QStringLiteral("report.doc"), QStringLiteral("notes.txt")}) {
        QFile file(dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
    }

    FileSystemModel model;
    QSignalSpy finishedSpy(&model, &FileSystemModel::scanFinished);
    model.setRootPath(dir.path());
    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(model.rowCount(), 4);

    model.setFilterMode(FileSystemModel::FilterMode::Wildcard);
    // "*" must match zero-or-more of anything, so this excludes
    // "report.doc" and "notes.txt" but keeps both ".txt" names -- a plain
    // substring filter for "report" would also match "report.doc", which
    // is exactly the distinction a wildcard mode exists to make.
    model.setFilterText(QStringLiteral("report*.txt"));
    QCOMPARE(model.rowCount(), 2);

    // Case-insensitive, matching Explorer's own wildcard behavior.
    model.setFilterText(QStringLiteral("REPORT*.TXT"));
    QCOMPARE(model.rowCount(), 2);

    // "?" means exactly one character -- "report.doc" has the same
    // 3-character extension length as "report.txt" (so "report.???" would
    // match both), but only "report.txt" ends in "xt".
    model.setFilterText(QStringLiteral("report.?xt"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.filePath(model.index(0, FileSystemModel::NameColumn)),
             dir.filePath(QStringLiteral("report.txt")));
}

void FileSystemModelTest::folderSizesAreOffByDefaultAndComputeOnDemand()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir root(dir.path());
    QVERIFY(root.mkpath(QStringLiteral("sub/nested")));

    auto writeFile = [](const QString &path, int byteCount) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(byteCount, 'x'));
    };
    writeFile(root.filePath(QStringLiteral("sub/top.bin")), 100);
    writeFile(root.filePath(QStringLiteral("sub/nested/deep.bin")), 50);

    FileSystemModel model;
    QSignalSpy finishedSpy(&model, &FileSystemModel::scanFinished);
    model.setRootPath(dir.path());
    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(model.rowCount(), 1);

    const QModelIndex sizeIndex = model.index(0, FileSystemModel::SizeColumn);
    QVERIFY(model.isDir(model.index(0, FileSystemModel::NameColumn)));

    // Off by default: no size shown, nothing to sort by.
    QVERIFY(!model.calculateFolderSizes());
    QCOMPARE(model.data(sizeIndex).toString(), QString());
    QCOMPARE(model.sizeOf(model.index(0, FileSystemModel::NameColumn)), qint64(-1));

    model.setCalculateFolderSizes(true);
    QVERIFY(model.calculateFolderSizes());

    // Recursive: both the direct child and the nested one count.
    QVERIFY(QTest::qWaitFor(
        [&] { return model.sizeOf(model.index(0, FileSystemModel::NameColumn)) == 150; }, 5000));
    QCOMPARE(model.data(sizeIndex).toString(), QStringLiteral("150 bytes"));
}

void FileSystemModelTest::folderSizesAreClearedWhenDisabled()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir root(dir.path());
    QVERIFY(root.mkpath(QStringLiteral("sub")));
    QFile file(root.filePath(QStringLiteral("sub/a.bin")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(64, 'x'));
    file.close();

    FileSystemModel model;
    QSignalSpy finishedSpy(&model, &FileSystemModel::scanFinished);
    model.setRootPath(dir.path());
    QVERIFY(finishedSpy.wait(5000));

    model.setCalculateFolderSizes(true);
    const QModelIndex nameIndex = model.index(0, FileSystemModel::NameColumn);
    QVERIFY(QTest::qWaitFor([&] { return model.sizeOf(nameIndex) == 64; }, 5000));

    model.setCalculateFolderSizes(false);
    QCOMPARE(model.sizeOf(nameIndex), qint64(-1));
    QCOMPARE(model.data(model.index(0, FileSystemModel::SizeColumn)).toString(), QString());
}

QTEST_MAIN(FileSystemModelTest)
#include "filesystemmodeltest.moc"
