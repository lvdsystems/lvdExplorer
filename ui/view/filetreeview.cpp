#include "ui/view/filetreeview.h"

#include "ui/pane/filepane.h"

#include <QDataStream>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

namespace {
// Same-process-only marker used to hand the drop handler a pointer back to
// the FilePane a drag originated from, so it knows which pane's model to
// refresh after a move. Never crosses process/window-manager boundaries,
// so it's meaningless (and harmlessly ignored) for drags from outside the
// app.
const char *kSourcePaneMimeType = "application/x-lvdexplorer-source-pane";
}

FileTreeView::FileTreeView(QWidget *parent)
    : QTreeView(parent)
{
}

void FileTreeView::startDrag(Qt::DropActions supportedActions)
{
    if (!m_ownerPane)
        return;

    const QStringList paths = m_ownerPane->selectedPaths();
    if (paths.isEmpty())
        return;

    QList<QUrl> urls;
    urls.reserve(paths.size());
    for (const QString &path : paths)
        urls.append(QUrl::fromLocalFile(path));

    auto *mimeData = new QMimeData;
    mimeData->setUrls(urls);

    QByteArray ownerPayload;
    QDataStream stream(&ownerPayload, QIODevice::WriteOnly);
    stream << reinterpret_cast<quintptr>(m_ownerPane);
    mimeData->setData(QString::fromLatin1(kSourcePaneMimeType), ownerPayload);

    auto *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->exec(supportedActions, Qt::MoveAction);
}

void FileTreeView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void FileTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void FileTreeView::dropEvent(QDropEvent *event)
{
    if (!m_ownerPane || !event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    FilePane *sourcePane = nullptr;
    const QByteArray ownerPayload = event->mimeData()->data(QString::fromLatin1(kSourcePaneMimeType));
    if (ownerPayload.size() == sizeof(quintptr)) {
        QDataStream stream(ownerPayload);
        quintptr ptrValue = 0;
        stream >> ptrValue;
        sourcePane = reinterpret_cast<FilePane *>(ptrValue);
    }

    QString destDir = m_ownerPane->currentPath();
    const QModelIndex hoverIndex = indexAt(event->position().toPoint());
    if (hoverIndex.isValid() && m_ownerPane->isDirIndex(hoverIndex))
        destDir = m_ownerPane->pathForIndex(hoverIndex);

    QStringList sourcePaths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            sourcePaths.append(url.toLocalFile());
    }

    const bool forceCopy = event->modifiers().testFlag(Qt::ControlModifier);
    const bool forceMove = event->modifiers().testFlag(Qt::ShiftModifier);

    m_ownerPane->handleFilesDropped(sourcePaths, destDir, sourcePane, forceCopy, forceMove);
    event->acceptProposedAction();
}
