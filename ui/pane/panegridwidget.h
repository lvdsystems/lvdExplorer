#pragma once

#include "core/session/sessiondata.h"

#include <QList>
#include <QWidget>

class QVBoxLayout;
class PaneContainer;

enum class GridLayoutMode { OnePane, TwoPanesHorizontal, TwoPanesVertical, FourPanes };

// Arranges 1, 2, or 4 PaneContainers via nested QSplitters. Switching
// layout mode grows/shrinks the container list; containers dropped by a
// shrink are destroyed (their tabs/history are not preserved) 
class PaneGridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PaneGridWidget(QWidget *parent = nullptr);

    void setLayoutMode(GridLayoutMode mode);
    GridLayoutMode layoutMode() const { return m_mode; }

    // session save/restore Populates/reads only
    // the paneLayout and panes fields of \p data -- window geometry/state
    // is MainWindow's own concern.
    void captureInto(SessionData &data) const;
    void applyFrom(const SessionData &data);

private:
    static int requiredCount(GridLayoutMode mode);
    static PaneContainer *createContainer();
    void rebuild();

    GridLayoutMode m_mode = GridLayoutMode::OnePane;
    QList<PaneContainer *> m_containers;
    QWidget *m_currentRoot = nullptr;
    QVBoxLayout *m_rootLayout = nullptr;
};
