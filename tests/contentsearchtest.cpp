#include "core/search/contentsearchengine.h"
#include "core/search/contentsearchoptions.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

class ContentSearchTest : public QObject
{
    Q_OBJECT

private slots:
    void findsSubstringMatchRecursively();
    void findsRegexMatch();
    void respectsNonRecursiveScope();
    void skipsBinaryFiles();
    void cancelStopsFurtherMatches();
};

namespace {

void writeLines(const QString &path, const QStringList &lines)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QTextStream stream(&file);
    for (const QString &line : lines)
        stream << line << '\n';
}

QString makeTempTree(const QTemporaryDir &dir)
{
    QDir root(dir.path());
    root.mkpath(QStringLiteral("sub"));
    writeLines(root.filePath(QStringLiteral("top.txt")), {QStringLiteral("hello world"), QStringLiteral("needle here")});
    writeLines(root.filePath(QStringLiteral("sub/nested.txt")), {QStringLiteral("another needle line")});
    writeLines(root.filePath(QStringLiteral("unrelated.txt")), {QStringLiteral("nothing to see")});
    return dir.path();
}

} // namespace

void ContentSearchTest::findsSubstringMatchRecursively()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeTempTree(dir);

    ContentSearchEngine engine;
    QSignalSpy finishedSpy(&engine, &ContentSearchEngine::finished);
    QVector<ContentMatch> allMatches;
    connect(&engine, &ContentSearchEngine::matchesReady,
            [&](const QVector<ContentMatch> &m) { allMatches += m; });

    ContentSearchOptions options;
    options.rootPath = root;
    options.pattern = QStringLiteral("needle");
    options.recursive = true;
    engine.start(options);

    QVERIFY(finishedSpy.wait(5000));

    QCOMPARE(allMatches.size(), 2);
    QStringList lines;
    for (const auto &match : allMatches)
        lines << match.lineText;
    QVERIFY(lines.contains(QStringLiteral("needle here")));
    QVERIFY(lines.contains(QStringLiteral("another needle line")));
}

void ContentSearchTest::findsRegexMatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeTempTree(dir);

    ContentSearchEngine engine;
    QSignalSpy finishedSpy(&engine, &ContentSearchEngine::finished);
    QVector<ContentMatch> allMatches;
    connect(&engine, &ContentSearchEngine::matchesReady,
            [&](const QVector<ContentMatch> &m) { allMatches += m; });

    ContentSearchOptions options;
    options.rootPath = root;
    options.pattern = QStringLiteral("^.*needle.*$");
    options.mode = ContentSearchOptions::Mode::Regex;
    options.recursive = true;
    engine.start(options);

    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(allMatches.size(), 2);
}

void ContentSearchTest::respectsNonRecursiveScope()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeTempTree(dir);

    ContentSearchEngine engine;
    QSignalSpy finishedSpy(&engine, &ContentSearchEngine::finished);
    QVector<ContentMatch> allMatches;
    connect(&engine, &ContentSearchEngine::matchesReady,
            [&](const QVector<ContentMatch> &m) { allMatches += m; });

    ContentSearchOptions options;
    options.rootPath = root;
    options.pattern = QStringLiteral("needle");
    options.recursive = false;
    engine.start(options);

    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(allMatches.size(), 1);
    QCOMPARE(allMatches.first().lineText, QStringLiteral("needle here"));
}

void ContentSearchTest::skipsBinaryFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir root(dir.path());

    // A "binary" file that would otherwise textually contain the pattern,
    // to prove the NUL-byte sniff (not just luck) is what excludes it.
    QFile binaryFile(root.filePath(QStringLiteral("data.bin")));
    QVERIFY(binaryFile.open(QIODevice::WriteOnly));
    QByteArray payload("needle");
    payload.insert(0, '\0');
    binaryFile.write(payload);
    binaryFile.close();

    writeLines(root.filePath(QStringLiteral("plain.txt")), {QStringLiteral("needle in text")});

    ContentSearchEngine engine;
    QSignalSpy finishedSpy(&engine, &ContentSearchEngine::finished);
    QVector<ContentMatch> allMatches;
    connect(&engine, &ContentSearchEngine::matchesReady,
            [&](const QVector<ContentMatch> &m) { allMatches += m; });

    ContentSearchOptions options;
    options.rootPath = dir.path();
    options.pattern = QStringLiteral("needle");
    options.recursive = true;
    engine.start(options);

    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(allMatches.size(), 1);
    QCOMPARE(QFileInfo(allMatches.first().filePath).fileName(), QStringLiteral("plain.txt"));
}

void ContentSearchTest::cancelStopsFurtherMatches()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // A tree large enough that cancel() called immediately after start()
    // has a realistic chance of landing mid-scan, same reasoning as
    // SearchEngineTest's equivalent case.
    QDir root(dir.path());
    for (int i = 0; i < 20; ++i) {
        const QString sub = QStringLiteral("d%1").arg(i);
        QVERIFY(root.mkpath(sub));
        for (int j = 0; j < 100; ++j)
            writeLines(root.filePath(QStringLiteral("%1/file%2.txt").arg(sub).arg(j)),
                       {QStringLiteral("needle line one"), QStringLiteral("needle line two")});
    }

    ContentSearchEngine engine;
    QSignalSpy finishedSpy(&engine, &ContentSearchEngine::finished);

    ContentSearchOptions options;
    options.rootPath = dir.path();
    options.pattern = QStringLiteral("needle");
    options.recursive = true;
    engine.start(options);
    engine.cancel();

    QVERIFY(finishedSpy.wait(5000));
    // finished(wasCancelled, totalMatches) -- wasCancelled is the first arg.
    QCOMPARE(finishedSpy.first().at(0).toBool(), true);
}

QTEST_GUILESS_MAIN(ContentSearchTest)
#include "contentsearchtest.moc"
