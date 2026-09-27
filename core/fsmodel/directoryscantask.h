#pragma once

#include <QAtomicInt>
#include <QDateTime>
#include <QObject>
#include <QRunnable>
#include <QSharedPointer>
#include <QString>
#include <QVector>

struct FileEntry
{
    QString name;
    QString absolutePath;
    qint64 size = -1;
    bool isDir = false;
    bool isSymLink = false;
    QDateTime modified;
};

// Enumerates one directory off the GUI thread and streams results back in
// batches. Cancellation is checked between entries so navigating away
// stops the scan promptly instead of finishing a wasted listing.
class DirectoryScanTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    DirectoryScanTask(QString path, int generation, QSharedPointer<QAtomicInt> cancelled);

    void run() override;

signals:
    void batchReady(int generation, QVector<FileEntry> entries);
    void finished(int generation, bool wasCancelled);

private:
    QString m_path;
    int m_generation;
    QSharedPointer<QAtomicInt> m_cancelled;
};
