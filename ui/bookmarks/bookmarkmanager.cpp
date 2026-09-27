#include "ui/bookmarks/bookmarkmanager.h"

#include <QDir>
#include <QSettings>

namespace {
QString settingsGroup()
{
    return QStringLiteral("bookmarks");
}
} // namespace

BookmarkManager &BookmarkManager::instance()
{
    static BookmarkManager manager;
    return manager;
}

BookmarkManager::BookmarkManager()
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    m_bookmarks = settings.value(QStringLiteral("paths")).toStringList();
    settings.endGroup();
}

bool BookmarkManager::contains(const QString &path) const
{
    return m_bookmarks.contains(QDir::cleanPath(path));
}

void BookmarkManager::add(const QString &path)
{
    const QString cleaned = QDir::cleanPath(path);
    if (cleaned.isEmpty() || m_bookmarks.contains(cleaned))
        return;

    m_bookmarks.append(cleaned);
    save();
    emit changed();
}

void BookmarkManager::remove(const QString &path)
{
    if (m_bookmarks.removeAll(QDir::cleanPath(path)) > 0) {
        save();
        emit changed();
    }
}

void BookmarkManager::save()
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    settings.setValue(QStringLiteral("paths"), m_bookmarks);
    settings.endGroup();
}
