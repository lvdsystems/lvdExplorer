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
        if (overwrite && QFileInfo::exists(destPath))
            QFile::remove(destPath);
        return QFile::copy(sourcePath, destPath);
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
    if (overwrite && QFileInfo::exists(destPath)) {
        // Clears the way for rename() (or the copy+delete fallback below)
        // to succeed -- unlike POSIX rename(2), QDir::rename() (MoveFileW
        // under the hood on Windows) refuses to replace an existing
        // destination.
        const QFileInfo destInfo(destPath);
        if (destInfo.isDir())
            QDir(destPath).removeRecursively();
        else
            QFile::remove(destPath);
    }

    QDir dir;
    if (dir.rename(sourcePath, destPath))
        return true;

    // rename() fails across volumes/mount points; fall back to copy+delete.
    if (cancelled() || !copyRecursively(sourcePath, destPath, done, total, overwrite))
        return false;

    const QFileInfo sourceInfo(sourcePath);
    return sourceInfo.isDir() ? QDir(sourcePath).removeRecursively() : QFile::remove(sourcePath);
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
