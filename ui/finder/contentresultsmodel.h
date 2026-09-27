#pragma once

#include "core/search/contentmatch.h"

#include <QAbstractTableModel>
#include <QFileIconProvider>
#include <QVector>

// Results list for the Finder panel's Content tab (grep-style search).
// Unlike SearchResultsModel, one row is a matching *line*, not a file --
// the same file can appear multiple times with different line numbers.
class ContentResultsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { NameColumn = 0, LineColumn, SnippetColumn, PathColumn, ColumnCount };

    explicit ContentResultsModel(QObject *parent = nullptr);

    void clear();
    void addEntries(const QVector<ContentMatch> &entries);

    QString filePath(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QVector<ContentMatch> m_entries;
    QFileIconProvider m_iconProvider;
};
