#pragma once

#include <QAtomicInt>
#include <QObject>
#include <QRunnable>
#include <QSharedPointer>
#include <QString>

// Computes one folder's total recursive size off the UI thread. One task
// per folder in the current listing (see
// FileSystemModel::startFolderSizeTasks()), so folders stream their size
// in independently and in parallel rather than one huge task blocking on
// the slowest subfolder.
class FolderSizeTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    FolderSizeTask(QString folderPath, int generation, QSharedPointer<QAtomicInt> cancelled);

    void run() override;

signals:
    void sizeReady(int generation, QString folderPath, qint64 size);

private:
    QString m_folderPath;
    int m_generation;
    QSharedPointer<QAtomicInt> m_cancelled;
};
