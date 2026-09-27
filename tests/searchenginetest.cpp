#include "core/search/searchengine.h"
#include "core/search/searchoptions.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class SearchEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void findsSubstringMatchRecursively();
    void findsRegexMatch();
    void respectsNonRecursiveScope();
    void cancelStopsFurtherMatches();
};

namespace {

void touch(const QString &path)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
}

QString makeTempTree(const QTemporaryDir &dir)
{
    QDir root(dir.path());
    root.mkpath(QStringLiteral("sub"));
    touch(root.filePath(QStringLiteral("needle.txt")));
    touch(root.filePath(QStringLiteral("sub/needle_nested.txt")));
    touch(root.filePath(QStringLiteral("unrelated.txt")));
    return dir.path();
}

} // namespace

void SearchEngineTest::findsSubstringMatchRecursively()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeTempTree(dir);

    SearchEngine engine;
    QSignalSpy finishedSpy(&engine, &SearchEngine::finished);
    QVector<FileEntry> allMatches;
    connect(&engine, &SearchEngine::matchesReady,
            [&](const QVector<FileEntry> &m) { allMatches += m; });

    SearchOptions options;
    options.rootPath = root;
    options.pattern = QStringLiteral("needle");
    options.recursive = true;
    engine.start(options);

    QVERIFY(finishedSpy.wait(5000));

    QCOMPARE(allMatches.size(), 2);
    QStringList names;
    for (const auto &entry : allMatches)
        names << entry.name;
    QVERIFY(names.contains(QStringLiteral("needle.txt")));
    QVERIFY(names.contains(QStringLiteral("needle_nested.txt")));
}

void SearchEngineTest::findsRegexMatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeTempTree(dir);

    SearchEngine engine;
    QSignalSpy finishedSpy(&engine, &SearchEngine::finished);
    QVector<FileEntry> allMatches;
    connect(&engine, &SearchEngine::matchesReady,
            [&](const QVector<FileEntry> &m) { allMatches += m; });

    SearchOptions options;
    options.rootPath = root;
    options.pattern = QStringLiteral("^needle.*\\.txt$");
    options.mode = SearchOptions::Mode::Regex;
    options.recursive = true;
    engine.start(options);

    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(allMatches.size(), 2);
}

void SearchEngineTest::respectsNonRecursiveScope()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeTempTree(dir);

    SearchEngine engine;
    QSignalSpy finishedSpy(&engine, &SearchEngine::finished);
    QVector<FileEntry> allMatches;
    connect(&engine, &SearchEngine::matchesReady,
            [&](const QVector<FileEntry> &m) { allMatches += m; });

    SearchOptions options;
    options.rootPath = root;
    options.pattern = QStringLiteral("needle");
    options.recursive = false;
    engine.start(options);

    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(allMatches.size(), 1);
    QCOMPARE(allMatches.first().name, QStringLiteral("needle.txt"));
}

void SearchEngineTest::cancelStopsFurtherMatches()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // A tree large enough that cancel() called immediately after start()
    // has a realistic chance of actually landing mid-scan -- against the
    // tiny 3-file tree the other tests use, this would just race a
    // near-instant scan and be flaky.
    QDir root(dir.path());
    for (int i = 0; i < 20; ++i) {
        const QString sub = QStringLiteral("d%1").arg(i);
        QVERIFY(root.mkpath(sub));
        for (int j = 0; j < 200; ++j)
            touch(root.filePath(QStringLiteral("%1/file%2.txt").arg(sub).arg(j)));
    }

    SearchEngine engine;
    QSignalSpy finishedSpy(&engine, &SearchEngine::finished);

    SearchOptions options;
    options.rootPath = dir.path();
    options.pattern = QStringLiteral("file");
    options.recursive = true;
    engine.start(options);
    engine.cancel();

    QVERIFY(finishedSpy.wait(5000));
    // finished(wasCancelled, totalMatches) -- wasCancelled is the first arg.
    QCOMPARE(finishedSpy.first().at(0).toBool(), true);
}

QTEST_GUILESS_MAIN(SearchEngineTest)
#include "searchenginetest.moc"
