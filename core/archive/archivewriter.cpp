#include "core/archive/archivewriter.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

bool writeFileEntry(archive *a, const QString &diskPath, const QString &entryName, QString *error)
{
    QFile file(diskPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Could not read %1").arg(diskPath);
        return false;
    }

    archive_entry *entry = archive_entry_new();
    archive_entry_set_pathname_utf8(entry, entryName.toUtf8().constData());
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_entry_set_size(entry, file.size());
    archive_entry_set_mtime(entry, QFileInfo(diskPath).lastModified().toSecsSinceEpoch(), 0);
    const bool headerOk = archive_write_header(a, entry) == ARCHIVE_OK;
    archive_entry_free(entry);
    if (!headerOk) {
        if (error)
            *error = QString::fromUtf8(archive_error_string(a));
        return false;
    }

    char buffer[64 * 1024];
    while (true) {
        const qint64 read = file.read(buffer, sizeof buffer);
        if (read < 0) {
            if (error)
                *error = QStringLiteral("Could not read %1").arg(diskPath);
            return false;
        }
        if (read == 0)
            break;
        if (archive_write_data(a, buffer, static_cast<size_t>(read)) < 0) {
            if (error)
                *error = QString::fromUtf8(archive_error_string(a));
            return false;
        }
    }
    return true;
}

bool writeDirEntry(archive *a, const QString &entryName, const QDateTime &modified, QString *error)
{
    archive_entry *entry = archive_entry_new();
    // A trailing slash marks a directory entry, the same convention
    // ArchiveReader::normalizedEntryPath() reads back on the way in.
    archive_entry_set_pathname_utf8(entry, (entryName + QLatin1Char('/')).toUtf8().constData());
    archive_entry_set_filetype(entry, AE_IFDIR);
    archive_entry_set_perm(entry, 0755);
    archive_entry_set_mtime(entry, modified.toSecsSinceEpoch(), 0);
    const bool ok = archive_write_header(a, entry) == ARCHIVE_OK;
    if (!ok && error)
        *error = QString::fromUtf8(archive_error_string(a));
    archive_entry_free(entry);
    return ok;
}

// Recursively writes diskPath (a file or a folder) into the archive under
// entryName, the archive-relative path it should appear at.
bool addPath(archive *a, const QString &diskPath, const QString &entryName, QString *error)
{
    const QFileInfo info(diskPath);
    if (!info.isDir())
        return writeFileEntry(a, diskPath, entryName, error);

    if (!writeDirEntry(a, entryName, info.lastModified(), error))
        return false;

    const QDir dir(diskPath);
    const QStringList children = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QString &child : children) {
        if (!addPath(a, dir.filePath(child), entryName + QLatin1Char('/') + child, error))
            return false;
    }
    return true;
}

} // namespace

bool ArchiveWriter::createZip(const QString &zipPath, const QStringList &sourcePaths, QString *error)
{
    archive *a = archive_write_new();
    archive_write_set_format_zip(a);

#ifdef _WIN32
    const int status = archive_write_open_filename_w(a, reinterpret_cast<const wchar_t *>(zipPath.utf16()));
#else
    const int status = archive_write_open_filename(a, QFile::encodeName(zipPath).constData());
#endif
    if (status != ARCHIVE_OK) {
        if (error)
            *error = QString::fromUtf8(archive_error_string(a));
        archive_write_free(a);
        return false;
    }

    bool ok = true;
    for (const QString &sourcePath : sourcePaths) {
        if (!addPath(a, sourcePath, QFileInfo(sourcePath).fileName(), error)) {
            ok = false;
            break;
        }
    }

    archive_write_close(a);
    archive_write_free(a);
    if (!ok)
        QFile::remove(zipPath); // don't leave a half-written zip behind
    return ok;
}
