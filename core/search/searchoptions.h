#pragma once

#include <QString>

struct SearchOptions
{
    // Wildcard: "*" (zero or more chars) and "?" (exactly one char),
    // matched against the whole file name -- same semantics as Explorer's
    // own filename wildcards (e.g. "*.txt"), via
    // QRegularExpression::wildcardToRegularExpression().
    enum class Mode { Substring, Regex, Wildcard };

    QString rootPath;
    QString pattern;
    Mode mode = Mode::Substring;
    bool caseSensitive = false;
    bool recursive = true;
};
