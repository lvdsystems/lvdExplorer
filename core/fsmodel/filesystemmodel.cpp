#include "core/fsmodel/filesystemmodel.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLocale>
#include <QSet>
#include <QThreadPool>
#include <QTimer>

namespace {

// Above this many entries, skip per-entry watching and rely on the root
// directory watch alone (still catches add/remove/rename reliably; a
// folder this large watched entry-by-entry risks exhausting the OS's
// per-process watch-handle budget, e.g. inotify on Linux).
constexpr int kMaxWatchedEntries = 500;

// Coalesces a burst of individual change notifications (extracting an
// archive, a build writing many files) into one rescan instead of one per
// notification.
constexpr int kAutoRefreshDebounceMs = 300;

QString formatSize(qint64 bytes)
{
    if (bytes < 0)
        return {};

    constexpr qint64 kKiB = 1024;
    constexpr qint64 kMiB = kKiB * 1024;
    constexpr qint64 kGiB = kMiB * 1024;

    if (bytes < kKiB)
        return QObject::tr("%1 bytes").arg(bytes);
    if (bytes < kMiB)
        return QObject::tr("%1 KB").arg(bytes / double(kKiB), 0, 'f', 1);
    if (bytes < kGiB)
        return QObject::tr("%1 MB").arg(bytes / double(kMiB), 0, 'f', 1);
    return QObject::tr("%1 GB").arg(bytes / double(kGiB), 0, 'f', 1);
}

} // namespace

FileSystemModel::FileSystemModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &FileSystemModel::scheduleAutoRefresh);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &FileSystemModel::scheduleAutoRefresh);

    m_refreshDebounce = new QTimer(this);
    m_refreshDebounce->setSingleShot(true);
    m_refreshDebounce->setInterval(kAutoRefreshDebounceMs);
    connect(m_refreshDebounce, &QTimer::timeout, this, &FileSystemModel::refresh);
}

void FileSystemModel::setRootPath(const QString &path)
{
    if (m_rootPath == path)
        return;

    m_rootPath = path;
    watchRootDirectory();
    startScan();
}

void FileSystemModel::refresh()
{
    startScan();
}

void FileSystemModel::startScan()
{
    if (m_cancelFlag)
        m_cancelFlag->storeRelaxed(1);

    beginResetModel();
    m_entries.clear();
    m_visibleRows.clear();
    endResetModel();

    ++m_generation;
    m_cancelFlag = QSharedPointer<QAtomicInt>::create(0);

    emit scanStarted();

    auto *task = new DirectoryScanTask(m_rootPath, m_generation, m_cancelFlag);
    connect(task, &DirectoryScanTask::batchReady, this, &FileSystemModel::handleBatchReady);
    connect(task, &DirectoryScanTask::finished, this, &FileSystemModel::handleScanFinished);
    QThreadPool::globalInstance()->start(task);
}

void FileSystemModel::scheduleAutoRefresh()
{
    m_refreshDebounce->start();
}

void FileSystemModel::watchRootDirectory()
{
    const QStringList currentDirs = m_watcher->directories();
    if (!currentDirs.isEmpty())
        m_watcher->removePaths(currentDirs);
    if (!m_rootPath.isEmpty())
        m_watcher->addPath(m_rootPath);
}

void FileSystemModel::syncWatchedEntries()
{
    // Directory-level watching alone reliably catches entries being added,
    // removed, or renamed, but an existing file being modified *in place*
    // (its size/date changing with no rename) is only guaranteed to be
    // reported by watching that file directly -- hence both.
    QStringList desired;
    if (m_entries.size() <= kMaxWatchedEntries) {
        desired.reserve(m_entries.size());
        for (const auto &entry : std::as_const(m_entries))
            desired.append(entry.absolutePath);
    }

    const QStringList currentlyWatched = m_watcher->files();
    const QSet<QString> desiredSet(desired.begin(), desired.end());
    const QSet<QString> currentSet(currentlyWatched.begin(), currentlyWatched.end());

    QStringList toRemove;
    for (const QString &path : currentlyWatched) {
        if (!desiredSet.contains(path))
            toRemove.append(path);
    }
    if (!toRemove.isEmpty())
        m_watcher->removePaths(toRemove);

    QStringList toAdd;
    for (const QString &path : desired) {
        if (!currentSet.contains(path))
            toAdd.append(path);
    }
    if (!toAdd.isEmpty())
        m_watcher->addPaths(toAdd);
}

void FileSystemModel::setFilterText(const QString &text)
{
    if (m_filterText == text)
        return;

    beginResetModel();
    m_filterText = text;
    if (m_filterMode == FilterMode::Regex)
        m_filterRegex.setPattern(text);
    rebuildVisibleRows();
    endResetModel();
}

void FileSystemModel::setFilterMode(FilterMode mode)
{
    if (m_filterMode == mode)
        return;

    beginResetModel();
    m_filterMode = mode;
    if (mode == FilterMode::Regex)
        m_filterRegex.setPattern(m_filterText);
    rebuildVisibleRows();
    endResetModel();
}

bool FileSystemModel::passesFilter(const FileEntry &entry) const
{
    if (m_filterText.isEmpty())
        return true;
    if (m_filterMode == FilterMode::Regex)
        return m_filterRegex.isValid() && m_filterRegex.match(entry.name).hasMatch();
    return entry.name.contains(m_filterText, Qt::CaseInsensitive);
}

void FileSystemModel::rebuildVisibleRows()
{
    m_visibleRows.clear();
    if (!hasActiveFilter())
        return; // rowCount()/entryForRow() bypass m_visibleRows entirely in this case

    m_visibleRows.reserve(m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i) {
        if (passesFilter(m_entries.at(i)))
            m_visibleRows.append(i);
    }
}

int FileSystemModel::actualIndexForRow(int row) const
{
    if (!hasActiveFilter())
        return row;
    return (row >= 0 && row < m_visibleRows.size()) ? m_visibleRows.at(row) : -1;
}

const FileEntry *FileSystemModel::entryForRow(int row) const
{
    const int actual = actualIndexForRow(row);
    if (actual < 0 || actual >= m_entries.size())
        return nullptr;
    return &m_entries.at(actual);
}

void FileSystemModel::handleBatchReady(int generation, QVector<FileEntry> entries)
{
    if (generation != m_generation || entries.isEmpty())
        return;

    if (hasActiveFilter()) {
        // Streaming new rows in while a filter is active would require
        // recomputing which of them are visible and at what display
        // position; a full reset is simplest and this is a rare overlap
        // (filtering typically happens after a scan has already settled).
        beginResetModel();
        m_entries += entries;
        rebuildVisibleRows();
        endResetModel();
        return;
    }

    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size() + entries.size() - 1);
    m_entries += entries;
    endInsertRows();
}

void FileSystemModel::handleScanFinished(int generation, bool wasCancelled)
{
    if (generation != m_generation)
        return;

    if (!wasCancelled) {
        applySort();
        syncWatchedEntries();
    }

    int dirCount = 0;
    int fileCount = 0;
    for (const auto &entry : std::as_const(m_entries))
        entry.isDir ? ++dirCount : ++fileCount;

    emit scanFinished(fileCount, dirCount);
}

QString FileSystemModel::filePath(const QModelIndex &index) const
{
    const FileEntry *entry = entryForRow(index.row());
    return entry ? entry->absolutePath : QString();
}

bool FileSystemModel::isDir(const QModelIndex &index) const
{
    const FileEntry *entry = entryForRow(index.row());
    return entry && entry->isDir;
}

qint64 FileSystemModel::sizeOf(const QModelIndex &index) const
{
    const FileEntry *entry = entryForRow(index.row());
    return entry ? entry->size : -1;
}

bool FileSystemModel::removeEntry(const QString &absolutePath)
{
    for (int actualIndex = 0; actualIndex < m_entries.size(); ++actualIndex) {
        if (m_entries.at(actualIndex).absolutePath != absolutePath)
            continue;

        if (hasActiveFilter()) {
            beginResetModel();
            m_entries.removeAt(actualIndex);
            rebuildVisibleRows();
            endResetModel();
        } else {
            beginRemoveRows(QModelIndex(), actualIndex, actualIndex);
            m_entries.removeAt(actualIndex);
            endRemoveRows();
        }
        return true;
    }
    return false;
}

int FileSystemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return hasActiveFilter() ? m_visibleRows.size() : m_entries.size();
}

int FileSystemModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(ColumnCount);
}

QVariant FileSystemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    const FileEntry *entryPtr = entryForRow(index.row());
    if (!entryPtr)
        return {};
    const FileEntry &entry = *entryPtr;

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case NameColumn:
            return entry.name;
        case SizeColumn:
            return entry.isDir ? QString() : formatSize(entry.size);
        case TypeColumn:
            return entry.isDir ? tr("File folder") : m_iconProvider.type(QFileInfo(entry.absolutePath));
        case ModifiedColumn:
            return QLocale::system().toString(entry.modified, QLocale::ShortFormat);
        default:
            return {};
        }
    }

    if (role == Qt::DecorationRole && index.column() == NameColumn)
        return m_iconProvider.icon(QFileInfo(entry.absolutePath));

    if (role == Qt::TextAlignmentRole && index.column() == SizeColumn)
        return QVariant(Qt::AlignRight | Qt::AlignVCenter);

    return {};
}

QVariant FileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case NameColumn:
        return tr("Name");
    case SizeColumn:
        return tr("Size");
    case TypeColumn:
        return tr("Type");
    case ModifiedColumn:
        return tr("Date Modified");
    default:
        return {};
    }
}

bool FileSystemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || index.column() != NameColumn)
        return false;

    if (m_readOnly) {
        emit errorOccurred(tr("Pane is read-only: rename is blocked."));
        return false;
    }

    const int actualIndex = actualIndexForRow(index.row());
    if (actualIndex < 0 || actualIndex >= m_entries.size())
        return false;

    const QString newName = value.toString().trimmed();
    if (newName.isEmpty() || newName.contains(QLatin1Char('/')) || newName.contains(QLatin1Char('\\'))) {
        emit errorOccurred(tr("“%1” is not a valid name.").arg(newName));
        return false;
    }

    FileEntry &entry = m_entries[actualIndex];
    if (newName == entry.name)
        return false;

    const QFileInfo oldInfo(entry.absolutePath);
    const QString newPath = oldInfo.absoluteDir().filePath(newName);

    QDir dir;
    if (!dir.rename(entry.absolutePath, newPath)) {
        emit errorOccurred(tr("Could not rename “%1” to “%2”.").arg(entry.name, newName));
        return false;
    }

    entry.name = newName;
    entry.absolutePath = newPath;
    emit dataChanged(index, index);
    return true;
}

Qt::ItemFlags FileSystemModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    // ItemIsDragEnabled is what QAbstractItemView's mouse handling checks
    // before starting a drag (see shouldStartDrag()) -- without it, moving
    // the mouse past the drag threshold on a pressed item falls back to
    // its other default behavior for ExtendedSelection, which is to keep
    // extending the selection instead. ItemIsDropEnabled likewise lets a
    // drag-over actually land on a row rather than only the viewport
    // background, even though FileTreeView's own dropEvent() does the
    // real move/copy dispatch regardless of which row was under the cursor.
    Qt::ItemFlags result = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    if (index.column() == NameColumn)
        result |= Qt::ItemIsEditable;
    return result;
}

bool FileSystemModel::rawLessThan(const FileEntry &a, const FileEntry &b, int column)
{
    switch (column) {
    case SizeColumn:
        return a.size < b.size;
    case TypeColumn:
        return QString::compare(a.name.section(QLatin1Char('.'), -1),
                                 b.name.section(QLatin1Char('.'), -1),
                                 Qt::CaseInsensitive) < 0;
    case ModifiedColumn:
        return a.modified < b.modified;
    case NameColumn:
    default:
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    }
}

void FileSystemModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= ColumnCount)
        return;

    m_sortColumn = column;
    m_sortOrder = order;
    applySort();
}

void FileSystemModel::applySort()
{
    if (hasActiveFilter()) {
        // Persistent-index preservation (below) assumes row == m_entries
        // index, which isn't true while filtered; reset instead.
        beginResetModel();
        std::stable_sort(m_entries.begin(), m_entries.end(), [this](const FileEntry &a, const FileEntry &b) {
            if (a.isDir != b.isDir)
                return a.isDir;
            return m_sortOrder == Qt::AscendingOrder ? rawLessThan(a, b, m_sortColumn)
                                                       : rawLessThan(b, a, m_sortColumn);
        });
        rebuildVisibleRows();
        endResetModel();
        return;
    }

    emit layoutAboutToBeChanged();

    const QModelIndexList persistent = persistentIndexList();
    QVector<QString> pathByPersistent;
    pathByPersistent.reserve(persistent.size());
    for (const QModelIndex &idx : persistent)
        pathByPersistent.append(m_entries.value(idx.row()).absolutePath);

    // Directories are always grouped first, then sorted by the active
    // column within each group -- matches Explorer/Nautilus convention.
    std::stable_sort(m_entries.begin(), m_entries.end(), [this](const FileEntry &a, const FileEntry &b) {
        if (a.isDir != b.isDir)
            return a.isDir;
        return m_sortOrder == Qt::AscendingOrder ? rawLessThan(a, b, m_sortColumn)
                                                   : rawLessThan(b, a, m_sortColumn);
    });

    QHash<QString, int> rowForPath;
    rowForPath.reserve(m_entries.size());
    for (int row = 0; row < m_entries.size(); ++row)
        rowForPath.insert(m_entries.at(row).absolutePath, row);

    QModelIndexList updated;
    updated.reserve(persistent.size());
    for (int i = 0; i < persistent.size(); ++i) {
        const int newRow = rowForPath.value(pathByPersistent.at(i), -1);
        updated.append(newRow >= 0 ? index(newRow, persistent.at(i).column()) : QModelIndex());
    }
    changePersistentIndexList(persistent, updated);

    emit layoutChanged();
}
