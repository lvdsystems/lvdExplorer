#include "core/ops/opsengine.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class OpsEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void copiesFiles();
    void movesFiles();
    void deletesFilesToTrash();
    void rejectsDeleteFromReadOnlySource();
    void rejectsMoveInvolvingReadOnlyPane();
    void copyIntoSameFolderMakesNumberedSibling();
    void conflictingCopyOverwritesWhenUserChoosesOverwrite();
    void conflictingCopySkipsWhenUserChoosesSkip();

private:
    void writeContent(const QString &path, const QByteArray &content);
};

namespace {
void touch(const QString &path)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
}

// FileOpTask's conflict prompt runs on the GUI thread via a blocking
// cross-thread call while the worker thread waits, so answering it here
// means finding the QMessageBox once it becomes the active modal widget
// and clicking a button on it -- there's no other hook to answer
// synchronously from the test. The poll timer is deliberately leaked
// (deleteLater after it fires); it self-stops after one match and the
// test process exits shortly after anyway.
void answerNextConflictDialog(QMessageBox::ButtonRole role, bool checkApplyToAll)
{
    auto *timer = new QTimer();
    timer->setInterval(10);
    QObject::connect(timer, &QTimer::timeout, [timer, role, checkApplyToAll]() {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (!box)
            return;
        if (checkApplyToAll) {
            if (QCheckBox *checkBox = box->checkBox())
                checkBox->setChecked(true);
        }
        const QList<QAbstractButton *> buttons = box->buttons();
        for (QAbstractButton *button : buttons) {
            if (box->buttonRole(button) == role) {
                timer->stop();
                timer->deleteLater();
                button->click();
                return;
            }
        }
    });
    timer->start();
}
} // namespace

void OpsEngineTest::copiesFiles()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());
    const QString source = QDir(srcDir.path()).filePath(QStringLiteral("a.txt"));
    touch(source);

    FileOpRequest request;
    request.kind = FileOpKind::Copy;
    request.sourcePaths = {source};
    request.destDir = dstDir.path();

    QString rejection;
    FileOpHandle *handle = OpsEngine::instance().submit(request, &rejection);
    QVERIFY(handle);

    QSignalSpy finishedSpy(handle, &FileOpHandle::finished);
    QVERIFY(finishedSpy.wait(5000));

    QVERIFY(QFile::exists(QDir(dstDir.path()).filePath(QStringLiteral("a.txt"))));
    QVERIFY(QFile::exists(source)); // copy leaves the source intact
}

void OpsEngineTest::movesFiles()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());
    const QString source = QDir(srcDir.path()).filePath(QStringLiteral("b.txt"));
    touch(source);

    FileOpRequest request;
    request.kind = FileOpKind::Move;
    request.sourcePaths = {source};
    request.destDir = dstDir.path();

    FileOpHandle *handle = OpsEngine::instance().submit(request);
    QVERIFY(handle);

    QSignalSpy finishedSpy(handle, &FileOpHandle::finished);
    QVERIFY(finishedSpy.wait(5000));

    QVERIFY(QFile::exists(QDir(dstDir.path()).filePath(QStringLiteral("b.txt"))));
    QVERIFY(!QFile::exists(source));
}

void OpsEngineTest::deletesFilesToTrash()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("c.txt"));
    touch(path);

    FileOpRequest request;
    request.kind = FileOpKind::Delete;
    request.sourcePaths = {path};

    FileOpHandle *handle = OpsEngine::instance().submit(request);
    QVERIFY(handle);

    QSignalSpy finishedSpy(handle, &FileOpHandle::finished);
    QVERIFY(finishedSpy.wait(5000));

    const QStringList failed = finishedSpy.first().at(1).toStringList();
    QVERIFY(failed.isEmpty());
    QVERIFY(!QFile::exists(path));
}

void OpsEngineTest::rejectsDeleteFromReadOnlySource()
{
    FileOpRequest request;
    request.kind = FileOpKind::Delete;
    request.sourcePaths = {QStringLiteral("/some/path.txt")};
    request.sourceReadOnly = true;

    QString rejection;
    FileOpHandle *handle = OpsEngine::instance().submit(request, &rejection);
    QVERIFY(!handle);
    QVERIFY(!rejection.isEmpty());
}

void OpsEngineTest::rejectsMoveInvolvingReadOnlyPane()
{
    FileOpRequest request;
    request.kind = FileOpKind::Move;
    request.sourcePaths = {QStringLiteral("/some/path.txt")};
    request.destDir = QStringLiteral("/some/dest");
    request.destReadOnly = true;

    QString rejection;
    FileOpHandle *handle = OpsEngine::instance().submit(request, &rejection);
    QVERIFY(!handle);
    QVERIFY(!rejection.isEmpty());
}

void OpsEngineTest::writeContent(const QString &path, const QByteArray &content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

void OpsEngineTest::copyIntoSameFolderMakesNumberedSibling()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = QDir(dir.path()).filePath(QStringLiteral("a.txt"));
    writeContent(source, QByteArrayLiteral("original"));

    FileOpRequest request;
    request.kind = FileOpKind::Copy;
    request.sourcePaths = {source};
    request.destDir = dir.path(); // same folder the file already lives in

    FileOpHandle *handle = OpsEngine::instance().submit(request);
    QVERIFY(handle);

    QSignalSpy finishedSpy(handle, &FileOpHandle::finished);
    QVERIFY(finishedSpy.wait(5000));

    const QStringList failed = finishedSpy.first().at(1).toStringList();
    QVERIFY(failed.isEmpty());

    // The original must be untouched (never overwritten by copying it
    // onto itself), and a "a (1).txt" sibling must exist alongside it.
    const QString numbered = QDir(dir.path()).filePath(QStringLiteral("a (1).txt"));
    QVERIFY(QFile::exists(source));
    QVERIFY(QFile::exists(numbered));

    QFile originalFile(source);
    QVERIFY(originalFile.open(QIODevice::ReadOnly));
    QCOMPARE(originalFile.readAll(), QByteArrayLiteral("original"));
}

void OpsEngineTest::conflictingCopyOverwritesWhenUserChoosesOverwrite()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());
    const QString source = QDir(srcDir.path()).filePath(QStringLiteral("a.txt"));
    const QString dest = QDir(dstDir.path()).filePath(QStringLiteral("a.txt"));
    writeContent(source, QByteArrayLiteral("new content"));
    writeContent(dest, QByteArrayLiteral("old content"));

    FileOpRequest request;
    request.kind = FileOpKind::Copy;
    request.sourcePaths = {source};
    request.destDir = dstDir.path();

    FileOpHandle *handle = OpsEngine::instance().submit(request);
    QVERIFY(handle);

    answerNextConflictDialog(QMessageBox::AcceptRole, /*checkApplyToAll=*/false);

    QSignalSpy finishedSpy(handle, &FileOpHandle::finished);
    QVERIFY(finishedSpy.wait(5000));

    const QStringList failed = finishedSpy.first().at(1).toStringList();
    QVERIFY(failed.isEmpty());

    QFile destFile(dest);
    QVERIFY(destFile.open(QIODevice::ReadOnly));
    QCOMPARE(destFile.readAll(), QByteArrayLiteral("new content"));
}

void OpsEngineTest::conflictingCopySkipsWhenUserChoosesSkip()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());
    const QString source = QDir(srcDir.path()).filePath(QStringLiteral("a.txt"));
    const QString dest = QDir(dstDir.path()).filePath(QStringLiteral("a.txt"));
    writeContent(source, QByteArrayLiteral("new content"));
    writeContent(dest, QByteArrayLiteral("old content"));

    FileOpRequest request;
    request.kind = FileOpKind::Copy;
    request.sourcePaths = {source};
    request.destDir = dstDir.path();

    FileOpHandle *handle = OpsEngine::instance().submit(request);
    QVERIFY(handle);

    answerNextConflictDialog(QMessageBox::RejectRole, /*checkApplyToAll=*/false);

    QSignalSpy finishedSpy(handle, &FileOpHandle::finished);
    QVERIFY(finishedSpy.wait(5000));

    // Skipping is a deliberate user choice, not a failure -- it must not
    // show up in the "some items could not be processed" list.
    const QStringList failed = finishedSpy.first().at(1).toStringList();
    QVERIFY(failed.isEmpty());

    QFile destFile(dest);
    QVERIFY(destFile.open(QIODevice::ReadOnly));
    QCOMPARE(destFile.readAll(), QByteArrayLiteral("old content"));
}

QTEST_MAIN(OpsEngineTest)
#include "opsenginetest.moc"
