#pragma once

#include <QStringList>

// Thin wrapper around the system clipboard for file Cut/Copy/Paste. Uses
// real QMimeData file URLs (rather than an app-private buffer) so copying
// in lvdExplorer and pasting into Explorer/Nautilus -- or the reverse --
// works like any other file manager. Cut-vs-copy provenance for content
// lvdExplorer itself placed on the clipboard is also tracked in-process
// (see fileclipboard.cpp), since the OS clipboard has no generic concept
// of "was this pane read-only when it was cut".
namespace FileClipboard {

struct Contents
{
    QStringList paths;
    bool cut = false;
    bool sourceReadOnly = false;

    bool isEmpty() const { return paths.isEmpty(); }
};

// sourceReadOnly should be the read-only state of the pane the paths were
// cut/copied from -- OpsEngine::submit() is what actually enforces §10 at
// paste time, using whatever get() later reports.
void set(const QStringList &paths, bool cut, bool sourceReadOnly);

Contents get();

} // namespace FileClipboard
