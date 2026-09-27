#pragma once

#include <QWidget>

class QListWidget;
class QListWidgetItem;

// The dockable Bookmarks panel: a flat, persisted list of favorite
// folders. Default hidden -- opened via the View menu/toolbar star icon,
// or automatically the first time a folder is bookmarked, same
// discoverability pattern as the Finder dock.
class BookmarksPanel : public QWidget
{
    Q_OBJECT

public:
    explicit BookmarksPanel(QWidget *parent = nullptr);

signals:
    void bookmarkActivated(const QString &path);

private slots:
    void refresh();
    void removeSelected();
    void handleActivated(QListWidgetItem *item);
    void showContextMenu(const QPoint &pos);

private:
    QListWidget *m_list = nullptr;
};
