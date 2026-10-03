#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>
#include <QStringList>

// Extracts archive entries (files, or whole folders) into destDir off the UI
// thread. finished() carries the top-level paths that now exist on disk, so
// a selected folder comes back as one folder rather than its loose files.
class ArchiveExtractTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    ArchiveExtractTask(QString archivePath, QStringList entryPaths, QString destDir);

    void run() override;

signals:
    void finished(QStringList topLevelPaths, QString error);

private:
    QString m_archivePath;
    QStringList m_entryPaths;
    QString m_destDir;
};
