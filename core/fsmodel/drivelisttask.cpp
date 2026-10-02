#include "core/fsmodel/drivelisttask.h"

#include <QDir>
#include <QStorageInfo>

DriveListTask::DriveListTask()
{
    setAutoDelete(true);
}

void DriveListTask::run()
{
    QVector<DriveEntry> drives;
    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    drives.reserve(volumes.size());

    for (const QStorageInfo &volume : volumes) {
        // isReady() and displayName() are exactly the calls that can
        // block for a long network timeout on an unreachable mapped
        // drive -- this whole loop runs off the GUI thread for that
        // reason (see the class comment).
        if (!volume.isValid() || !volume.isReady())
            continue;

        DriveEntry entry;
        entry.rootPath = volume.rootPath();
        QString label = QDir::toNativeSeparators(entry.rootPath);
        const QString displayName = volume.displayName();
        if (!displayName.isEmpty() && displayName != label)
            label = QObject::tr("%1 (%2)").arg(displayName, label);
        entry.label = label;
        drives.append(entry);
    }

    emit finished(drives);
}
