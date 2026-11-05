#pragma once

#include <QString>
#include <QStringList>

// Write-only counterpart to ArchiveReader: builds a ZIP from real files and
// folders already on disk. Blocking: callers run this off the UI thread
// (see ArchiveCreateTask).
class ArchiveWriter
{
public:
    // Creates a ZIP at zipPath containing each of sourcePaths as a
    // top-level entry. A file becomes one entry; a folder is recursed into
    // and keeps its own name as the prefix for everything nested under it
    // -- the same shape ArchiveReader::extract()/ArchiveExtractAllTask
    // expect a "top-level entry" to have the other way around. Overwrites
    // zipPath if it already exists.
    static bool createZip(const QString &zipPath, const QStringList &sourcePaths, QString *error);
};
