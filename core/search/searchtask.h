#pragma once

#include "core/fsmodel/directoryscantask.h" // reuses FileEntry
#include "core/search/searchoptions.h"

#include <QAtomicInt>
#include <QObject>
#include <QRegularExpression>
#include <QRunnable>
#include <QSharedPointer>
#include <QVector>

// Off-UI-thread recursive filename search, streaming matches back in
// batches with cancellation checked between entries -- same pattern as
// DirectoryScanTask. v1 scope note:
// this is a single background task doing the whole recursive walk. Still
// fully async and cancelable, just not parallelized across workers yet.
class SearchTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    SearchTask(SearchOptions options, int generation, QSharedPointer<QAtomicInt> cancelled);

    void run() override;

signals:
    void matchesReady(int generation, QVector<FileEntry> matches);
    void finished(int generation, bool wasCancelled);

private:
    bool matches(const QString &name) const;

    SearchOptions m_options;
    int m_generation;
    QSharedPointer<QAtomicInt> m_cancelled;
    QRegularExpression m_regex;
};
