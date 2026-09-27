#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

// Plain data captured from/applied to the UI tree (MainWindow -> grid ->
// PaneContainer -> FilePane). Kept free of any UI dependency so it stays
// in core/ and is trivially (de)serializable

struct TabSessionState
{
    QString path;
    // QHeaderView::saveState()/restoreState() blob: covers column widths,
    // order, visibility, *and* the sort indicator (section + direction) in
    // one round-trippable value, so there's no separate sortColumn/Order.
    QByteArray headerState;
    bool readOnly = false;
};

struct PaneSessionState
{
    int activeTab = 0;
    QVector<TabSessionState> tabs;
};

struct WindowSessionState
{
    QByteArray geometry;
    QByteArray state;
};

struct SessionData
{
    WindowSessionState window;
    QString paneLayout; // see GridLayoutMode <-> string mapping in panegridwidget.cpp
    QVector<PaneSessionState> panes;
};
