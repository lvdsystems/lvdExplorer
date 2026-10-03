#pragma once

#include <QAbstractTableModel>
#include <QAtomicInt>
#include <QFileIconProvider>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QVector>

#include "core/fsmodel/directoryscantask.h"

class QFileSystemWatcher;
class QTimer;

// Flat (non-recursive) listing of a single directory's contents, backed by
// an async DirectoryScanTask so navigation never blocks the UI thread.
// Deliberately a QAbstractTableModel rather than a full tree: each pane
// shows one directory level at a time, matching Explorer/Q-Dir's
// right-hand detail view.
//
// Also implements the instant per-pane filter box: when
// no filter is active, rows map 1:1 onto m_entries and streaming/sorting
// behave exactly as before (fast incremental insert). A non-empty filter
// switches to a visible-row index (m_visibleRows) and falls back to full
// model resets for mutations -- correctness over smoothness in that case,
// since it's a much less common path than plain browsing.
//
// Also watches the current directory (and, below kMaxWatchedEntries, each
// entry inside it) with a QFileSystemWatcher, so changes made by *other*
// tools/processes -- a file added or removed, or an existing file's size
// or modified date changing in place -- rescan automatically instead of
// only ever updating on an explicit Refresh.
class FileSystemModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { NameColumn = 0, SizeColumn, TypeColumn, ModifiedColumn, ColumnCount };
    enum class FilterMode { Substring, Regex };

    explicit FileSystemModel(QObject *parent = nullptr);

    void setRootPath(const QString &path);
    QString rootPath() const { return m_rootPath; }
    void refresh();

    // While an inline rename is open, a watcher-triggered rescan would reset
    // the view and tear the editor down mid-edit, so it's held until resumed.
    void setAutoRefreshSuspended(bool suspended);

    // Backstop enforcement for §10: FilePane's own read-only checkbox
    // already declines to *start* an inline rename, but QTreeView's
    // SelectedClicked edit trigger can reach setData() without going
    // through that path (clicking an already-selected item's name). This
    // is the one place that can't be bypassed.
    void setReadOnly(bool readOnly) { m_readOnly = readOnly; }
    bool isReadOnly() const { return m_readOnly; }

    void setFilterText(const QString &text);
    void setFilterMode(FilterMode mode);
    QString filterText() const { return m_filterText; }
    FilterMode filterMode() const { return m_filterMode; }

    QString filePath(const QModelIndex &index) const;
    bool isDir(const QModelIndex &index) const;
    qint64 sizeOf(const QModelIndex &index) const; // -1 for directories or an invalid index

    bool removeEntry(const QString &absolutePath);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void sort(int column, Qt::SortOrder order) override;

signals:
    // Fires before the reset that every scan begins with, while the view
    // still knows its selection.
    void scanAboutToStart();
    void scanStarted();
    void scanFinished(int fileCount, int dirCount);
    void errorOccurred(const QString &message);

private slots:
    void handleBatchReady(int generation, QVector<FileEntry> entries);
    void handleScanFinished(int generation, bool wasCancelled);
    void scheduleAutoRefresh();
    void onAutoRefreshTimeout();

private:
    void startScan();
    void applySort();
    bool hasActiveFilter() const { return !m_filterText.isEmpty(); }
    bool passesFilter(const FileEntry &entry) const;
    void rebuildVisibleRows();
    int actualIndexForRow(int row) const;
    const FileEntry *entryForRow(int row) const;
    void watchRootDirectory();
    void syncWatchedEntries();

    static bool rawLessThan(const FileEntry &a, const FileEntry &b, int column);

    QString m_rootPath;
    QVector<FileEntry> m_entries;
    QVector<int> m_visibleRows; // m_entries indices that pass the active filter
    int m_generation = 0;
    QSharedPointer<QAtomicInt> m_cancelFlag;
    int m_sortColumn = NameColumn;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    QFileIconProvider m_iconProvider;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_refreshDebounce = nullptr;
    bool m_autoRefreshSuspended = false;
    bool m_autoRefreshPending = false;

    QString m_filterText;
    FilterMode m_filterMode = FilterMode::Substring;
    QRegularExpression m_filterRegex;
    bool m_readOnly = false;
};
