#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>
#include <QStringList>

// Lists an archive's top-level entries and extracts all of them into
// destDir, in one background task. Used by the "Extract Here"/"Extract to
// <folder>" context-menu actions, where -- unlike copying a selection
// already browsed inside the archive -- the caller doesn't know in advance
// which entries exist.
class ArchiveExtractAllTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    ArchiveExtractAllTask(QString archivePath, QString destDir);

    void run() override;

signals:
    // extractedTopLevelPaths is empty (with a non-empty error) on failure.
    void finished(QStringList extractedTopLevelPaths, QString error);

private:
    QString m_archivePath;
    QString m_destDir;
};
