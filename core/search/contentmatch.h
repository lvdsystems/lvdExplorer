#pragma once

#include <QString>

// One matching line from ContentSearchTask's grep-style scan. Unlike
// SearchTask's FileEntry, a single file can contribute several of these.
struct ContentMatch
{
    QString filePath;
    int lineNumber = 0;
    QString lineText;
};
