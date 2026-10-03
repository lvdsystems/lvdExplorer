#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

struct ArchiveEntry
{
    QString path; // inside the archive, '/'-separated, no trailing slash
    bool isDir = false;
    qint64 size = 0;
    QDateTime modified;
};

// Read-only access to ZIP, 7z and RAR archives through libarchive. Blocking:
// callers run these off the UI thread.
class ArchiveReader
{
public:
    static bool isArchiveFile(const QString &path);

    // A folder inside an archive is addressed as "<archive>/<inner>" -- the
    // same string a breadcrumb and session state can carry. Returns true
    // when path is (or lies within) an existing archive file.
    static bool splitArchivePath(const QString &path, QString *archivePath, QString *innerPath);

    static bool list(const QString &archivePath, QVector<ArchiveEntry> *entries, QString *error);

    // Extracts each requested entry, plus everything beneath it when it's a
    // directory, into destDir keeping the archive-relative layout.
    static bool extract(const QString &archivePath, const QStringList &entryPaths, const QString &destDir,
                        QStringList *extractedFiles, QString *error);
};
