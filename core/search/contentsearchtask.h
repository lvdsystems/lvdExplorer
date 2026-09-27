#pragma once

#include "core/search/contentmatch.h"
#include "core/search/contentsearchoptions.h"

#include <QAtomicInt>
#include <QObject>
#include <QRegularExpression>
#include <QRunnable>
#include <QSharedPointer>
#include <QVector>

// Off-UI-thread recursive content ("grep") search: walks the tree the same
// way SearchTask does, but opens each candidate file and tests it line by
// line instead of matching the file name. Skips files that look binary (a
// NUL byte in the first few KB) and files above kMaxFileSize, so a stray
// video or archive under the search root can't stall the scan or flood the
// results with garbage. Same v1 scope note as SearchTask: one task does
// the whole walk rather than fanning out across a QThreadPool.
class ContentSearchTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    ContentSearchTask(ContentSearchOptions options, int generation, QSharedPointer<QAtomicInt> cancelled);

    void run() override;

signals:
    void matchesReady(int generation, QVector<ContentMatch> matches);
    void finished(int generation, bool wasCancelled);

private:
    bool matches(const QString &line) const;
    bool looksBinary(const QString &filePath) const;
    void searchFile(const QString &filePath, QVector<ContentMatch> &batch);

    ContentSearchOptions m_options;
    int m_generation;
    QSharedPointer<QAtomicInt> m_cancelled;
    QRegularExpression m_regex;
};
