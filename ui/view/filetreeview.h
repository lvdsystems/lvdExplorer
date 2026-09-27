#pragma once

#include <QTreeView>

class FilePane;

// QTreeView subclass that implements file drag & drop between panes.
// Drag source and drop target are always different FileSystemModel
// instances (one per pane), so this is handled at the view level with
// plain QMimeData urls rather than through QAbstractItemModel's
// mimeData()/dropMimeData() (which assumes reordering within one model).
class FileTreeView : public QTreeView
{
    Q_OBJECT

public:
    explicit FileTreeView(QWidget *parent = nullptr);

    void setOwnerPane(FilePane *pane) { m_ownerPane = pane; }

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    FilePane *m_ownerPane = nullptr;
};
