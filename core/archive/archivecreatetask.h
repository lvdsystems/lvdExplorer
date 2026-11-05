#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>
#include <QStringList>

// Builds a ZIP from sourcePaths in the background. Used by the "Create
// Zip..." context-menu action.
class ArchiveCreateTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    ArchiveCreateTask(QString zipPath, QStringList sourcePaths);

    void run() override;

signals:
    // zipPath echoes the constructor argument; empty (with a non-empty
    // error) on failure.
    void finished(QString zipPath, QString error);

private:
    QString m_zipPath;
    QStringList m_sourcePaths;
};
