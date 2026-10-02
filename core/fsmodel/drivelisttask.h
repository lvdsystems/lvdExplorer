#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>
#include <QVector>

struct DriveEntry
{
    QString rootPath;
    QString label; // e.g. "Data (D:\)" -- native path plus display name when they differ
};

// Enumerates mounted volumes off the GUI thread. QStorageInfo::isReady()
// and displayName() can block for a full network timeout when a mapped
// drive points at a currently-unreachable share (VPN down, server
// offline, stale mount, etc.) -- the same reason DirectoryScanTask,
// SearchTask, and friends already keep their own filesystem work off the
// UI thread, applied here to the pane toolbar's Drives dropdown, which
// previously built its list synchronously in a QMenu::aboutToShow handler
// and could hang the whole app on an unreachable drive.
class DriveListTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    DriveListTask();

    void run() override;

signals:
    void finished(QVector<DriveEntry> drives);
};
