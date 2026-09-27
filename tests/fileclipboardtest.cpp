#include "ui/clipboard/fileclipboard.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QTest>
#include <QUrl>

class FileClipboardTest : public QObject
{
    Q_OBJECT

private slots:
    void copyRoundTrip();
    void cutRoundTrip();
    void externalContentDefaultsToNotReadOnly();
};

void FileClipboardTest::copyRoundTrip()
{
    const QStringList paths = {QStringLiteral("/tmp/a.txt"), QStringLiteral("/tmp/b.txt")};
    FileClipboard::set(paths, /*cut=*/false, /*sourceReadOnly=*/true);

    const FileClipboard::Contents contents = FileClipboard::get();
    QCOMPARE(contents.paths, paths);
    QVERIFY(!contents.cut);
    // sourceReadOnly is still tracked for a Copy -- OpsEngine never blocks
    // Copy on it, but Paste always passes through whatever get() reports.
    QVERIFY(contents.sourceReadOnly);
}

void FileClipboardTest::cutRoundTrip()
{
    const QStringList paths = {QStringLiteral("/tmp/c.txt")};
    FileClipboard::set(paths, /*cut=*/true, /*sourceReadOnly=*/false);

    const FileClipboard::Contents contents = FileClipboard::get();
    QCOMPARE(contents.paths, paths);
    QVERIFY(contents.cut);
    QVERIFY(!contents.sourceReadOnly);
}

void FileClipboardTest::externalContentDefaultsToNotReadOnly()
{
    // Simulates pasting something lvdExplorer itself didn't place there (a
    // copy made in Explorer/Nautilus, say): FileClipboard::set() was never
    // called for this content, so get() must not attribute its
    // last-remembered sourceReadOnly to it -- same convention as an
    // external drag-and-drop with no source pane (FilePane::handleFilesDropped).
    FileClipboard::set({QStringLiteral("/tmp/mine.txt")}, /*cut=*/true, /*sourceReadOnly=*/true);

    auto *externalMimeData = new QMimeData;
    externalMimeData->setUrls({QUrl::fromLocalFile(QStringLiteral("/tmp/external.txt"))});
    QApplication::clipboard()->setMimeData(externalMimeData);

    const FileClipboard::Contents contents = FileClipboard::get();
    QCOMPARE(contents.paths, QStringList{QStringLiteral("/tmp/external.txt")});
    QVERIFY(!contents.sourceReadOnly);
}

QTEST_MAIN(FileClipboardTest)
#include "fileclipboardtest.moc"
