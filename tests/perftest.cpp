#include "core/fsmodel/filesystemmodel.h"
#include "core/search/searchengine.h"

#include <QDir>
#include <QFile>
#include <QItemSelectionModel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

// Replicable performance suite, modeled on the category breakdown published
// by https://github.com/RoyiAvital/RoyiFileManager (Pane Load, Filter Bar,
// Fuzzy Find, Selections, Navigation, Refresh/Selection). Two differences
// from that table, both deliberate rather than oversights:
//   - No QuickView rows: lvdExplorer has no preview pane (yet).
//   - "Filename Search" rather than "Fuzzy Find": lvdExplorer's Finder does
//     substring/regex matching, not literal fuzzy matching, so the row is
//     named for what it actually measures.
// Absolute numbers aren't meant to be compared against that table directly
// -- different app, different machine, different disk. The point of this
// suite is to track lvdExplorer's own numbers release over release, the
// same way that table tracks RoyiFileManager's.
//
// Not registered with add_test()/ctest (see tests/CMakeLists.txt) since a
// benchmark isn't a pass/fail check and each run takes noticeably longer
// than the rest of the suite combined. Build and run it directly:
//   cmake --build build --target perftest --config Release
//   build/Release/perftest.exe -o results.csv,csv   (or no -o for console output)
// Always benchmark a Release build -- Debug's timings don't reflect real
// usage and aren't comparable release over release.
class PerformanceBenchmark : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void paneLoadSmallFolder();
    void paneLoadLargeFolder();
    void filterBarSmallFolder();
    void filterBarLargeFolder();
    void filenameSearchLargeFolder();
    void filenameSearchRecursive();
    void selectionsSmallFolder();
    void selectionsLargeFolder();
    void selectionReadback();
    void navigation();
    void refreshPreservingSelection();

private:
    QTemporaryDir m_root;
    QString m_smallDir;   // 1,000 files, flat
    QString m_secondSmallDir; // a second, distinct 1,000-file folder, for Navigation
    QString m_largeDir;   // 50,000 files, flat
    QString m_recursiveDir; // 50 folders x 100 files, for recursive search
};

namespace {

constexpr int kSmallCount = 1000;
constexpr int kLargeCount = 50000;
constexpr int kRecursiveFolders = 50;
constexpr int kRecursiveFilesPerFolder = 100;

void touch(const QString &path)
{
    QFile file(path);
    [[maybe_unused]] const bool opened = file.open(QIODevice::WriteOnly);
}

void makeFlatFolder(const QString &path, int count)
{
    QDir().mkpath(path);
    const QDir dir(path);
    for (int i = 0; i < count; ++i)
        touch(dir.filePath(QStringLiteral("file_%1.txt").arg(i, 6, 10, QLatin1Char('0'))));
}

void makeNestedTree(const QString &path, int folders, int filesPerFolder)
{
    for (int f = 0; f < folders; ++f) {
        const QString sub = QDir(path).filePath(QStringLiteral("dir_%1").arg(f, 3, 10, QLatin1Char('0')));
        QDir().mkpath(sub);
        const QDir subDir(sub);
        for (int i = 0; i < filesPerFolder; ++i)
            touch(subDir.filePath(QStringLiteral("file_%1.txt").arg(i, 4, 10, QLatin1Char('0'))));
    }
}

// Blocks until the model's scan completes. Only safe to call with a path
// that differs from the model's current root -- FileSystemModel::setRootPath
// is a no-op when it doesn't, and scanFinished would then never arrive.
void waitForScan(FileSystemModel &model, const QString &path)
{
    QSignalSpy spy(&model, &FileSystemModel::scanFinished);
    model.setRootPath(path);
    QVERIFY(spy.wait(30000));
}

} // namespace

void PerformanceBenchmark::initTestCase()
{
    QVERIFY(m_root.isValid());
    m_smallDir = QDir(m_root.path()).filePath(QStringLiteral("small"));
    m_secondSmallDir = QDir(m_root.path()).filePath(QStringLiteral("small2"));
    m_largeDir = QDir(m_root.path()).filePath(QStringLiteral("large"));
    m_recursiveDir = QDir(m_root.path()).filePath(QStringLiteral("recursive"));

    makeFlatFolder(m_smallDir, kSmallCount);
    makeFlatFolder(m_secondSmallDir, kSmallCount);
    makeFlatFolder(m_largeDir, kLargeCount);
    makeNestedTree(m_recursiveDir, kRecursiveFolders, kRecursiveFilesPerFolder);
}

void PerformanceBenchmark::paneLoadSmallFolder()
{
    QBENCHMARK {
        FileSystemModel model;
        waitForScan(model, m_smallDir);
    }
}

void PerformanceBenchmark::paneLoadLargeFolder()
{
    QBENCHMARK {
        FileSystemModel model;
        waitForScan(model, m_largeDir);
    }
}

void PerformanceBenchmark::filterBarSmallFolder()
{
    FileSystemModel model;
    waitForScan(model, m_smallDir);

    // Toggles between two distinct filter strings each iteration --
    // setFilterText() no-ops when called with the text it already has, so
    // repeating the same string would silently stop measuring anything.
    bool toggled = false;
    QBENCHMARK {
        toggled = !toggled;
        model.setFilterText(toggled ? QStringLiteral("file_000") : QString());
    }
}

void PerformanceBenchmark::filterBarLargeFolder()
{
    FileSystemModel model;
    waitForScan(model, m_largeDir);

    bool toggled = false;
    QBENCHMARK {
        toggled = !toggled;
        model.setFilterText(toggled ? QStringLiteral("file_000") : QString());
    }
}

void PerformanceBenchmark::filenameSearchLargeFolder()
{
    QBENCHMARK {
        SearchEngine engine;
        QSignalSpy finishedSpy(&engine, &SearchEngine::finished);

        SearchOptions options;
        options.rootPath = m_largeDir;
        options.pattern = QStringLiteral("file_025");
        options.recursive = false;
        engine.start(options);

        QVERIFY(finishedSpy.wait(30000));
    }
}

void PerformanceBenchmark::filenameSearchRecursive()
{
    QBENCHMARK {
        SearchEngine engine;
        QSignalSpy finishedSpy(&engine, &SearchEngine::finished);

        SearchOptions options;
        options.rootPath = m_recursiveDir;
        options.pattern = QStringLiteral("file_005");
        options.recursive = true;
        engine.start(options);

        QVERIFY(finishedSpy.wait(30000));
    }
}

void PerformanceBenchmark::selectionsSmallFolder()
{
    FileSystemModel model;
    waitForScan(model, m_smallDir);
    QItemSelectionModel selection(&model);
    const QModelIndex bottomRight = model.index(model.rowCount() - 1, FileSystemModel::ColumnCount - 1);

    QBENCHMARK {
        selection.select(QItemSelection(model.index(0, 0), bottomRight), QItemSelectionModel::ClearAndSelect);
    }
}

void PerformanceBenchmark::selectionsLargeFolder()
{
    FileSystemModel model;
    waitForScan(model, m_largeDir);
    QItemSelectionModel selection(&model);
    const QModelIndex bottomRight = model.index(model.rowCount() - 1, FileSystemModel::ColumnCount - 1);

    QBENCHMARK {
        selection.select(QItemSelection(model.index(0, 0), bottomRight), QItemSelectionModel::ClearAndSelect);
    }
}

void PerformanceBenchmark::selectionReadback()
{
    FileSystemModel model;
    waitForScan(model, m_largeDir);
    QItemSelectionModel selection(&model);
    const QModelIndex bottomRight = model.index(model.rowCount() - 1, FileSystemModel::ColumnCount - 1);
    selection.select(QItemSelection(model.index(0, 0), bottomRight), QItemSelectionModel::ClearAndSelect);

    QBENCHMARK {
        const auto rows = selection.selectedRows(FileSystemModel::NameColumn);
        QCOMPARE(rows.size(), model.rowCount());
    }
}

void PerformanceBenchmark::navigation()
{
    FileSystemModel model;
    waitForScan(model, m_smallDir);

    bool toggled = false;
    QBENCHMARK {
        toggled = !toggled;
        waitForScan(model, toggled ? m_secondSmallDir : m_smallDir);
    }
}

void PerformanceBenchmark::refreshPreservingSelection()
{
    // Mirrors FilePane::rememberSelection()/restoreSelection()
    // (ui/pane/filepane.cpp): note the selected paths, rescan, then
    // re-find and reselect them by walking the repopulated model -- the
    // same O(rowCount) pass every auto-refresh runs while something is
    // selected, added so external filesystem changes don't drop the
    // selection (see PLAN.md's release notes for the folder/rename
    // focus-retention work).
    FileSystemModel model;
    waitForScan(model, m_largeDir);
    QItemSelectionModel selection(&model);
    selection.select(QItemSelection(model.index(0, 0), model.index(99, 0)), QItemSelectionModel::ClearAndSelect);

    QBENCHMARK {
        QStringList keptPaths;
        for (const QModelIndex &index : selection.selectedRows(FileSystemModel::NameColumn))
            keptPaths.append(model.filePath(index));

        QSignalSpy spy(&model, &FileSystemModel::scanFinished);
        model.refresh();
        QVERIFY(spy.wait(30000));

        QItemSelection restored;
        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row, FileSystemModel::NameColumn);
            if (keptPaths.contains(model.filePath(index)))
                restored.select(index, index);
        }
        selection.select(restored, QItemSelectionModel::ClearAndSelect);
    }
}

QTEST_MAIN(PerformanceBenchmark)
#include "perftest.moc"
