#include "ui/pane/filepane.h"

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
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>

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
    m_view->header()->setSectionResizeMode(FileSystemModel::NameColumn, QHeaderView::Stretch);
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
        drivesMenu->clear();
        for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
            if (!volume.isValid() || !volume.isReady())
                continue;
            const QString root = volume.rootPath();
            QString label = QDir::toNativeSeparators(root);
            if (!volume.displayName().isEmpty() && volume.displayName() != label)
                label = tr("%1 (%2)").arg(volume.displayName(), label);
            QAction *action = drivesMenu->addAction(label);
            connect(action, &QAction::triggered, this, [this, root] { navigateTo(root); });
        }
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
    if (cleaned.isEmpty() || !QFileInfo::exists(cleaned)) {
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
    m_upAction->setEnabled(dir.cdUp());
}

void FilePane::onActivated(const QModelIndex &index)
{
    const QString path = m_model->filePath(index);
    if (path.isEmpty())
        return;

    if (m_model->isDir(index))
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
    cutAction->setEnabled(!indexes.isEmpty());
    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(!indexes.isEmpty());
    QAction *pasteAction = menu.addAction(tr("Paste"));
    pasteAction->setEnabled(!FileClipboard::get().isEmpty());
    menu.addSeparator();
    QAction *checksumAction = menu.addAction(IconFactory::checksum(), tr("Checksums..."));
    checksumAction->setEnabled(!indexes.isEmpty());
    menu.addSeparator();
    QAction *terminalAction = menu.addAction(IconFactory::terminal(), tr("Open Terminal Here"));
    QAction *nativeMenuAction = menu.addAction(IconFactory::shellMenu(), tr("More Options (Shell Menu)..."));
    nativeMenuAction->setEnabled(!indexes.isEmpty());
    menu.addSeparator();
    QAction *propertiesAction = menu.addAction(IconFactory::properties(), tr("Properties"));
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
    // wasn't there before".
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

void FilePane::renameSelected()
{
    if (m_readOnly) {
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
    if (m_readOnly) {
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
    if (m_readOnly) {
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
    FileClipboard::set(paths, /*cut=*/true, m_readOnly);
}

void FilePane::copySelected()
{
    const QStringList paths = selectedPaths();
    if (paths.isEmpty())
        return;
    FileClipboard::set(paths, /*cut=*/false, m_readOnly);
}

void FilePane::pasteClipboard()
{
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
    m_model->setReadOnly(readOnly);

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
    if (m_readOnly)
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
    if (!state.headerState.isEmpty())
        m_view->header()->restoreState(state.headerState);
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
