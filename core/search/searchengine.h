#pragma once

#include "core/fsmodel/directoryscantask.h" // FileEntry
#include "core/search/searchoptions.h"

#include <QAtomicInt>
#include <QObject>
#include <QSharedPointer>

// Orchestrates SearchTask generations the same way FileSystemModel
// orchestrates DirectoryScanTask: each start() bumps a generation and
// raises the previous task's cancel flag, and results/finish signals from
// a stale generation are silently dropped.
class SearchEngine : public QObject
{
    Q_OBJECT

public:
    explicit SearchEngine(QObject *parent = nullptr);

    void start(const SearchOptions &options);
    void cancel();

signals:
    void started();
    void matchesReady(const QVector<FileEntry> &matches);
    void finished(bool wasCancelled, int totalMatches);

private slots:
    void handleMatchesReady(int generation, QVector<FileEntry> matches);
    void handleTaskFinished(int generation, bool wasCancelled);

private:
    int m_generation = 0;
    QSharedPointer<QAtomicInt> m_cancelFlag;
    int m_totalMatches = 0;
};
