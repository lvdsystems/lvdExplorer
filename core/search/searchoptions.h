#pragma once

#include <QString>

struct SearchOptions
{
    enum class Mode { Substring, Regex };

    QString rootPath;
    QString pattern;
    Mode mode = Mode::Substring;
    bool caseSensitive = false;
    bool recursive = true;
};
