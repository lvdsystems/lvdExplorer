#include "core/ops/opsengine.h"

#include "core/ops/fileoptask.h"

#include <QThreadPool>

OpsEngine &OpsEngine::instance()
{
    static OpsEngine engine;
    return engine;
}

FileOpHandle *OpsEngine::submit(const FileOpRequest &request, QString *rejectionReason)
{
    const bool blockedDelete = request.kind == FileOpKind::Delete && request.sourceReadOnly;
    const bool blockedMove = request.kind == FileOpKind::Move && (request.sourceReadOnly || request.destReadOnly);

    if (blockedDelete || blockedMove) {
        if (rejectionReason) {
            *rejectionReason = blockedMove
                ? QObject::tr("Pane is read-only: move is blocked. Hold Ctrl to copy instead.")
                : QObject::tr("Pane is read-only: delete is blocked.");
        }
        return nullptr;
    }

    auto *handle = new FileOpHandle;
    m_queue.enqueue({request, handle});

    if (!m_busy)
        startNext();

    return handle;
}

void OpsEngine::cancelCurrent()
{
    if (m_cancelFlag)
        m_cancelFlag->storeRelaxed(1);
}

void OpsEngine::startNext()
{
    if (m_queue.isEmpty()) {
        m_busy = false;
        return;
    }

    m_busy = true;
    QueuedJob job = m_queue.dequeue();

    m_cancelFlag = QSharedPointer<QAtomicInt>::create(0);
    auto *task = new FileOpTask(job.request, m_cancelFlag);
    QPointer<FileOpHandle> handle = job.handle;

    connect(task, &FileOpTask::progress, this, [handle](int done, int total, const QString &item) {
        if (handle)
            emit handle->progress(done, total, item);
    });
    connect(task, &FileOpTask::finished, this, [this, handle](bool wasCancelled, const QStringList &failed) {
        if (handle) {
            emit handle->finished(wasCancelled, failed);
            handle->deleteLater();
        }
        startNext();
    });

    QThreadPool::globalInstance()->start(task);
}
