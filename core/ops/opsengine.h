#pragma once

#include "core/ops/fileophandle.h"
#include "core/ops/fileoprequest.h"

#include <QAtomicInt>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QSharedPointer>

// The app-wide "progress queue": jobs run one at a time on
// QThreadPool via FileOpTask, queued in submission order. A process-wide
// singleton rather than something threaded through FilePane/PaneContainer/
// PaneGridWidget constructors -- same pragmatic choice as
// QThreadPool::globalInstance(), which the rest of the codebase already
// relies on.
//
// This is also the single centralized read-only enforcement point for
// §10: submit() rejects (returns nullptr) a Delete whose source pane is
// read-only, or a Move where either the source or destination pane is
// read-only, before the job ever reaches the queue.
class OpsEngine : public QObject
{
    Q_OBJECT

public:
    static OpsEngine &instance();

    // Returns nullptr if rejected outright (read-only violation), with
    // *rejectionReason set to a user-facing message. Otherwise returns a
    // handle scoped to this submission; it emits finished() and then
    // deletes itself shortly after.
    FileOpHandle *submit(const FileOpRequest &request, QString *rejectionReason = nullptr);

    void cancelCurrent();

private:
    OpsEngine() = default;

    struct QueuedJob
    {
        FileOpRequest request;
        QPointer<FileOpHandle> handle;
    };

    void startNext();

    QQueue<QueuedJob> m_queue;
    bool m_busy = false;
    QSharedPointer<QAtomicInt> m_cancelFlag;
};
