#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

// Top-level (non-recursive) comparison of two folders by name/size/date,
struct FolderCompareEntry
{
    QString name;
    bool inA = false;
    bool inB = false;
    bool isDir = false;
    qint64 sizeA = -1;
    qint64 sizeB = -1;
    QDateTime modifiedA;
    QDateTime modifiedB;

    bool onlyInA() const { return inA && !inB; }
    bool onlyInB() const { return inB && !inA; }
    bool differs() const { return inA && inB && (sizeA != sizeB || modifiedA != modifiedB); }
};

QVector<FolderCompareEntry> compareFolders(const QString &folderA, const QString &folderB);
