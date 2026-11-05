#pragma once

#include "core/session/sessiondata.h"
#include "ui/theme/thememanager.h"

#include <QMainWindow>
#include <QPointer>

class PaneGridWidget;
class FilePane;
class FinderPanel;
class BookmarksPanel;
class QDockWidget;
class QAction;
class QMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupMenus();
    void setupFinder();
    void setupBookmarks();
    void setupStatusBar();
    void openSessionManager();
    void openFolderCompare();
    void setTheme(ThemeManager::Theme theme);
    void trackFocusedPane(QWidget *previous, QWidget *now);
    SessionData captureSession() const;
    void applySession(const SessionData &data);
    void setCurrentSessionName(const QString &name);
    void loadSessionNamed(const QString &name);
    void saveSessionNamed(const QString &name);
    void saveSession();
    void saveSessionAs();
    void rebuildRecentSessionsMenu();

    PaneGridWidget *m_grid = nullptr;
    QDockWidget *m_finderDock = nullptr;
    FinderPanel *m_finderPanel = nullptr;
    QDockWidget *m_bookmarksDock = nullptr;
    BookmarksPanel *m_bookmarksPanel = nullptr;
    QPointer<FilePane> m_lastFocusedPane;
    QPointer<FilePane> m_secondLastFocusedPane;

    QString m_currentSessionName;
    QMenu *m_sessionMenu = nullptr;
    QAction *m_manageSessionsAction = nullptr;
    QList<QAction *> m_recentSessionActions;
};
