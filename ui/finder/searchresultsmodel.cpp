#include "ui/finder/searchresultsmodel.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>

namespace {

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

SearchResultsModel::SearchResultsModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void SearchResultsModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

void SearchResultsModel::addEntries(const QVector<FileEntry> &entries)
{
    if (entries.isEmpty())
        return;

    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size() + entries.size() - 1);
    m_entries += entries;
    endInsertRows();
}

QString SearchResultsModel::filePath(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    return m_entries.at(index.row()).absolutePath;
}

int SearchResultsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

int SearchResultsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(ColumnCount);
}

QVariant SearchResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};

    const FileEntry &entry = m_entries.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case NameColumn:
            return entry.name;
        case PathColumn:
            return QDir::toNativeSeparators(QFileInfo(entry.absolutePath).absolutePath());
        case SizeColumn:
            return entry.isDir ? QString() : formatSize(entry.size);
        case ModifiedColumn:
            return QLocale::system().toString(entry.modified, QLocale::ShortFormat);
        default:
            return {};
        }
    }

    if (role == Qt::DecorationRole && index.column() == NameColumn)
        return m_iconProvider.icon(QFileInfo(entry.absolutePath));

    if (role == Qt::ToolTipRole)
        return QDir::toNativeSeparators(entry.absolutePath);

    return {};
}

QVariant SearchResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case NameColumn:
        return tr("Name");
    case PathColumn:
        return tr("Folder");
    case SizeColumn:
        return tr("Size");
    case ModifiedColumn:
        return tr("Date Modified");
    default:
        return {};
    }
}
