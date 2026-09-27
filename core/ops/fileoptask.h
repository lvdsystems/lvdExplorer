#pragma once

#include "core/ops/fileoprequest.h"

#include <QAtomicInt>
#include <QObject>
#include <QRunnable>
#include <QSharedPointer>
#include <QStringList>

// Executes one FileOpRequest off the UI thread, reporting per-item (and,
// for recursive copies/moves, per-nested-file) progress and checking
// cancellation between every file. Progress denominator is the number of
// *top-level* selected items, not a full recursive pre-count -- avoids an
// extra directory walk before work even starts, at the cost of the
// percentage bar not advancing granularly while a single large folder
// copies (the current-item label still updates continuously in that case).
class FileOpTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    FileOpTask(FileOpRequest request, QSharedPointer<QAtomicInt> cancelled);

    void run() override;

signals:
    void progress(int itemsDone, int itemsTotal, QString currentItem);
    void finished(bool wasCancelled, QStringList failedItems);

private:
    // What to do about a destination that already exists. Overwrite/Skip
    // answer just the current item; *All persists for the rest of this
    // task's run() loop so a bulk copy with several conflicts doesn't ask
    // once per file. Cancel aborts the whole operation, same as the
    // existing cancel button.
    enum class ConflictChoice { Overwrite, OverwriteAll, Skip, SkipAll, Cancel };

    bool cancelled() const { return m_cancelled->loadRelaxed() != 0; }
    bool copyRecursively(const QString &sourcePath, const QString &destPath, int done, int total,
                          bool overwrite = false);
    bool moveOne(const QString &sourcePath, const QString &destPath, int done, int total, bool overwrite = false);
    ConflictChoice askConflictResolution(const QString &sourcePath, const QString &destPath);

    FileOpRequest m_request;
    QSharedPointer<QAtomicInt> m_cancelled;
};
