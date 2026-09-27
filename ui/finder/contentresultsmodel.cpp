#include "ui/finder/contentresultsmodel.h"

#include <QDir>
#include <QFileInfo>

ContentResultsModel::ContentResultsModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void ContentResultsModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

void ContentResultsModel::addEntries(const QVector<ContentMatch> &entries)
{
    if (entries.isEmpty())
        return;

    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size() + entries.size() - 1);
    m_entries += entries;
    endInsertRows();
}

QString ContentResultsModel::filePath(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    return m_entries.at(index.row()).filePath;
}

int ContentResultsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

int ContentResultsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(ColumnCount);
}

QVariant ContentResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};

    const ContentMatch &entry = m_entries.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case NameColumn:
            return QFileInfo(entry.filePath).fileName();
        case LineColumn:
            return entry.lineNumber;
        case SnippetColumn:
            return entry.lineText;
        case PathColumn:
            return QDir::toNativeSeparators(QFileInfo(entry.filePath).absolutePath());
        default:
            return {};
        }
    }

    if (role == Qt::DecorationRole && index.column() == NameColumn)
        return m_iconProvider.icon(QFileInfo(entry.filePath));

    if (role == Qt::ToolTipRole)
        return QStringLiteral("%1:%2").arg(QDir::toNativeSeparators(entry.filePath)).arg(entry.lineNumber);

    return {};
}

QVariant ContentResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case NameColumn:
        return tr("Name");
    case LineColumn:
        return tr("Line");
    case SnippetColumn:
        return tr("Match");
    case PathColumn:
        return tr("Folder");
    default:
        return {};
    }
}
