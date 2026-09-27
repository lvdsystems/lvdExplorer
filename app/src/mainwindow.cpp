#include "mainwindow.h"

#include "core/session/sessionmanager.h"
#include "ui/bookmarks/bookmarkmanager.h"
#include "ui/bookmarks/bookmarkspanel.h"
#include "ui/dialogs/aboutdialog.h"
#include "ui/dialogs/foldercomparedialog.h"
#include "ui/dialogs/sessionmanagerdialog.h"
#include "ui/dialogs/shortcuteditordialog.h"
#include "ui/finder/finderpanel.h"
#include "ui/pane/filepane.h"
#include "ui/pane/panegridwidget.h"
#include "ui/resources/iconfactory.h"
#include "ui/shortcuts/shortcutmanager.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("lvdExplorer"));
    resize(1200, 800);

    m_grid = new PaneGridWidget(this);
    setCentralWidget(m_grid);

    setupFinder();
    setupBookmarks();
    setupMenus();
    setupStatusBar();

    connect(qApp, &QApplication::focusChanged, this, &MainWindow::trackFocusedPane);

    // Restore the autosaved session on launch unless the user asked for a
    // blank start (--new-session)
    const bool startBlank = QCoreApplication::arguments().contains(QStringLiteral("--new-session"));
    if (!startBlank) {
        SessionData data;
        if (SessionManager::load(SessionManager::kAutosaveSessionName, data))
            applySession(data);
    }

    // Focus hasn't settled anywhere yet at construction time, so the first
    // status-bar text is filled in once the event loop actually starts.
    // Falls back to the first pane in the grid if OS focus tracking hasn't
    // landed on one yet (e.g. nothing has been clicked), so the status bar
    // never sits on the placeholder "Ready" text longer than a frame.
    QTimer::singleShot(0, this, [this] {
        trackFocusedPane(nullptr, QApplication::focusWidget());
        if (!m_lastFocusedPane) {
            if (auto *pane = m_grid->findChild<FilePane *>())
                trackFocusedPane(nullptr, pane);
        }
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    SessionManager::save(SessionManager::kAutosaveSessionName, captureSession());
    QMainWindow::closeEvent(event);
}

void MainWindow::setupFinder()
{
    m_finderPanel = new FinderPanel(this);
    connect(m_finderPanel, &FinderPanel::resultActivated, this, [this](const QString &folder, const QString &file) {
        FilePane *target = m_lastFocusedPane;
        if (!target) {
            // Fall back to whichever pane is currently on top, so the
            // Finder is still useful before the user has clicked into any
            // pane yet.
            target = m_grid->findChild<FilePane *>();
        }
        if (target)
            target->navigateToAndSelect(folder, file);
    });

    m_finderDock = new QDockWidget(tr("Find Files"), this);
    m_finderDock->setObjectName(QStringLiteral("FinderDock"));
    m_finderDock->setWidget(m_finderPanel);
    m_finderDock->setVisible(false);
    addDockWidget(Qt::RightDockWidgetArea, m_finderDock);
}

void MainWindow::setupBookmarks()
{
    m_bookmarksPanel = new BookmarksPanel(this);
    connect(m_bookmarksPanel, &BookmarksPanel::bookmarkActivated, this, [this](const QString &path) {
        FilePane *target = m_lastFocusedPane;
        if (!target)
            target = m_grid->findChild<FilePane *>();
        if (target)
            target->navigateTo(path);
    });

    // Default hidden (per the panel's own doc comment): most users won't
    // have any bookmarks yet on first launch, so this dock would otherwise
    // just be empty chrome.
    m_bookmarksDock = new QDockWidget(tr("Bookmarks"), this);
    m_bookmarksDock->setObjectName(QStringLiteral("BookmarksDock"));
    m_bookmarksDock->setWidget(m_bookmarksPanel);
    m_bookmarksDock->setVisible(false);
    addDockWidget(Qt::LeftDockWidgetArea, m_bookmarksDock);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::trackFocusedPane(QWidget *, QWidget *now)
{
    for (QWidget *w = now; w; w = w->parentWidget()) {
        if (auto *pane = qobject_cast<FilePane *>(w)) {
            if (pane != m_lastFocusedPane) {
                if (m_lastFocusedPane) {
                    m_secondLastFocusedPane = m_lastFocusedPane;
                    disconnect(m_lastFocusedPane, &FilePane::statusMessage, this, nullptr);
                }
                m_lastFocusedPane = pane;
                connect(pane, &FilePane::statusMessage, this,
                        [this](const QString &text) { statusBar()->showMessage(text); });
                statusBar()->showMessage(pane->statusText());
            }
            return;
        }
    }
}

void MainWindow::openFolderCompare()
{
    const QString folderA = m_lastFocusedPane ? m_lastFocusedPane->currentPath() : QString();
    const QString folderB = m_secondLastFocusedPane ? m_secondLastFocusedPane->currentPath() : QString();

    auto *dialog = new FolderCompareDialog(folderA, folderB, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::setTheme(ThemeManager::Theme theme)
{
    ThemeManager::apply(theme);
    ThemeManager::save(theme);
}

void MainWindow::setupMenus()
{
    QAction *findFilesAction = m_finderDock->toggleViewAction();
    findFilesAction->setText(tr("&Find Files..."));
    findFilesAction->setIcon(IconFactory::search());
    ShortcutManager::instance().registerAction(findFilesAction, QStringLiteral("app.findFiles"), tr("Find Files"),
                                                QKeySequence::Find); // Ctrl+F
    connect(findFilesAction, &QAction::toggled, this, [this](bool visible) {
        if (visible)
            m_finderPanel->setInitialRoot(m_lastFocusedPane ? m_lastFocusedPane->currentPath() : QString());
    });

    QAction *bookmarksAction = m_bookmarksDock->toggleViewAction();
    bookmarksAction->setText(tr("&Bookmarks"));
    bookmarksAction->setIcon(IconFactory::bookmark());
    ShortcutManager::instance().registerAction(bookmarksAction, QStringLiteral("app.bookmarks"), tr("Bookmarks"),
                                                QKeySequence(Qt::CTRL | Qt::Key_B));

    QAction *compareFoldersAction = new QAction(IconFactory::compareFolders(), tr("&Compare Folders..."), this);
    connect(compareFoldersAction, &QAction::triggered, this, &MainWindow::openFolderCompare);

    QAction *manageSessions = new QAction(IconFactory::sessions(), tr("&Sessions..."), this);
    ShortcutManager::instance().registerAction(manageSessions, QStringLiteral("app.sessions"), tr("Sessions"),
                                                QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    connect(manageSessions, &QAction::triggered, this, &MainWindow::openSessionManager);

    QAction *shortcutsAction = new QAction(tr("&Keyboard Shortcuts..."), this);
    connect(shortcutsAction, &QAction::triggered, this, [this] {
        auto *dialog = new ShortcutEditorDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });

    // Pane layout actions are created here (before the toolbar) so the
    // *same* QAction instances can be added to both the toolbar and the
    // View menu -- one flips, the other follows, with no separate
    // checked-state bookkeeping needed.
    QAction *onePane = new QAction(IconFactory::layoutOnePane(), tr("&1 Pane"), this);
    QAction *twoPanesSide = new QAction(IconFactory::layoutTwoPanesHorizontal(), tr("&2 Panes (Commander)"), this);
    QAction *twoPanesStacked = new QAction(IconFactory::layoutTwoPanesVertical(), tr("2 Panes (&Stacked)"), this);
    QAction *fourPanes = new QAction(IconFactory::layoutFourPanes(), tr("&4 Panes"), this);

    ShortcutManager::instance().registerAction(onePane, QStringLiteral("view.onePane"), tr("View: 1 Pane"),
                                                QKeySequence(Qt::CTRL | Qt::Key_1));
    ShortcutManager::instance().registerAction(twoPanesSide, QStringLiteral("view.twoPanesCommander"),
                                                tr("View: 2 Panes (Commander)"), QKeySequence(Qt::CTRL | Qt::Key_2));
    ShortcutManager::instance().registerAction(twoPanesStacked, QStringLiteral("view.twoPanesStacked"),
                                                tr("View: 2 Panes (Stacked)"), QKeySequence(Qt::CTRL | Qt::Key_3));
    ShortcutManager::instance().registerAction(fourPanes, QStringLiteral("view.fourPanes"), tr("View: 4 Panes"),
                                                QKeySequence(Qt::CTRL | Qt::Key_4));

    connect(onePane, &QAction::triggered, this,
            [this] { m_grid->setLayoutMode(GridLayoutMode::OnePane); });
    connect(twoPanesSide, &QAction::triggered, this,
            [this] { m_grid->setLayoutMode(GridLayoutMode::TwoPanesHorizontal); });
    connect(twoPanesStacked, &QAction::triggered, this,
            [this] { m_grid->setLayoutMode(GridLayoutMode::TwoPanesVertical); });
    connect(fourPanes, &QAction::triggered, this,
            [this] { m_grid->setLayoutMode(GridLayoutMode::FourPanes); });

    auto *toolBar = addToolBar(tr("Main"));
    toolBar->setObjectName(QStringLiteral("MainToolBar"));
    toolBar->setIconSize(QSize(18, 18));
    // Quick layout switcher: 1 / 2 (Commander-style side-by-side) / 4 --
    // the most frequently used three, one click away. 2-stacked stays a
    // View-menu-only option rather than crowding the toolbar further.
    toolBar->addAction(onePane);
    toolBar->addAction(twoPanesSide);
    toolBar->addAction(fourPanes);
    toolBar->addSeparator();
    toolBar->addAction(findFilesAction);
    toolBar->addAction(bookmarksAction);
    toolBar->addAction(compareFoldersAction);
    toolBar->addAction(manageSessions);

    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(findFilesAction);
    toolsMenu->addAction(compareFoldersAction);
    toolsMenu->addSeparator();
    toolsMenu->addAction(shortcutsAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(onePane);
    viewMenu->addAction(twoPanesSide);
    viewMenu->addAction(twoPanesStacked);
    viewMenu->addAction(fourPanes);
    viewMenu->addSeparator();
    viewMenu->addAction(bookmarksAction);

    QMenu *themeMenu = viewMenu->addMenu(tr("&Theme"));
    auto *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);

    QAction *systemTheme = themeMenu->addAction(tr("&System Default"));
    QAction *lightTheme = themeMenu->addAction(tr("&Light"));
    QAction *darkTheme = themeMenu->addAction(tr("&Dark"));
    for (QAction *action : {systemTheme, lightTheme, darkTheme}) {
        action->setCheckable(true);
        themeGroup->addAction(action);
    }

    connect(systemTheme, &QAction::triggered, this, [this] { setTheme(ThemeManager::Theme::System); });
    connect(lightTheme, &QAction::triggered, this, [this] { setTheme(ThemeManager::Theme::Light); });
    connect(darkTheme, &QAction::triggered, this, [this] { setTheme(ThemeManager::Theme::Dark); });

    switch (ThemeManager::loadSaved()) {
    case ThemeManager::Theme::Light:
        lightTheme->setChecked(true);
        break;
    case ThemeManager::Theme::Dark:
        darkTheme->setChecked(true);
        break;
    case ThemeManager::Theme::System:
    default:
        systemTheme->setChecked(true);
        break;
    }

    QMenu *sessionMenu = menuBar()->addMenu(tr("&Session"));
    sessionMenu->addAction(manageSessions);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAction = helpMenu->addAction(tr("&About lvdExplorer..."));
    connect(aboutAction, &QAction::triggered, this, [this] {
        AboutDialog dialog(this);
        dialog.exec();
    });
}

void MainWindow::openSessionManager()
{
    auto *dialog = new SessionManagerDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &SessionManagerDialog::saveRequested, this, [this](const QString &name) {
        if (!SessionManager::save(name, captureSession()))
            QMessageBox::warning(this, tr("lvdExplorer"), tr("Could not save session “%1”.").arg(name));
    });
    connect(dialog, &SessionManagerDialog::loadRequested, this, [this](const QString &name) {
        SessionData data;
        if (SessionManager::load(name, data))
            applySession(data);
        else
            QMessageBox::warning(this, tr("lvdExplorer"), tr("Could not load session “%1”.").arg(name));
    });

    dialog->exec();
}

SessionData MainWindow::captureSession() const
{
    SessionData data;
    data.window.geometry = saveGeometry();
    data.window.state = saveState();
    m_grid->captureInto(data);
    return data;
}

void MainWindow::applySession(const SessionData &data)
{
    if (!data.window.geometry.isEmpty())
        restoreGeometry(data.window.geometry);
    if (!data.window.state.isEmpty())
        restoreState(data.window.state);
    m_grid->applyFrom(data);
}
