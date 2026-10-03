#pragma once

#include "core/session/sessiondata.h"

#include <QModelIndex>
#include <QStringList>
#include <QWidget>

class QAction;
class QCheckBox;
class QLineEdit;
class BreadcrumbBar;
class FileSystemModel;
class FileTreeView;

// A single browsable pane: toolbar (back/forward/up/refresh/read-only
// lock), breadcrumb, and a details view over a FileSystemModel. Arranged
// and tabbed by PaneContainer/PaneGridWidget.
class FilePane : public QWidget
{
    Q_OBJECT

public:
    explicit FilePane(QWidget *parent = nullptr);

    void navigateTo(const QString &path);
    QString currentPath() const { return m_currentPath; }

    bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly);
    bool isBrowsingArchive() const { return m_inArchive; }

    // Used by FileTreeView to implement cross-pane drag & drop (§8) without
    // routing file-system operations through QAbstractItemModel's
    // mimeData()/dropMimeData(), which assumes reordering within one model.
    QStringList selectedPaths() const;
    QStringList extractForTransfer(const QStringList &archiveEntryPaths);
    bool isDirIndex(const QModelIndex &index) const;
    QString pathForIndex(const QModelIndex &index) const;
    void handleFilesDropped(const QStringList &sourcePaths, const QString &destDir,
                             FilePane *sourcePane, bool forceCopy, bool forceMove);

    TabSessionState captureState() const;
    void applyState(const TabSessionState &state);

    void navigateToAndSelect(const QString &folderPath, const QString &targetFilePath);
    void selectPath(const QString &path);

    QString statusText() const { return m_statusText; }

signals:
    void pathChanged(const QString &path);
    void statusMessage(const QString &text);

private slots:
    void goBack();
    void goForward();
    void goUp();
    void refresh();
    void onActivated(const QModelIndex &index);
    void onBreadcrumbPathActivated(const QString &path);
    void showContextMenu(const QPoint &pos);
    void renameSelected();
    void batchRenameSelected();
    void showChecksums();
    void onEditorClosed();
    void rememberSelection();
    void restoreSelection();
    void deleteSelected();
    void createNewFolder();
    void cutSelected();
    void copySelected();
    void pasteClipboard();

private:
    void navigateInternal(const QString &path);
    void pushHistory(const QString &path);
    void updateNavigationActions();
    QList<QModelIndex> selectedNameIndexes() const;
    void showReadOnlyNotice(const QString &message);
    void updateStatusText();
    bool changesBlocked() const { return m_readOnly || m_inArchive; }
    void openArchiveEntry(const QString &archiveEntryPath);
    void copyArchiveEntries(const QStringList &archiveEntryPaths);

    FileSystemModel *m_model = nullptr;
    FileTreeView *m_view = nullptr;
    BreadcrumbBar *m_breadcrumb = nullptr;
    QCheckBox *m_readOnlyCheck = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QCheckBox *m_filterRegexCheck = nullptr;

    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QAction *m_upAction = nullptr;
    QAction *m_refreshAction = nullptr;
    QAction *m_cutAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;

    QStringList m_history;
    int m_historyIndex = -1;
    QString m_currentPath;
    bool m_readOnly = false;
    bool m_inArchive = false;
    QString m_statusText;
    // Selection captured just before a scan resets the view, restored once
    // the scan finishes so external changes don't drop what was selected.
    QStringList m_keptSelectionPaths;
    QString m_keptCurrentPath;
};
