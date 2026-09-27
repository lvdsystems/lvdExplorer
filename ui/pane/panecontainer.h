#pragma once

#include "core/session/sessiondata.h"

#include <QTabWidget>

class FilePane;

// One grid cell: a tab bar of independent FilePane instances, each with
// its own history/path/read-only state. PaneGridWidget arranges one or
// more of these into the 1/2/4-pane layouts.
class PaneContainer : public QTabWidget
{
    Q_OBJECT

public:
    explicit PaneContainer(QWidget *parent = nullptr);

    // Called by PaneTabBar (its own tab bar or another pane's) when a tab
    // drag lands here. sourceIndex is the dragged tab's index within
    // \p source; destIndex is where it was dropped in this container's bar
    // (-1 meaning "at the end").
    void receiveTab(PaneContainer *source, int sourceIndex, int destIndex);

    PaneSessionState captureState() const;
    void applyState(const PaneSessionState &state);

private slots:
    void addNewTab();
    void closeTab(int index);

private:
    FilePane *createPane(const QString &initialPath);
    void updateTabLabel(FilePane *pane);
};
