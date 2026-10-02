#include "core/ops/checksumtask.h"

#include <QAtomicInt>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSharedPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThreadPool>

class ChecksumTaskTest : public QObject
{
    Q_OBJECT

private slots:
    void hashesASingleFile();
    void recursesIntoAFolder();
};

namespace {

void writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

} // namespace

void ChecksumTaskTest::hashesASingleFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = QDir(dir.path()).filePath(QStringLiteral("hello.txt"));
    writeFile(filePath, QByteArrayLiteral("hello world"));

    auto cancelFlag = QSharedPointer<QAtomicInt>::create(0);
    auto *task = new ChecksumTask({filePath}, cancelFlag);
    QSignalSpy finishedSpy(task, &ChecksumTask::finished);
    QVector<ChecksumResult> results;
    connect(task, &ChecksumTask::resultReady, this,
            [&results](const ChecksumResult &r) { results << r; });

    QThreadPool::globalInstance()->start(task);
    QVERIFY(finishedSpy.wait(5000));

    QCOMPARE(results.size(), 1);
    QVERIFY(results.first().ok);
    QCOMPARE(results.first().displayName, QStringLiteral("hello.txt"));
    QCOMPARE(results.first().md5,
             QString::fromLatin1(QCryptographicHash::hash(QByteArrayLiteral("hello world"),
                                                            QCryptographicHash::Md5)
                                      .toHex()));
}

void ChecksumTaskTest::recursesIntoAFolder()
{
    // Regression test for the "checksum on a folder just fails" bug: a
    // directory in the input list must be expanded into every file it
    // contains (recursively), not reported as an error.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir root(dir.path());
    QVERIFY(root.mkpath(QStringLiteral("myfolder/sub")));
    writeFile(root.filePath(QStringLiteral("myfolder/top.txt")), QByteArrayLiteral("top"));
    writeFile(root.filePath(QStringLiteral("myfolder/sub/nested.txt")), QByteArrayLiteral("nested"));

    const QString folderPath = root.filePath(QStringLiteral("myfolder"));
    auto cancelFlag = QSharedPointer<QAtomicInt>::create(0);
    auto *task = new ChecksumTask({folderPath}, cancelFlag);
    QSignalSpy finishedSpy(task, &ChecksumTask::finished);
    QVector<ChecksumResult> results;
    connect(task, &ChecksumTask::resultReady, this,
            [&results](const ChecksumResult &r) { results << r; });

    QThreadPool::globalInstance()->start(task);
    QVERIFY(finishedSpy.wait(5000));

    QCOMPARE(results.size(), 2);
    for (const ChecksumResult &r : std::as_const(results))
        QVERIFY(r.ok);

    QStringList displayNames;
    for (const ChecksumResult &r : std::as_const(results))
        displayNames << r.displayName;
    QVERIFY(displayNames.contains(QStringLiteral("myfolder/top.txt")));
    QVERIFY(displayNames.contains(QStringLiteral("myfolder/sub/nested.txt")));
}

QTEST_GUILESS_MAIN(ChecksumTaskTest)
#include "checksumtasktest.moc"
