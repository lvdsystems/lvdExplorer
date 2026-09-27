#pragma once

#include <QPoint>
#include <QTabBar>

class PaneContainer;

// Custom tab bar that replaces QTabBar's built-in "movable tabs" drag with
// one that also works across different PaneContainer instances -- dragging
// a tab from one pane's bar and dropping it on another's moves the whole
// FilePane (and its tab) to the destination pane.
class PaneTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit PaneTabBar(PaneContainer *owner, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    PaneContainer *m_owner = nullptr;
    QPoint m_dragStartPos;
    bool m_dragArmed = false;
};
