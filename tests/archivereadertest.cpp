#include "core/archive/archivereader.h"
#include "core/fsmodel/archivescantask.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <cstring>

class ArchiveReaderTest : public QObject
{
    Q_OBJECT

private slots:
    void listsFilesAndSynthesizesFolders();
    void extractsSelectedFileOnly();
    void extractsFolderRecursively();
    void rejectsMissingFile();
    void splitsVirtualPathsInsideArchives();
    void scanListsOnlyDirectChildrenOfEachFolder();
};

namespace {

// Builds a small ZIP with libarchive's own writer so the test doesn't need a
// checked-in binary fixture.
bool writeZip(const QString &zipPath)
{
    archive *a = archive_write_new();
    archive_write_set_format_zip(a);
#ifdef _WIN32
    if (archive_write_open_filename_w(a, reinterpret_cast<const wchar_t *>(zipPath.utf16())) != ARCHIVE_OK)
        return false;
#else
    if (archive_write_open_filename(a, QFile::encodeName(zipPath).constData()) != ARCHIVE_OK)
        return false;
#endif

    const struct Item {
        const char *path;
        const char *content; // nullptr for a directory
    } items[] = {
        {"top.txt", "top level"},
        {"docs/readme.md", "readme body"},
        {"docs/deep/notes.txt", "deep notes"},
    };

    for (const Item &item : items) {
        archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, item.path);
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_entry_set_size(entry, static_cast<la_int64_t>(strlen(item.content)));
        archive_entry_set_mtime(entry, 1700000000, 0);
        archive_write_header(a, entry);
        archive_write_data(a, item.content, strlen(item.content));
        archive_entry_free(entry);
    }

    archive_write_close(a);
    archive_write_free(a);
    return true;
}

QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

} // namespace

void ArchiveReaderTest::listsFilesAndSynthesizesFolders()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString zipPath = QDir(dir.path()).filePath(QStringLiteral("sample.zip"));
    QVERIFY(writeZip(zipPath));

    QVector<ArchiveEntry> entries;
    QString error;
    QVERIFY2(ArchiveReader::list(zipPath, &entries, &error), qPrintable(error));

    QStringList files;
    QStringList folders;
    for (const ArchiveEntry &entry : std::as_const(entries)) {
        if (entry.isDir)
            folders << entry.path;
        else
            files << entry.path;
    }
    QVERIFY(files.contains(QStringLiteral("top.txt")));
    QVERIFY(files.contains(QStringLiteral("docs/readme.md")));
    QVERIFY(files.contains(QStringLiteral("docs/deep/notes.txt")));
    // No explicit folder entries were written, so both must be synthesized.
    QVERIFY(folders.contains(QStringLiteral("docs")));
    QVERIFY(folders.contains(QStringLiteral("docs/deep")));
}

void ArchiveReaderTest::extractsSelectedFileOnly()
{
    QTemporaryDir archiveDir;
    QTemporaryDir outDir;
    QVERIFY(archiveDir.isValid() && outDir.isValid());
    const QString zipPath = QDir(archiveDir.path()).filePath(QStringLiteral("sample.zip"));
    QVERIFY(writeZip(zipPath));

    QStringList extracted;
    QString error;
    QVERIFY2(ArchiveReader::extract(zipPath, {QStringLiteral("top.txt")}, outDir.path(), &extracted, &error),
             qPrintable(error));

    QCOMPARE(extracted.size(), 1);
    QCOMPARE(readFile(QDir(outDir.path()).filePath(QStringLiteral("top.txt"))), QStringLiteral("top level"));
    QVERIFY(!QFile::exists(QDir(outDir.path()).filePath(QStringLiteral("docs/readme.md"))));
}

void ArchiveReaderTest::extractsFolderRecursively()
{
    QTemporaryDir archiveDir;
    QTemporaryDir outDir;
    QVERIFY(archiveDir.isValid() && outDir.isValid());
    const QString zipPath = QDir(archiveDir.path()).filePath(QStringLiteral("sample.zip"));
    QVERIFY(writeZip(zipPath));

    QStringList extracted;
    QString error;
    QVERIFY2(ArchiveReader::extract(zipPath, {QStringLiteral("docs")}, outDir.path(), &extracted, &error),
             qPrintable(error));

    QCOMPARE(extracted.size(), 2);
    QCOMPARE(readFile(QDir(outDir.path()).filePath(QStringLiteral("docs/readme.md"))), QStringLiteral("readme body"));
    QCOMPARE(readFile(QDir(outDir.path()).filePath(QStringLiteral("docs/deep/notes.txt"))),
             QStringLiteral("deep notes"));
    QVERIFY(!QFile::exists(QDir(outDir.path()).filePath(QStringLiteral("top.txt"))));
}

void ArchiveReaderTest::rejectsMissingFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVector<ArchiveEntry> entries;
    QString error;
    QVERIFY(!ArchiveReader::list(QDir(dir.path()).filePath(QStringLiteral("missing.zip")), &entries, &error));
    QVERIFY(!error.isEmpty());
}

void ArchiveReaderTest::splitsVirtualPathsInsideArchives()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString zipPath = QDir(dir.path()).filePath(QStringLiteral("sample.zip"));
    QVERIFY(writeZip(zipPath));

    QString archive;
    QString inner;
    QVERIFY(ArchiveReader::splitArchivePath(zipPath, &archive, &inner));
    QCOMPARE(archive, zipPath);
    QVERIFY(inner.isEmpty());

    const QString deep = zipPath + QStringLiteral("/docs/deep");
    QVERIFY(ArchiveReader::splitArchivePath(deep, &archive, &inner));
    QCOMPARE(archive, zipPath);
    QCOMPARE(inner, QStringLiteral("docs/deep"));

    // A real folder is never treated as an archive, even if its name ends in .zip.
    QVERIFY(!ArchiveReader::splitArchivePath(dir.path(), nullptr, nullptr));
    QVERIFY(!ArchiveReader::splitArchivePath(QDir(dir.path()).filePath(QStringLiteral("nothing/here")), nullptr,
                                             nullptr));
}

void ArchiveReaderTest::scanListsOnlyDirectChildrenOfEachFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString zipPath = QDir(dir.path()).filePath(QStringLiteral("sample.zip"));
    QVERIFY(writeZip(zipPath));

    auto collect = [&](const QString &inner) {
        QStringList names;
        auto cancel = QSharedPointer<QAtomicInt>::create(0);
        ArchiveScanTask task(zipPath, inner, 1, cancel);
        QObject::connect(&task, &ArchiveScanTask::batchReady, [&names](int, const QVector<FileEntry> &entries) {
            for (const FileEntry &entry : entries)
                names << (entry.isDir ? entry.name + QStringLiteral("/") : entry.name);
        });
        task.run();
        names.sort();
        return names;
    };

    QCOMPARE(collect(QString()), (QStringList{QStringLiteral("docs/"), QStringLiteral("top.txt")}));
    QCOMPARE(collect(QStringLiteral("docs")), (QStringList{QStringLiteral("deep/"), QStringLiteral("readme.md")}));
}

QTEST_GUILESS_MAIN(ArchiveReaderTest)
#include "archivereadertest.moc"
