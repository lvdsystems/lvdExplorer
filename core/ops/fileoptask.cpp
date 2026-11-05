#include "core/ops/fileoptask.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>

namespace {

// "name.ext" -> "name (1).ext", skipping any already-taken numbers.
// Directories never have their name split on a dot -- a folder literally
// named "my.folder" must become "my.folder (1)", not "my (1).folder".
QString uniqueCopyDestination(const QString &path)
{
    const QFileInfo info(path);
    const QDir dir = info.absoluteDir();
    const QString suffix = info.isDir() ? QString() : info.suffix();
    const QString baseName = suffix.isEmpty() ? info.fileName() : info.completeBaseName();

    int n = 1;
    QString candidate;
    do {
        const QString numberedBase = QStringLiteral("%1 (%2)").arg(baseName).arg(n++);
        candidate = suffix.isEmpty() ? dir.filePath(numberedBase) : dir.filePath(numberedBase + QLatin1Char('.') + suffix);
    } while (QFileInfo::exists(candidate));
    return candidate;
}

// Windows copies inherit the source's read-only attribute, and removing a
// read-only file fails, which would otherwise block every later overwrite.
void removeExistingFile(const QString &path)
{
    QFile::setPermissions(path, QFile::permissions(path) | QFile::WriteOwner);
    QFile::remove(path);
}

// A path, next to `path`, that nothing currently occupies -- scratch space
// for building a replacement before swapping it into `path` (see
// replaceWithBackup() below).
QString temporarySiblingPath(const QString &path)
{
    QString candidate = path + QStringLiteral(".lvdexplorer-tmp");
    int n = 0;
    while (QFileInfo::exists(candidate))
        candidate = path + QStringLiteral(".lvdexplorer-tmp%1").arg(++n);
    return candidate;
}

// Best-effort removal of scratch output left behind by a failed/cancelled
// replace -- never the caller's actual source or destination.
void removeTempDebris(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists())
        return;
    if (info.isDir())
        QDir(path).removeRecursively();
    else
        removeExistingFile(path);
}

// Swaps `newContentPath` into `destPath`, which may already exist (as a
// file or a directory). If it does, its current content is renamed aside
// and only actually removed once `newContentPath` has landed at `destPath`
// -- so a failure partway through this function restores the original
// instead of leaving `destPath` missing or empty. This matters because
// QDir::rename() (unlike POSIX rename(2)) always refuses to replace an
// existing target on every platform, which is exactly why callers can't
// just rename straight over `destPath` and call it done.
bool replaceWithBackup(const QString &destPath, const QString &newContentPath)
{
    QDir dir;
    if (!QFileInfo::exists(destPath))
        return dir.rename(newContentPath, destPath);

    const QString backupPath = temporarySiblingPath(destPath);
    if (!dir.rename(destPath, backupPath))
        return false; // couldn't get the original out of the way; nothing touched yet

    if (dir.rename(newContentPath, destPath)) {
        removeTempDebris(backupPath);
        return true;
    }

    // Same-volume rename of something we just created failing is not
    // expected, but if it somehow does, restore the original rather than
    // leaving destPath empty.
    dir.rename(backupPath, destPath);
    return false;
}

} // namespace

FileOpTask::FileOpTask(FileOpRequest request, QSharedPointer<QAtomicInt> cancelled)
    : m_request(std::move(request))
    , m_cancelled(std::move(cancelled))
{
    setAutoDelete(true);
}

bool FileOpTask::copyRecursively(const QString &sourcePath, const QString &destPath, int done, int total,
                                  bool overwrite)
{
    if (cancelled())
        return false;

    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.isDir()) {
        if (!overwrite || !QFileInfo::exists(destPath))
            return QFile::copy(sourcePath, destPath);

        // Destination exists and is being overwritten: copy to a scratch
        // path first and only replace the existing file once that copy is
        // confirmed complete, rather than deleting it up front and hoping
        // the copy that's supposed to replace it actually succeeds.
        const QString tempDest = temporarySiblingPath(destPath);
        if (!QFile::copy(sourcePath, tempDest))
            return false;
        if (replaceWithBackup(destPath, tempDest))
            return true;
        removeTempDebris(tempDest);
        return false;
    }

    QDir destDirMaker;
    if (!destDirMaker.mkpath(destPath))
        return false;

    const QDir sourceDir(sourcePath);
    const QStringList entries = sourceDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        if (cancelled())
            return false;

        const QString childSource = sourceDir.filePath(entry);
        const QString childDest = QDir(destPath).filePath(entry);
        emit progress(done, total, entry); // keeps the current-item label live during a big nested copy
        // overwrite propagates into the merge: a folder-level "overwrite"
        // answer settles every nested conflict inside it too, rather than
        // asking again per file.
        if (!copyRecursively(childSource, childDest, done, total, overwrite))
            return false;
    }
    return true;
}

bool FileOpTask::moveOne(const QString &sourcePath, const QString &destPath, int done, int total, bool overwrite)
{
    QDir dir;

    if (!overwrite || !QFileInfo::exists(destPath)) {
        if (dir.rename(sourcePath, destPath))
            return true;

        // rename() fails across volumes/mount points; fall back to copy+delete.
        if (cancelled() || !copyRecursively(sourcePath, destPath, done, total))
            return false;

        const QFileInfo sourceInfo(sourcePath);
        return sourceInfo.isDir() ? QDir(sourcePath).removeRecursively() : QFile::remove(sourcePath);
    }

    // Overwriting an existing destination: move/copy the source into a
    // scratch path next to it first, and only swap that into place once
    // it's fully there (see replaceWithBackup()). The previous approach --
    // delete destPath, *then* try to (re)create it -- meant any failure in
    // between (a full disk, a permissions error, an AV lock, a cross-volume
    // copy that dies partway through, ...) destroyed the destination's
    // existing content for nothing, with no way back. This way the
    // original survives any failure short of the final on-disk swap
    // itself, and the source is only ever removed once the replacement has
    // actually landed.
    const QString tempDest = temporarySiblingPath(destPath);
    const bool sourceAlreadyMoved = dir.rename(sourcePath, tempDest);
    const bool haveReplacement =
        sourceAlreadyMoved || (!cancelled() && copyRecursively(sourcePath, tempDest, done, total));

    if (!haveReplacement) {
        removeTempDebris(tempDest);
        return false;
    }

    if (!replaceWithBackup(destPath, tempDest)) {
        // Couldn't swap the replacement in -- put it back where it came
        // from instead of leaving it orphaned under a temp name.
        if (sourceAlreadyMoved)
            dir.rename(tempDest, sourcePath);
        else
            removeTempDebris(tempDest);
        return false;
    }

    if (!sourceAlreadyMoved) {
        const QFileInfo sourceInfo(sourcePath);
        sourceInfo.isDir() ? QDir(sourcePath).removeRecursively() : QFile::remove(sourcePath);
    }
    return true;
}

FileOpTask::ConflictChoice FileOpTask::askConflictResolution(const QString &sourcePath, const QString &destPath)
{
    Q_UNUSED(sourcePath);

    // FileOpTask runs on a QThreadPool worker thread, but the dialog has
    // to show on the GUI thread -- invokeMethod's functor overload with a
    // return-value pointer, called with BlockingQueuedConnection, posts
    // the call there and blocks this thread until the user answers.
    ConflictChoice result = ConflictChoice::Cancel;
    QMetaObject::invokeMethod(
        qApp,
        [destPath]() -> ConflictChoice {
            QMessageBox box;
            box.setIcon(QMessageBox::Warning);
            box.setWindowTitle(QObject::tr("File Already Exists"));
            box.setText(QObject::tr("“%1” already exists in the destination folder.")
                            .arg(QFileInfo(destPath).fileName()));
            box.setInformativeText(QObject::tr("Do you want to overwrite it?"));

            auto *applyToAllBox = new QCheckBox(QObject::tr("Apply to all remaining conflicts"), &box);
            box.setCheckBox(applyToAllBox);

            QPushButton *overwriteButton = box.addButton(QObject::tr("Overwrite"), QMessageBox::AcceptRole);
            QPushButton *skipButton = box.addButton(QObject::tr("Skip"), QMessageBox::RejectRole);
            box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(skipButton);
            box.exec();

            const bool applyToAll = applyToAllBox->isChecked();
            if (box.clickedButton() == overwriteButton)
                return applyToAll ? ConflictChoice::OverwriteAll : ConflictChoice::Overwrite;
            if (box.clickedButton() == skipButton)
                return applyToAll ? ConflictChoice::SkipAll : ConflictChoice::Skip;
            return ConflictChoice::Cancel;
        },
        Qt::BlockingQueuedConnection, &result);

    return result;
}

void FileOpTask::run()
{
    const int total = m_request.sourcePaths.size();
    int done = 0;
    QStringList failed;
    bool wasCancelled = false;

    // Persist across the loop so one "apply to all" answer settles every
    // later conflict in this same operation instead of asking per file.
    bool overwriteAllRemaining = false;
    bool skipAllRemaining = false;

    for (const QString &sourcePath : m_request.sourcePaths) {
        if (cancelled()) {
            wasCancelled = true;
            break;
        }

        const QString fileName = QFileInfo(sourcePath).fileName();
        emit progress(done, total, fileName);

        bool ok = true;
        bool skippedByUser = false;

        switch (m_request.kind) {
        case FileOpKind::Delete:
            ok = QFile::moveToTrash(sourcePath);
            break;
        case FileOpKind::Copy:
        case FileOpKind::Move: {
            QString destPath = QDir(m_request.destDir).filePath(fileName);
            if (QDir::cleanPath(sourcePath) == QDir::cleanPath(destPath)) {
                if (m_request.kind == FileOpKind::Copy) {
                    // Pasting into the same folder it came from: never
                    // overwrite the source itself -- make an auto-numbered
                    // sibling copy instead, matching Explorer/Nautilus's
                    // "Copy (1)" convention.
                    destPath = uniqueCopyDestination(destPath);
                    ok = copyRecursively(sourcePath, destPath, done, total);
                }
                // Same-location Move is a no-op (nothing to rename to);
                // ok stays true, matching prior behavior.
                break;
            }

            bool overwrite = false;
            if (QFileInfo::exists(destPath)) {
                if (skipAllRemaining) {
                    skippedByUser = true;
                } else if (overwriteAllRemaining) {
                    overwrite = true;
                } else {
                    switch (askConflictResolution(sourcePath, destPath)) {
                    case ConflictChoice::Overwrite:
                        overwrite = true;
                        break;
                    case ConflictChoice::OverwriteAll:
                        overwrite = true;
                        overwriteAllRemaining = true;
                        break;
                    case ConflictChoice::Skip:
                        skippedByUser = true;
                        break;
                    case ConflictChoice::SkipAll:
                        skippedByUser = true;
                        skipAllRemaining = true;
                        break;
                    case ConflictChoice::Cancel:
                        wasCancelled = true;
                        break;
                    }
                }
            }

            if (wasCancelled)
                break;
            if (!skippedByUser) {
                ok = m_request.kind == FileOpKind::Copy ? copyRecursively(sourcePath, destPath, done, total, overwrite)
                                                         : moveOne(sourcePath, destPath, done, total, overwrite);
            }
            break;
        }
        }

        if (wasCancelled)
            break;
        if (!ok && !skippedByUser)
            failed.append(fileName);
        ++done;
        emit progress(done, total, fileName);
    }

    emit finished(wasCancelled, failed);
}
