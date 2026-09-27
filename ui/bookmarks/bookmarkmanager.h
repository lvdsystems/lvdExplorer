#pragma once

#include <QObject>
#include <QStringList>

// Persisted list of favorite folders, shown in the dockable Bookmarks
// panel (default hidden, per the panel's own doc comment). QSettings-
// backed, same convention as ShortcutManager. A QObject singleton (unlike
// ShortcutManager) because bookmarking a folder from any pane's context
// menu needs to live-update the panel if it happens to already be open.
class BookmarkManager : public QObject
{
    Q_OBJECT

public:
    static BookmarkManager &instance();

    QStringList bookmarks() const { return m_bookmarks; }
    bool contains(const QString &path) const;
    void add(const QString &path);
    void remove(const QString &path);

signals:
    void changed();

private:
    BookmarkManager();

    void save();

    QStringList m_bookmarks;
};
