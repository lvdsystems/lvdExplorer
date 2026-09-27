#include "ui/bookmarks/bookmarkspanel.h"

#include "ui/bookmarks/bookmarkmanager.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QListWidget>
#include <QVBoxLayout>

BookmarksPanel::BookmarksPanel(QWidget *parent)
    : QWidget(parent)
{
    m_list = new QListWidget(this);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(m_list);

    connect(m_list, &QListWidget::itemActivated, this, &BookmarksPanel::handleActivated);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &BookmarksPanel::showContextMenu);
    connect(&BookmarkManager::instance(), &BookmarkManager::changed, this, &BookmarksPanel::refresh);

    refresh();
}

void BookmarksPanel::refresh()
{
    m_list->clear();
    for (const QString &path : BookmarkManager::instance().bookmarks()) {
        const QString native = QDir::toNativeSeparators(path);
        const QString name = QFileInfo(path).fileName();
        auto *item = new QListWidgetItem(name.isEmpty() ? native : name, m_list);
        item->setData(Qt::UserRole, path);
        item->setToolTip(native);
    }
}

void BookmarksPanel::handleActivated(QListWidgetItem *item)
{
    if (!item)
        return;
    emit bookmarkActivated(item->data(Qt::UserRole).toString());
}

void BookmarksPanel::removeSelected()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return;
    BookmarkManager::instance().remove(item->data(Qt::UserRole).toString());
}

void BookmarksPanel::showContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_list->itemAt(pos);
    if (!item)
        return;
    m_list->setCurrentItem(item);

    QMenu menu(this);
    QAction *removeAction = menu.addAction(tr("Remove Bookmark"));
    if (menu.exec(m_list->mapToGlobal(pos)) == removeAction)
        removeSelected();
}
