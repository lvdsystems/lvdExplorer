#include "ui/pane/filepane.h"

#include "core/archive/archiveextracttask.h"
#include "core/archive/archivereader.h"
#include "core/fsmodel/drivelisttask.h"
#include "core/fsmodel/filesystemmodel.h"
#include "core/ops/fileophandle.h"
#include "core/ops/fileoprequest.h"
#include "core/ops/opsengine.h"
#include "platform/platformshell.h"
#include "ui/bookmarks/bookmarkmanager.h"
#include "ui/clipboard/fileclipboard.h"
#include "ui/dialogs/batchrenamedialog.h"
#include "ui/dialogs/checksumdialog.h"
#include "ui/dialogs/progressdialog.h"
#include "ui/resources/iconfactory.h"
#include "ui/shortcuts/shortcutmanager.h"
#include "ui/view/filetreeview.h"
#include "ui/widgets/breadcrumbbar.h"

#include <QAbstractItemDelegate>
#include <QAction>
#include <QCheckBox>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QStorageInfo>
#include <QStyle>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QToolBar>
#include <QUuid>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Entries opened or copied out of an archive are extracted here. The temp
// root is removed when the process exits.
QString archiveTempRoot()
{
    static QTemporaryDir root;
    return root.isValid() ? root.path() : QDir::tempPath();
}

QString newArchiveTempFolder()
{
    const QString folder = QDir(archiveTempRoot()).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QDir().mkpath(folder);
    return folder;
}

} // namespace

FilePane::FilePane(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);

    m_model = new FileSystemModel(this);

    m_view = new FileTreeView(this);
    m_view->setOwnerPane(this);
    m_view->setModel(m_model);
    m_view->setRootIsDecorated(false);
    m_view->setUniformRowHeights(true);
    m_view->setAlternatingRowColors(true);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(FileSystemModel::NameColumn, Qt::AscendingOrder);
    m_view->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    // Every column user-resizable (Stretch forbids manual resizing, and
    // QTreeView defaults to stretching the last section -- both disabled
    // so Name/Size/Type/Date modified all behave the same way).
    m_view->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_view->header()->setStretchLastSection(false);
    m_view->header()->resizeSection(FileSystemModel::NameColumn, 240);
    m_view->header()->resizeSection(FileSystemModel::SizeColumn, 90);
    m_view->header()->resizeSection(FileSystemModel::TypeColumn, 110);
    m_view->header()->resizeSection(FileSystemModel::ModifiedColumn, 150);
    m_view->setDragEnabled(true);
    m_view->setAcceptDrops(true);
    m_view->setDropIndicatorShown(true);
    m_view->setDragDropMode(QAbstractItemView::DragDrop);

    // Breadcrumb and filter share one row --
    // they were two separate full-width rows before, which was one row of
    // chrome more than this pane needed.
    m_breadcrumb = new BreadcrumbBar(this);

    auto *addressBar = new QWidget(this);
    auto *addressLayout = new QHBoxLayout(addressBar);
    addressLayout->setContentsMargins(4, 2, 4, 2);
    m_filterEdit = new QLineEdit(addressBar);
    m_filterEdit->setPlaceholderText(tr("Filter this folder…"));
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setMaximumWidth(220);
    m_filterRegexCheck = new QCheckBox(tr("Regex"), addressBar);
    m_filterRegexCheck->setToolTip(tr("Treat the filter text as a regular expression instead of plain substring."));
    addressLayout->addWidget(m_breadcrumb, 1);
    addressLayout->addWidget(m_filterEdit);
    addressLayout->addWidget(m_filterRegexCheck);

    auto *toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(16, 16));
    m_backAction = toolBar->addAction(style()->standardIcon(QStyle::SP_ArrowBack), tr("Back"));
    m_forwardAction = toolBar->addAction(style()->standardIcon(QStyle::SP_ArrowForward), tr("Forward"));
    m_upAction = toolBar->addAction(style()->standardIcon(QStyle::SP_ArrowUp), tr("Up"));
    m_refreshAction = toolBar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("Refresh"));

    // WidgetWithChildrenShortcut (rather than the default WindowShortcut)
    // scopes each of these to the pane that owns them: with 2+ panes open
    // in the same window, WindowShortcut would register the *same* Alt+Left
    // etc. on every pane's action simultaneously, and Qt treats that as an
    // ambiguous shortcut (fires on none of them). Associating the action
    // with both the toolbar and the view means it fires whichever of the
    // two currently has focus.
    for (QAction *navAction : {m_backAction, m_forwardAction, m_upAction, m_refreshAction}) {
        navAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        m_view->addAction(navAction);
    }

    ShortcutManager::instance().registerAction(m_backAction, QStringLiteral("pane.back"), tr("Pane: Back"),
                                                QKeySequence(Qt::ALT | Qt::Key_Left));
    ShortcutManager::instance().registerAction(m_forwardAction, QStringLiteral("pane.forward"), tr("Pane: Forward"),
                                                QKeySequence(Qt::ALT | Qt::Key_Right));
    ShortcutManager::instance().registerAction(m_upAction, QStringLiteral("pane.up"), tr("Pane: Up"),
                                                QKeySequence(Qt::ALT | Qt::Key_Up));
    ShortcutManager::instance().registerAction(m_refreshAction, QStringLiteral("pane.refresh"), tr("Pane: Refresh"),
                                                QKeySequence::Refresh);

    connect(m_backAction, &QAction::triggered, this, &FilePane::goBack);
    connect(m_forwardAction, &QAction::triggered, this, &FilePane::goForward);
    connect(m_upAction, &QAction::triggered, this, &FilePane::goUp);
    connect(m_refreshAction, &QAction::triggered, this, &FilePane::refresh);

    auto *backspaceUpAction = new QAction(tr("Up"), m_view);
    backspaceUpAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    ShortcutManager::instance().registerAction(backspaceUpAction, QStringLiteral("pane.upBackspace"),
                                                tr("Pane: Up (Backspace)"), QKeySequence(Qt::Key_Backspace));
    m_view->addAction(backspaceUpAction);
    connect(backspaceUpAction, &QAction::triggered, this, &FilePane::goUp);

    toolBar->addSeparator();
    auto *newFolderAction = toolBar->addAction(IconFactory::newFolder(), tr("New Folder"));
    newFolderAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_view->addAction(newFolderAction);
    ShortcutManager::instance().registerAction(newFolderAction, QStringLiteral("pane.newFolder"),
                                                tr("Pane: New Folder"),
                                                QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(newFolderAction, &QAction::triggered, this, &FilePane::createNewFolder);

    // Minimal "volume list" quick-access: a real
    // favorites/drives sidebar tree isn't built yet, so this is a
    // deliberately small MVP rather than that fuller planned feature.
    auto *drivesButton = new QToolButton(toolBar);
    drivesButton->setIcon(IconFactory::drives());
    drivesButton->setText(tr("Drives"));
    drivesButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    drivesButton->setToolTip(tr("Jump to a drive or volume"));
    drivesButton->setPopupMode(QToolButton::InstantPopup);
    auto *drivesMenu = new QMenu(drivesButton);
    connect(drivesMenu, &QMenu::aboutToShow, this, [this, drivesMenu] {
        // Populated asynchronously via DriveListTask: QStorageInfo's own
        // isReady()/displayName() calls can block for a long network
        // timeout when a mapped drive points at a currently-unreachable
        // share, which used to freeze the whole app the moment this menu
        // was opened. Shows a placeholder immediately and fills in the
        // real list once the background scan reports back instead.
        drivesMenu->clear();
        QAction *loadingAction = drivesMenu->addAction(tr("Loading…"));
        loadingAction->setEnabled(false);

        auto *task = new DriveListTask;
        connect(task, &DriveListTask::finished, this, [this, drivesMenu](const QVector<DriveEntry> &drives) {
            drivesMenu->clear();
            if (drives.isEmpty()) {
                drivesMenu->addAction(tr("No drives found"))->setEnabled(false);
                return;
            }
            for (const DriveEntry &drive : drives) {
                QAction *action = drivesMenu->addAction(drive.label);
                connect(action, &QAction::triggered, this, [this, path = drive.rootPath] { navigateTo(path); });
            }
        });
        QThreadPool::globalInstance()->start(task);
    });
    drivesButton->setMenu(drivesMenu);
    toolBar->addWidget(drivesButton);

    toolBar->addSeparator();
    m_readOnlyCheck = new QCheckBox(tr("Read-only"), toolBar);
    m_readOnlyCheck->setToolTip(
        tr("Blocks delete, rename, and move operations in this pane (§10). Copying into it is still allowed."));
    connect(m_readOnlyCheck, &QCheckBox::toggled, this, &FilePane::setReadOnly);
    toolBar->addWidget(m_readOnlyCheck);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toolBar);
    layout->addWidget(addressBar);
    layout->addWidget(m_view, 1);

    connect(m_filterEdit, &QLineEdit::textChanged, m_model, &FileSystemModel::setFilterText);
    connect(m_filterEdit, &QLineEdit::textChanged, this, [this] { updateStatusText(); });
    connect(m_filterRegexCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_model->setFilterMode(checked ? FileSystemModel::FilterMode::Regex : FileSystemModel::FilterMode::Substring);
    });

    connect(m_view, &FileTreeView::activated, this, &FilePane::onActivated);
    connect(m_view, &FileTreeView::customContextMenuRequested, this, &FilePane::showContextMenu);
    connect(m_breadcrumb, &BreadcrumbBar::pathActivated, this, &FilePane::onBreadcrumbPathActivated);
    connect(m_model, &FileSystemModel::errorOccurred, this, [this](const QString &message) {
        QMessageBox::warning(this, tr("lvdExplorer"), message);
    });

    connect(m_view, &FileTreeView::editStarted, this, [this] { m_model->setAutoRefreshSuspended(true); });
    connect(m_view->itemDelegate(), &QAbstractItemDelegate::closeEditor, this, &FilePane::onEditorClosed);
    connect(m_model, &FileSystemModel::scanAboutToStart, this, &FilePane::rememberSelection);
    connect(m_model, &FileSystemModel::scanFinished, this, &FilePane::restoreSelection);

    auto *deleteAction = new QAction(tr("Delete"), m_view);
    deleteAction->setShortcutContext(Qt::WidgetShortcut);
    ShortcutManager::instance().registerAction(deleteAction, QStringLiteral("pane.delete"), tr("Pane: Delete"),
                                                QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, &FilePane::deleteSelected);
    m_view->addAction(deleteAction);

    auto *renameAction = new QAction(tr("Rename"), m_view);
    renameAction->setShortcutContext(Qt::WidgetShortcut);
    ShortcutManager::instance().registerAction(renameAction, QStringLiteral("pane.rename"), tr("Pane: Rename"),
                                                QKeySequence(Qt::Key_F2));
    connect(renameAction, &QAction::triggered, this, &FilePane::renameSelected);
    m_view->addAction(renameAction);

    // Cut/Copy don't need a read-only check here: they only stage the
    // system clipboard. The eventual Move (a Cut's paste) is what
    // OpsEngine::submit() actually blocks at paste time if the source was
    // read-only (§10) -- which also correctly covers pasting somewhere
    // else entirely, including another window.
    m_cutAction = new QAction(tr("Cut"), m_view);
    m_cutAction->setShortcutContext(Qt::WidgetShortcut);
    ShortcutManager::instance().registerAction(m_cutAction, QStringLiteral("pane.cut"), tr("Pane: Cut"),
                                                QKeySequence::Cut);
    connect(m_cutAction, &QAction::triggered, this, &FilePane::cutSelected);
    m_view->addAction(m_cutAction);

    m_copyAction = new QAction(tr("Copy"), m_view);
    m_copyAction->setShortcutContext(Qt::WidgetShortcut);
    ShortcutManager::instance().registerAction(m_copyAction, QStringLiteral("pane.copy"), tr("Pane: Copy"),
                                                QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, this, &FilePane::copySelected);
    m_view->addAction(m_copyAction);

    m_pasteAction = new QAction(tr("Paste"), m_view);
    m_pasteAction->setShortcutContext(Qt::WidgetShortcut);
    ShortcutManager::instance().registerAction(m_pasteAction, QStringLiteral("pane.paste"), tr("Pane: Paste"),
                                                QKeySequence::Paste);
    connect(m_pasteAction, &QAction::triggered, this, &FilePane::pasteClipboard);
    m_view->addAction(m_pasteAction);

    connect(m_model, &FileSystemModel::scanFinished, this, [this](int, int) { updateStatusText(); });
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { updateStatusText(); });
    updateStatusText();
}

void FilePane::navigateTo(const QString &path)
{
    const QString cleaned = QDir::cleanPath(path);
    const bool reachable = QFileInfo::exists(cleaned) || ArchiveReader::splitArchivePath(cleaned, nullptr, nullptr);
    if (cleaned.isEmpty() || !reachable) {
        QMessageBox::warning(this, tr("lvdExplorer"), tr("“%1” does not exist.").arg(path));
        return;
    }

    if (cleaned == m_currentPath)
        return;

    navigateInternal(cleaned);
    pushHistory(cleaned);
}

void FilePane::navigateInternal(const QString &path)
{
    m_currentPath = path;
    m_inArchive = ArchiveReader::splitArchivePath(path, nullptr, nullptr);
    m_model->setReadOnly(changesBlocked());
    m_model->setRootPath(path);
    m_breadcrumb->setPath(path);
    updateNavigationActions();
    emit pathChanged(path);
}

void FilePane::pushHistory(const QString &path)
{
    if (m_historyIndex >= 0 && m_historyIndex < m_history.size() - 1)
        m_history.erase(m_history.begin() + m_historyIndex + 1, m_history.end());

    if (m_history.isEmpty() || m_history.last() != path)
        m_history.append(path);

    m_historyIndex = m_history.size() - 1;
    updateNavigationActions();
}

void FilePane::goBack()
{
    if (m_historyIndex <= 0)
        return;
    --m_historyIndex;
    navigateInternal(m_history.at(m_historyIndex));
}

void FilePane::goForward()
{
    if (m_historyIndex < 0 || m_historyIndex >= m_history.size() - 1)
        return;
    ++m_historyIndex;
    navigateInternal(m_history.at(m_historyIndex));
}

void FilePane::goUp()
{
    if (m_inArchive) {
        QString archive;
        QString inner;
        ArchiveReader::splitArchivePath(m_currentPath, &archive, &inner);
        if (inner.isEmpty()) {
            navigateToAndSelect(QFileInfo(archive).absolutePath(), archive);
            return;
        }
        const int slash = inner.lastIndexOf(QLatin1Char('/'));
        navigateTo(slash < 0 ? archive : archive + QLatin1Char('/') + inner.left(slash));
        return;
    }

    QDir dir(m_currentPath);
    if (!dir.cdUp())
        return;
    navigateTo(dir.absolutePath());
}

void FilePane::refresh()
{
    m_model->refresh();
}

void FilePane::updateNavigationActions()
{
    m_backAction->setEnabled(m_historyIndex > 0);
    m_forwardAction->setEnabled(m_historyIndex >= 0 && m_historyIndex < m_history.size() - 1);

    QDir dir(m_currentPath);
    m_upAction->setEnabled(m_inArchive || dir.cdUp());
}

void FilePane::onActivated(const QModelIndex &index)
{
    const QString path = m_model->filePath(index);
    if (path.isEmpty())
        return;

    if (m_model->isDir(index))
        navigateTo(path);
    else if (m_inArchive)
        openArchiveEntry(path);
    else if (ArchiveReader::isArchiveFile(path))
        navigateTo(path);
    else
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void FilePane::onBreadcrumbPathActivated(const QString &path)
{
    navigateTo(path);
}

QList<QModelIndex> FilePane::selectedNameIndexes() const
{
    return m_view->selectionModel()->selectedRows(FileSystemModel::NameColumn);
}

QStringList FilePane::selectedPaths() const
{
    QStringList paths;
    const auto indexes = selectedNameIndexes();
    paths.reserve(indexes.size());
    for (const QModelIndex &index : indexes)
        paths.append(m_model->filePath(index));
    return paths;
}

bool FilePane::isDirIndex(const QModelIndex &index) const
{
    return m_model->isDir(index);
}

QString FilePane::pathForIndex(const QModelIndex &index) const
{
    return m_model->filePath(index);
}

void FilePane::showContextMenu(const QPoint &pos)
{
    const auto indexes = selectedNameIndexes();

    QMenu menu(this);
    QAction *newFolderAction = menu.addAction(IconFactory::newFolder(), tr("New Folder"));
    newFolderAction->setEnabled(!m_inArchive);
    menu.addSeparator();
    QAction *openAction = menu.addAction(tr("Open"));
    openAction->setEnabled(!indexes.isEmpty());
    const bool singleFolderSelected = indexes.size() == 1 && isDirIndex(indexes.first());
    QAction *bookmarkAction = menu.addAction(
        IconFactory::bookmark(),
        singleFolderSelected
            ? tr("Add “%1” to Bookmarks").arg(QFileInfo(m_model->filePath(indexes.first())).fileName())
            : tr("Add This Folder to Bookmarks"));
    menu.addSeparator();
    QAction *renameAction = menu.addAction(tr("Rename"));
    renameAction->setEnabled(indexes.size() == 1);
    QAction *batchRenameAction = menu.addAction(IconFactory::batchRename(), tr("Batch Rename..."));
    batchRenameAction->setEnabled(indexes.size() >= 2);
    menu.addSeparator();
    QAction *cutAction = menu.addAction(tr("Cut"));
    cutAction->setEnabled(!indexes.isEmpty() && !m_inArchive);
    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(!indexes.isEmpty());
    QAction *pasteAction = menu.addAction(tr("Paste"));
    pasteAction->setEnabled(!FileClipboard::get().isEmpty() && !m_inArchive);
    menu.addSeparator();
    QAction *checksumAction = menu.addAction(IconFactory::checksum(), tr("Checksums..."));
    checksumAction->setEnabled(!indexes.isEmpty() && !m_inArchive);
    menu.addSeparator();
    QAction *terminalAction = menu.addAction(IconFactory::terminal(), tr("Open Terminal Here"));
    terminalAction->setEnabled(!m_inArchive);
    QAction *nativeMenuAction = menu.addAction(IconFactory::shellMenu(), tr("More Options (Shell Menu)..."));
    nativeMenuAction->setEnabled(!indexes.isEmpty() && !m_inArchive);
    menu.addSeparator();
    QAction *propertiesAction = menu.addAction(IconFactory::properties(), tr("Properties"));
    propertiesAction->setEnabled(!m_inArchive);
    menu.addSeparator();
    QAction *deleteAction = menu.addAction(tr("Delete"));
    deleteAction->setEnabled(!indexes.isEmpty());

    const QPoint globalPos = m_view->viewport()->mapToGlobal(pos);
    QAction *chosen = menu.exec(globalPos);
    if (chosen == newFolderAction)
        createNewFolder();
    else if (chosen == openAction && !indexes.isEmpty())
        onActivated(indexes.first());
    else if (chosen == bookmarkAction)
        BookmarkManager::instance().add(singleFolderSelected ? pathForIndex(indexes.first()) : m_currentPath);
    else if (chosen == renameAction)
        renameSelected();
    else if (chosen == batchRenameAction)
        batchRenameSelected();
    else if (chosen == cutAction)
        cutSelected();
    else if (chosen == copyAction)
        copySelected();
    else if (chosen == pasteAction)
        pasteClipboard();
    else if (chosen == checksumAction)
        showChecksums();
    else if (chosen == terminalAction)
        PlatformShell::instance().openTerminalHere(m_currentPath);
    else if (chosen == nativeMenuAction)
        PlatformShell::instance().showNativeContextMenu(selectedPaths(), m_currentPath, globalPos, this);
    else if (chosen == propertiesAction)
        PlatformShell::instance().showProperties(indexes.isEmpty() ? QStringList{m_currentPath} : selectedPaths(), this);
    else if (chosen == deleteAction)
        deleteSelected();
}

void FilePane::createNewFolder()
{
    // Creating new content in a locked pane is deliberately not blocked
    // (§10): read-only guards delete/rename/move, not "add a thing that
    // wasn't there before". An archive can't take a new entry in place, though.
    if (m_inArchive) {
        showReadOnlyNotice(tr("Pane is read-only: new folder is blocked."));
        return;
    }

    const QString baseName = tr("New Folder");
    QDir dir(m_currentPath);

    QString candidateName = baseName;
    int suffix = 2;
    while (QFileInfo::exists(dir.filePath(candidateName)))
        candidateName = tr("%1 (%2)").arg(baseName).arg(suffix++);

    if (!dir.mkdir(candidateName)) {
        QMessageBox::warning(this, tr("New Folder"), tr("Could not create the new folder."));
        return;
    }

    const QString newPath = dir.filePath(candidateName);
    // Select and immediately offer rename, matching Explorer/Nautilus
    // convention -- the auto-numbered name is just a placeholder. The scan
    // that refresh() kicks off is async, so the selection has to wait for
    // it to actually populate the row; SingleShotConnection means this
    // doesn't accumulate across repeated New Folder clicks. Skips the
    // rename step in a read-only pane -- renameSelected() would just
    // reject it right back, which would read as a confusing "error"
    // immediately after a successful creation.
    connect(m_model, &FileSystemModel::scanFinished, this,
            [this, newPath](int, int) {
                selectPath(newPath);
                if (!m_readOnly)
                    renameSelected();
            },
            Qt::SingleShotConnection);
    refresh();
}

void FilePane::onEditorClosed()
{
    m_model->setAutoRefreshSuspended(false);
}

void FilePane::rememberSelection()
{
    m_keptSelectionPaths.clear();
    m_keptCurrentPath = m_model->filePath(m_view->currentIndex());
    for (const QModelIndex &index : selectedNameIndexes())
        m_keptSelectionPaths.append(m_model->filePath(index));
}

void FilePane::restoreSelection()
{
    if (m_keptSelectionPaths.isEmpty())
        return;

    QItemSelection selection;
    QModelIndex currentIndex;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const QModelIndex nameIndex = m_model->index(row, FileSystemModel::NameColumn);
        const QString path = m_model->filePath(nameIndex);
        if (!m_keptSelectionPaths.contains(path))
            continue;
        selection.select(nameIndex, nameIndex);
        if (path == m_keptCurrentPath)
            currentIndex = nameIndex;
    }
    m_keptSelectionPaths.clear();
    m_keptCurrentPath.clear();

    if (selection.isEmpty())
        return;
    m_view->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    if (currentIndex.isValid()) {
        m_view->setCurrentIndex(currentIndex);
        m_view->scrollTo(currentIndex);
    }
}

void FilePane::renameSelected()
{
    if (changesBlocked()) {
        showReadOnlyNotice(tr("Pane is read-only: rename is blocked."));
        return;
    }

    const auto indexes = selectedNameIndexes();
    if (indexes.size() != 1)
        return;
    m_view->edit(indexes.first());
}

void FilePane::batchRenameSelected()
{
    if (changesBlocked()) {
        showReadOnlyNotice(tr("Pane is read-only: rename is blocked."));
        return;
    }

    const QStringList paths = selectedPaths();
    if (paths.size() < 2)
        return;

    auto *dialog = new BatchRenameDialog(paths, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &BatchRenameDialog::renamed, this, &FilePane::refresh);
    dialog->exec();
}

void FilePane::showChecksums()
{
    const QStringList paths = selectedPaths();
    if (paths.isEmpty())
        return;

    auto *dialog = new ChecksumDialog(paths, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void FilePane::deleteSelected()
{
    // Early exit purely for UX -- don't ask "are you sure?" only to reject
    // it right after. OpsEngine::submit() below is the real, unbypassable
    // enforcement point, this is just avoiding a pointless
    // confirmation dialog on the common direct-action path.
    if (changesBlocked()) {
        showReadOnlyNotice(tr("Pane is read-only: delete is blocked."));
        return;
    }

    const auto indexes = selectedNameIndexes();
    if (indexes.isEmpty())
        return;

    const QString question = indexes.size() == 1
        ? tr("Move “%1” to the Trash?").arg(QFileInfo(m_model->filePath(indexes.first())).fileName())
        : tr("Move %1 items to the Trash?").arg(indexes.size());

    if (QMessageBox::question(this, tr("Delete"), question) != QMessageBox::Yes)
        return;

    QStringList paths;
    for (const QModelIndex &index : indexes)
        paths.append(m_model->filePath(index));

    FileOpRequest request;
    request.kind = FileOpKind::Delete;
    request.sourcePaths = paths;
    request.sourceReadOnly = m_readOnly;

    QString rejection;
    FileOpHandle *handle = OpsEngine::instance().submit(request, &rejection);
    if (!handle) {
        showReadOnlyNotice(rejection);
        return;
    }

    new ProgressDialog(tr("Deleting"), handle, this);

    connect(handle, &FileOpHandle::finished, this, [this, paths](bool, const QStringList &failed) {
        for (const QString &path : paths) {
            if (!failed.contains(QFileInfo(path).fileName()))
                m_model->removeEntry(path);
        }
    });
}

void FilePane::cutSelected()
{
    const QStringList paths = selectedPaths();
    if (paths.isEmpty())
        return;
    if (m_inArchive) {
        showReadOnlyNotice(tr("Pane is read-only: cut is blocked."));
        return;
    }
    FileClipboard::set(paths, /*cut=*/true, m_readOnly);
}

void FilePane::copySelected()
{
    const QStringList paths = selectedPaths();
    if (paths.isEmpty())
        return;
    if (m_inArchive) {
        copyArchiveEntries(paths);
        return;
    }
    FileClipboard::set(paths, /*cut=*/false, m_readOnly);
}

void FilePane::openArchiveEntry(const QString &archiveEntryPath)
{
    QString archive;
    QString inner;
    if (!ArchiveReader::splitArchivePath(archiveEntryPath, &archive, &inner) || inner.isEmpty())
        return;

    auto *task = new ArchiveExtractTask(archive, {inner}, newArchiveTempFolder());
    connect(task, &ArchiveExtractTask::finished, this, [this](const QStringList &topLevel, const QString &error) {
        if (!error.isEmpty() || topLevel.isEmpty()) {
            QMessageBox::warning(this, tr("lvdExplorer"),
                                 error.isEmpty() ? tr("Could not extract the file from the archive.") : error);
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(topLevel.first()));
    });
    QThreadPool::globalInstance()->start(task);
}

QStringList FilePane::extractForTransfer(const QStringList &archiveEntryPaths)
{
    QString archive;
    QStringList entries;
    for (const QString &path : archiveEntryPaths) {
        QString inner;
        if (ArchiveReader::splitArchivePath(path, &archive, &inner) && !inner.isEmpty())
            entries.append(inner);
    }
    if (entries.isEmpty())
        return {};

    ArchiveExtractTask task(archive, entries, newArchiveTempFolder());
    QStringList topLevel;
    QString error;
    connect(&task, &ArchiveExtractTask::finished, this, [&topLevel, &error](const QStringList &paths, const QString &message) {
        topLevel = paths;
        error = message;
    });
    task.run();

    if (!error.isEmpty())
        QMessageBox::warning(this, tr("lvdExplorer"), error);
    return topLevel;
}

void FilePane::copyArchiveEntries(const QStringList &archiveEntryPaths)
{
    QString archive;
    QStringList entries;
    for (const QString &path : archiveEntryPaths) {
        QString inner;
        if (ArchiveReader::splitArchivePath(path, &archive, &inner) && !inner.isEmpty())
            entries.append(inner);
    }
    if (entries.isEmpty())
        return;

    auto *task = new ArchiveExtractTask(archive, entries, newArchiveTempFolder());
    connect(task, &ArchiveExtractTask::finished, this, [this](const QStringList &topLevel, const QString &error) {
        if (!error.isEmpty()) {
            QMessageBox::warning(this, tr("lvdExplorer"), error);
            return;
        }
        if (!topLevel.isEmpty())
            FileClipboard::set(topLevel, /*cut=*/false, /*sourceReadOnly=*/false);
    });
    QThreadPool::globalInstance()->start(task);
}

void FilePane::pasteClipboard()
{
    if (m_inArchive) {
        showReadOnlyNotice(tr("Pane is read-only: paste is blocked."));
        return;
    }

    const FileClipboard::Contents contents = FileClipboard::get();
    if (contents.isEmpty())
        return;

    FileOpRequest request;
    request.kind = contents.cut ? FileOpKind::Move : FileOpKind::Copy;
    request.sourcePaths = contents.paths;
    request.destDir = m_currentPath;
    request.sourceReadOnly = contents.sourceReadOnly;
    request.destReadOnly = m_readOnly;

    QString rejection;
    FileOpHandle *handle = OpsEngine::instance().submit(request, &rejection);
    if (!handle) {
        showReadOnlyNotice(rejection);
        return;
    }

    new ProgressDialog(contents.cut ? tr("Moving") : tr("Copying"), handle, this);
    connect(handle, &FileOpHandle::finished, this, [this](bool, const QStringList &) { refresh(); });
}

void FilePane::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    m_model->setReadOnly(changesBlocked());

    m_readOnlyCheck->blockSignals(true);
    m_readOnlyCheck->setChecked(readOnly);
    m_readOnlyCheck->blockSignals(false);

    setStyleSheet(readOnly ? QStringLiteral("FilePane { border: 2px solid #d9822b; }") : QString());
    updateStatusText();
}

void FilePane::showReadOnlyNotice(const QString &message)
{
    QToolTip::showText(QCursor::pos(), message, this);
}

void FilePane::updateStatusText()
{
    const int totalCount = m_model->rowCount();
    const QList<QModelIndex> selected = selectedNameIndexes();

    QString text = tr("%1 item(s)").arg(totalCount);
    if (!selected.isEmpty()) {
        qint64 totalBytes = 0;
        bool anySized = false;
        for (const QModelIndex &index : selected) {
            const qint64 size = m_model->sizeOf(index);
            if (size >= 0) {
                totalBytes += size;
                anySized = true;
            }
        }
        text += tr(" — %1 selected").arg(selected.size());
        if (anySized)
            text += tr(" (%1)").arg(QLocale::system().formattedDataSize(totalBytes));
    }
    if (changesBlocked())
        text += tr(" — read-only");
    if (!m_filterEdit->text().isEmpty())
        text += tr(" — filtered");

    m_statusText = text;
    emit statusMessage(text);
}

TabSessionState FilePane::captureState() const
{
    TabSessionState state;
    state.path = m_currentPath;
    state.headerState = m_view->header()->saveState();
    state.readOnly = m_readOnly;
    return state;
}

void FilePane::applyState(const TabSessionState &state)
{
    navigateTo(state.path);
    if (!state.headerState.isEmpty()) {
        m_view->header()->restoreState(state.headerState);
        // A session saved before columns became freely resizable may have
        // restored the old Stretch-on-Name/stretch-last-section setup;
        // reassert Interactive so a stale save can't silently undo the fix.
        m_view->header()->setSectionResizeMode(QHeaderView::Interactive);
        m_view->header()->setStretchLastSection(false);
    }
    setReadOnly(state.readOnly);
}

void FilePane::navigateToAndSelect(const QString &folderPath, const QString &targetFilePath)
{
    navigateTo(folderPath);
    // Qt::SingleShotConnection auto-disconnects after the first emission,
    // so this doesn't accumulate a stale connection on repeated jumps.
    connect(m_model, &FileSystemModel::scanFinished, this,
            [this, targetFilePath](int, int) { selectPath(targetFilePath); }, Qt::SingleShotConnection);
}

void FilePane::selectPath(const QString &path)
{
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const QModelIndex nameIndex = m_model->index(row, FileSystemModel::NameColumn);
        if (m_model->filePath(nameIndex) != path)
            continue;

        m_view->setCurrentIndex(nameIndex);
        m_view->scrollTo(nameIndex);
        m_view->selectionModel()->select(nameIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_view->setFocus();
        break;
    }
}

void FilePane::handleFilesDropped(const QStringList &sourcePaths, const QString &destDir,
                                   FilePane *sourcePane, bool forceCopy, bool forceMove)
{
    if (sourcePaths.isEmpty())
        return;
    if (m_inArchive) {
        showReadOnlyNotice(tr("Pane is read-only: drop is blocked."));
        return;
    }

    bool useMove;
    if (forceCopy)
        useMove = false;
    else if (forceMove)
        useMove = true;
    else
        // Default convention: move within the same volume, copy across volumes.
        useMove = QStorageInfo(sourcePaths.first()).rootPath() == QStorageInfo(destDir).rootPath();

    FileOpRequest request;
    request.kind = useMove ? FileOpKind::Move : FileOpKind::Copy;
    request.sourcePaths = sourcePaths;
    request.destDir = destDir;
    request.sourceReadOnly = sourcePane && sourcePane->isReadOnly();
    request.destReadOnly = isReadOnly();

    QString rejection;
    FileOpHandle *handle = OpsEngine::instance().submit(request, &rejection);
    if (!handle) {
        showReadOnlyNotice(rejection);
        return;
    }

    new ProgressDialog(useMove ? tr("Moving") : tr("Copying"), handle, this);

    // The source pane's tab could be closed while this runs in the
    // background (drop and copy/move are no longer synchronous with each
    // other), so guard against it rather than assuming it outlives the job.
    QPointer<FilePane> sourcePaneGuard(sourcePane);
    connect(handle, &FileOpHandle::finished, this, [this, useMove, sourcePaneGuard](bool, const QStringList &) {
        refresh();
        if (useMove && sourcePaneGuard && sourcePaneGuard != this)
            sourcePaneGuard->refresh();
    });
}
