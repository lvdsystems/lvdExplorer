#include "ui/clipboard/fileclipboard.h"

#include "platform/platformshell.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>

namespace {
// Remembers what lvdExplorer itself last placed on the clipboard, so get()
// can tell "this is still what we cut/copied" (use the remembered
// sourceReadOnly) apart from "the clipboard now holds something else --
// pasted from outside, or copied elsewhere in lvdExplorer since" (fall
// back to false, the same default already used for an external
// drag-and-drop with no source pane, per FilePane::handleFilesDropped).
QStringList g_lastPaths;
bool g_lastCut = false;
bool g_lastSourceReadOnly = false;
} // namespace

namespace FileClipboard {

void set(const QStringList &paths, bool cut, bool sourceReadOnly)
{
    if (paths.isEmpty())
        return;

    auto *mimeData = new QMimeData;
    QList<QUrl> urls;
    urls.reserve(paths.size());
    for (const QString &path : paths)
        urls.append(QUrl::fromLocalFile(path));
    mimeData->setUrls(urls);

    PlatformShell::instance().markCutMimeData(mimeData, cut);
    QApplication::clipboard()->setMimeData(mimeData);

    g_lastPaths = paths;
    g_lastCut = cut;
    g_lastSourceReadOnly = sourceReadOnly;
}

Contents get()
{
    Contents contents;

    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    if (!mimeData || !mimeData->hasUrls())
        return contents;

    for (const QUrl &url : mimeData->urls()) {
        if (url.isLocalFile())
            contents.paths.append(url.toLocalFile());
    }
    if (contents.paths.isEmpty())
        return contents;

    contents.cut = PlatformShell::instance().isCutMimeData(mimeData);
    contents.sourceReadOnly =
        (contents.paths == g_lastPaths && contents.cut == g_lastCut) ? g_lastSourceReadOnly : false;
    return contents;
}

} // namespace FileClipboard
