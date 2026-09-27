#include "core/ops/foldercompare.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

class FolderCompareTest : public QObject
{
    Q_OBJECT

private slots:
    void classifiesOnlyInAOnlyInBDifferentAndIdentical();
};

namespace {
void write(const QString &path, const QByteArray &content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}
} // namespace

void FolderCompareTest::classifiesOnlyInAOnlyInBDifferentAndIdentical()
{
    QTemporaryDir dirA, dirB;
    QVERIFY(dirA.isValid() && dirB.isValid());

    write(QDir(dirA.path()).filePath(QStringLiteral("only_a.txt")), "a");
    write(QDir(dirB.path()).filePath(QStringLiteral("only_b.txt")), "b");

    const QString sameA = QDir(dirA.path()).filePath(QStringLiteral("same.txt"));
    const QString sameB = QDir(dirB.path()).filePath(QStringLiteral("same.txt"));
    write(sameA, "identical content");
    write(sameB, "identical content");
    // Written moments apart, so on filesystems with fine-grained mtime
    // resolution (NTFS: 100ns) they can otherwise land on different
    // timestamps by sheer chance, making differs() true intermittently
    // even though the content is byte-identical. Pin them to match so the
    // "identical" case is actually deterministic. setFileTime() requires
    // the file to be open to take effect -- silently does nothing
    // otherwise, which is exactly what made the first attempt at this
    // fix still flaky.
    QFile sameBFile(sameB);
    QVERIFY(sameBFile.open(QIODevice::ReadWrite));
    QVERIFY(sameBFile.setFileTime(QFileInfo(sameA).lastModified(), QFileDevice::FileModificationTime));
    sameBFile.close();

    write(QDir(dirA.path()).filePath(QStringLiteral("differs.txt")), "short");
    // Ensure a distinct mtime even on filesystems with coarse timestamp
    // resolution, so "differs" is detected by size difference regardless.
    write(QDir(dirB.path()).filePath(QStringLiteral("differs.txt")), "a longer piece of content");

    const QVector<FolderCompareEntry> entries = compareFolders(dirA.path(), dirB.path());
    QCOMPARE(entries.size(), 4);

    auto findEntry = [&](const QString &name) -> const FolderCompareEntry * {
        for (const auto &entry : entries) {
            if (entry.name == name)
                return &entry;
        }
        return nullptr;
    };

    const auto *onlyA = findEntry(QStringLiteral("only_a.txt"));
    QVERIFY(onlyA);
    QVERIFY(onlyA->onlyInA());
    QVERIFY(!onlyA->onlyInB());
    QVERIFY(!onlyA->differs());

    const auto *onlyB = findEntry(QStringLiteral("only_b.txt"));
    QVERIFY(onlyB);
    QVERIFY(onlyB->onlyInB());

    const auto *same = findEntry(QStringLiteral("same.txt"));
    QVERIFY(same);
    QVERIFY(!same->onlyInA() && !same->onlyInB() && !same->differs());

    const auto *differs = findEntry(QStringLiteral("differs.txt"));
    QVERIFY(differs);
    QVERIFY(differs->differs());
}

QTEST_GUILESS_MAIN(FolderCompareTest)
#include "foldercomparetest.moc"
