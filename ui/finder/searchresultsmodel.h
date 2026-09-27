#pragma once

#include "core/fsmodel/directoryscantask.h" // FileEntry

#include <QAbstractTableModel>
#include <QFileIconProvider>
#include <QVector>

// Simple accumulating results list for the Finder panel (§6). Unlike
// FileSystemModel this isn't tied to one directory's async scan lifecycle
// -- entries can come from anywhere under the search root, so it shows a
// Path (containing folder) column instead of Type.
class SearchResultsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { NameColumn = 0, PathColumn, SizeColumn, ModifiedColumn, ColumnCount };

    explicit SearchResultsModel(QObject *parent = nullptr);

    void clear();
    void addEntries(const QVector<FileEntry> &entries);

    QString filePath(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QVector<FileEntry> m_entries;
    QFileIconProvider m_iconProvider;
};
