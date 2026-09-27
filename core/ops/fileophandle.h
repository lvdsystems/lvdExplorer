#pragma once

#include <QObject>
#include <QStringList>

// Per-submission handle returned by OpsEngine::submit(). Exists so a
// caller can tell "did *my* job finish" apart from any other job in the
// queue, without OpsEngine needing to know anything about who submitted
// what. Deleted automatically shortly after finished() is emitted.
class FileOpHandle : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

signals:
    void progress(int itemsDone, int itemsTotal, const QString &currentItem);
    void finished(bool wasCancelled, const QStringList &failedItems);
};
