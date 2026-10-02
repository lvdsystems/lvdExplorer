#pragma once

#include <QAtomicInt>
#include <QObject>
#include <QRunnable>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

struct ChecksumResult
{
    QString path;        // absolute path -- used for the tooltip/copy, not shown directly
    QString displayName; // what the File column shows: just the filename for a directly
                          // selected file, or "FolderName/relative/path.ext" for a file
                          // found by recursing into a selected folder
    QString md5;
    QString sha1;
    QString sha256;
    bool ok = false;
};

// Computes MD5/SHA-1/SHA-256 for a list of files off the UI thread,
// reading each in 1 MiB chunks so both memory use on huge files and
// cancellation latency stay bounded. A directory in the input list is
// expanded into every file it contains (recursively) rather than failing
// outright -- QFile can't "open" a directory for reading.
class ChecksumTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    ChecksumTask(QStringList paths, QSharedPointer<QAtomicInt> cancelled);

    void run() override;

signals:
    void resultReady(ChecksumResult result);
    void finished(bool wasCancelled);

private:
    QStringList m_paths;
    QSharedPointer<QAtomicInt> m_cancelled;
};
