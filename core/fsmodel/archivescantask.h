#pragma once

#include "core/fsmodel/directoryscantask.h"

#include <QAtomicInt>
#include <QObject>
#include <QRunnable>
#include <QSharedPointer>
#include <QString>
#include <QVector>

// Lists the direct children of one folder inside an archive, with the same
// signals as DirectoryScanTask so FileSystemModel can treat both alike.
class ArchiveScanTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    ArchiveScanTask(QString archivePath, QString innerPath, int generation, QSharedPointer<QAtomicInt> cancelled);

    void run() override;

signals:
    void batchReady(int generation, QVector<FileEntry> entries);
    void failed(int generation, QString message);
    void finished(int generation, bool wasCancelled);

private:
    QString m_archivePath;
    QString m_innerPath;
    int m_generation;
    QSharedPointer<QAtomicInt> m_cancelled;
};
