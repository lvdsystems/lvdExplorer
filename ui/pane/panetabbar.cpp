#include "ui/pane/panetabbar.h"

#include "ui/pane/panecontainer.h"

#include <QApplication>
#include <QDataStream>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>

namespace {
// Same-process-only: carries a pointer back to the source PaneContainer so
// the drop handler knows which pane (and tab index) to pull the FilePane
// out of. Never meaningful outside this app, same convention as
// FileTreeView's source-pane marker.
const char *kTabMimeType = "application/x-lvdexplorer-tab";
}

PaneTabBar::PaneTabBar(PaneContainer *owner, QWidget *parent)
    : QTabBar(parent)
    , m_owner(owner)
{
    setAcceptDrops(true);
}

void PaneTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->position().toPoint();
        m_dragArmed = true;
    }
    QTabBar::mousePressEvent(event);
}

void PaneTabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragArmed || !(event->buttons() & Qt::LeftButton)) {
        QTabBar::mouseMoveEvent(event);
        return;
    }

    if ((event->position().toPoint() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
        QTabBar::mouseMoveEvent(event);
        return;
    }

    m_dragArmed = false;

    const int index = tabAt(m_dragStartPos);
    if (index < 0)
        return;

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream << reinterpret_cast<quintptr>(m_owner) << index;

    auto *mimeData = new QMimeData;
    mimeData->setData(QString::fromLatin1(kTabMimeType), payload);

    auto *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->exec(Qt::MoveAction);
}

void PaneTabBar::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(kTabMimeType)))
        event->acceptProposedAction();
}

void PaneTabBar::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(kTabMimeType)))
        event->acceptProposedAction();
}

void PaneTabBar::dropEvent(QDropEvent *event)
{
    const QByteArray payload = event->mimeData()->data(QString::fromLatin1(kTabMimeType));
    if (payload.size() < static_cast<int>(sizeof(quintptr) + sizeof(int))) {
        event->ignore();
        return;
    }

    QDataStream stream(payload);
    quintptr ownerPtr = 0;
    int sourceIndex = -1;
    stream >> ownerPtr >> sourceIndex;

    auto *sourceContainer = reinterpret_cast<PaneContainer *>(ownerPtr);
    if (!sourceContainer) {
        event->ignore();
        return;
    }

    const int dropIndex = tabAt(event->position().toPoint());
    m_owner->receiveTab(sourceContainer, sourceIndex, dropIndex);
    event->acceptProposedAction();
}
