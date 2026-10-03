#include "core/archive/archivereader.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

namespace {

constexpr size_t kBlockSize = 10240;

struct ArchiveHandle
{
    archive *a = nullptr;

    ArchiveHandle()
        : a(archive_read_new())
    {
        archive_read_support_format_zip(a);
        archive_read_support_format_7zip(a);
        archive_read_support_format_rar(a);
        archive_read_support_format_rar5(a);
    }

    ~ArchiveHandle()
    {
        archive_read_free(a);
    }

    bool open(const QString &path, QString *error)
    {
#ifdef _WIN32
        const int status = archive_read_open_filename_w(a, reinterpret_cast<const wchar_t *>(path.utf16()), kBlockSize);
#else
        const int status = archive_read_open_filename(a, QFile::encodeName(path).constData(), kBlockSize);
#endif
        if (status != ARCHIVE_OK) {
            if (error)
                *error = QString::fromUtf8(archive_error_string(a));
            return false;
        }
        return true;
    }
};

// Entry names are normalized to '/'-separated and without a trailing slash;
// the caller decides whether a directory marker was present.
QString normalizedEntryPath(archive_entry *entry, bool *isDirMarker)
{
    const char *raw = archive_entry_pathname_utf8(entry);
    if (!raw)
        return {};

    QString path = QString::fromUtf8(raw).replace(QLatin1Char('\\'), QLatin1Char('/'));
    *isDirMarker = path.endsWith(QLatin1Char('/'));
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    while (path.startsWith(QLatin1String("./")))
        path.remove(0, 2);
    return path;
}

// Rejects absolute names and "..", so a crafted archive can't write outside
// the extraction folder ("zip slip").
bool isSafeRelativePath(const QString &path)
{
    if (path.isEmpty() || path.startsWith(QLatin1Char('/')) || path.contains(QLatin1Char(':')))
        return false;
    const QStringList parts = path.split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (part == QLatin1String("..") || part.isEmpty())
            return false;
    }
    return true;
}

bool matchesWanted(const QString &entryPath, const QStringList &wanted)
{
    for (const QString &want : wanted) {
        if (entryPath == want || entryPath.startsWith(want + QLatin1Char('/')))
            return true;
    }
    return false;
}

} // namespace

bool ArchiveReader::isArchiveFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("zip") || suffix == QLatin1String("7z") || suffix == QLatin1String("rar");
}

bool ArchiveReader::splitArchivePath(const QString &path, QString *archivePath, QString *innerPath)
{
    const QFileInfo direct(path);
    if (direct.isFile() && isArchiveFile(path)) {
        if (archivePath)
            *archivePath = path;
        if (innerPath)
            innerPath->clear();
        return true;
    }
    if (direct.exists())
        return false;

    QString current = path;
    QStringList trailing;
    while (true) {
        const QFileInfo info(current);
        const QString parent = info.absolutePath();
        if (parent == current || parent.isEmpty())
            return false;
        trailing.prepend(info.fileName());
        current = parent;
        const QFileInfo candidate(current);
        if (candidate.isFile() && isArchiveFile(current)) {
            if (archivePath)
                *archivePath = current;
            if (innerPath)
                *innerPath = trailing.join(QLatin1Char('/'));
            return true;
        }
        if (candidate.exists())
            return false;
    }
}

bool ArchiveReader::list(const QString &archivePath, QVector<ArchiveEntry> *entries, QString *error)
{
    entries->clear();
    ArchiveHandle handle;
    if (!handle.open(archivePath, error))
        return false;

    QSet<QString> seenDirs;
    while (true) {
        archive_entry *entry = nullptr;
        const int status = archive_read_next_header(handle.a, &entry);
        if (status == ARCHIVE_EOF)
            break;
        if (status < ARCHIVE_WARN) {
            if (error)
                *error = QString::fromUtf8(archive_error_string(handle.a));
            return false;
        }

        bool dirMarker = false;
        const QString path = normalizedEntryPath(entry, &dirMarker);
        if (path.isEmpty() || !isSafeRelativePath(path))
            continue;

        ArchiveEntry item;
        item.path = path;
        item.isDir = dirMarker || archive_entry_filetype(entry) == AE_IFDIR;
        item.size = item.isDir ? 0 : archive_entry_size(entry);
        if (archive_entry_mtime_is_set(entry))
            item.modified = QDateTime::fromSecsSinceEpoch(archive_entry_mtime(entry));
        entries->append(item);
        if (item.isDir)
            seenDirs.insert(path);
    }

    // Some archives omit explicit folder entries; synthesize them so every
    // file's parent can still be navigated into.
    const QVector<ArchiveEntry> explicitEntries = *entries;
    for (const ArchiveEntry &item : explicitEntries) {
        QString parent = item.path;
        while (true) {
            const int slash = parent.lastIndexOf(QLatin1Char('/'));
            if (slash < 0)
                break;
            parent.truncate(slash);
            if (seenDirs.contains(parent))
                continue;
            seenDirs.insert(parent);
            ArchiveEntry dir;
            dir.path = parent;
            dir.isDir = true;
            dir.modified = item.modified;
            entries->append(dir);
        }
    }
    return true;
}

bool ArchiveReader::extract(const QString &archivePath, const QStringList &entryPaths, const QString &destDir,
                            QStringList *extractedFiles, QString *error)
{
    extractedFiles->clear();
    if (entryPaths.isEmpty())
        return true;

    ArchiveHandle handle;
    if (!handle.open(archivePath, error))
        return false;

    const QDir destination(destDir);
    while (true) {
        archive_entry *entry = nullptr;
        const int status = archive_read_next_header(handle.a, &entry);
        if (status == ARCHIVE_EOF)
            break;
        if (status < ARCHIVE_WARN) {
            if (error)
                *error = QString::fromUtf8(archive_error_string(handle.a));
            return false;
        }

        bool dirMarker = false;
        const QString path = normalizedEntryPath(entry, &dirMarker);
        if (path.isEmpty() || !isSafeRelativePath(path) || !matchesWanted(path, entryPaths))
            continue;
        if (dirMarker || archive_entry_filetype(entry) == AE_IFDIR) {
            destination.mkpath(path);
            continue;
        }

        const QString outPath = destination.filePath(path);
        QDir().mkpath(QFileInfo(outPath).absolutePath());
        QFile out(outPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (error)
                *error = QStringLiteral("Could not write %1").arg(outPath);
            return false;
        }

        char buffer[64 * 1024];
        while (true) {
            const la_ssize_t read = archive_read_data(handle.a, buffer, sizeof buffer);
            if (read == 0)
                break;
            if (read < 0) {
                if (error)
                    *error = QString::fromUtf8(archive_error_string(handle.a));
                return false;
            }
            out.write(buffer, read);
        }
        out.close();
        extractedFiles->append(outPath);
    }
    return true;
}
