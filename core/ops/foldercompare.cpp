#include "core/ops/foldercompare.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>

QVector<FolderCompareEntry> compareFolders(const QString &folderA, const QString &folderB)
{
    QHash<QString, FolderCompareEntry> byName;

    const QDir dirA(folderA);
    for (const QFileInfo &info : dirA.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        FolderCompareEntry &entry = byName[info.fileName()];
        entry.name = info.fileName();
        entry.inA = true;
        entry.isDir = info.isDir();
        entry.sizeA = info.isDir() ? -1 : info.size();
        entry.modifiedA = info.lastModified();
    }

    const QDir dirB(folderB);
    for (const QFileInfo &info : dirB.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        FolderCompareEntry &entry = byName[info.fileName()];
        entry.name = info.fileName();
        entry.inB = true;
        entry.isDir = entry.isDir || info.isDir();
        entry.sizeB = info.isDir() ? -1 : info.size();
        entry.modifiedB = info.lastModified();
    }

    QVector<FolderCompareEntry> result;
    result.reserve(byName.size());
    for (const auto &entry : std::as_const(byName))
        result.append(entry);

    std::sort(result.begin(), result.end(), [](const FolderCompareEntry &a, const FolderCompareEntry &b) {
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });

    return result;
}
