#include "ui/shortcuts/shortcutmanager.h"

#include <QAction>
#include <QSettings>

#include <algorithm>

namespace {
QString settingsGroup()
{
    return QStringLiteral("shortcuts");
}
} // namespace

ShortcutManager &ShortcutManager::instance()
{
    static ShortcutManager manager;
    return manager;
}

QKeySequence ShortcutManager::loadOverride(const QString &id, const QKeySequence &defaultSequence)
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    const bool has = settings.contains(id);
    const QString saved = settings.value(id).toString();
    settings.endGroup();
    if (!has)
        return defaultSequence;
    // An explicitly saved empty string means "user unassigned this
    // shortcut" -- distinct from "no override recorded at all".
    return saved.isEmpty() ? QKeySequence() : QKeySequence::fromString(saved);
}

void ShortcutManager::save(const QString &id, const QKeySequence &sequence)
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    settings.setValue(id, sequence.toString());
    settings.endGroup();
}

void ShortcutManager::clearSaved(const QString &id)
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    settings.remove(id);
    settings.endGroup();
}

void ShortcutManager::registerAction(QAction *action, const QString &id, const QString &description,
                                      const QKeySequence &defaultSequence)
{
    if (!m_entries.contains(id)) {
        Entry entry;
        entry.id = id;
        entry.description = description;
        entry.defaultSequence = defaultSequence;
        entry.currentSequence = loadOverride(id, defaultSequence);
        m_entries.insert(id, entry);
    }

    action->setShortcut(m_entries.value(id).currentSequence);
    m_liveActions.insert(id, action);
}

QList<ShortcutManager::Entry> ShortcutManager::allEntries() const
{
    QList<Entry> result = m_entries.values();
    std::sort(result.begin(), result.end(),
              [](const Entry &a, const Entry &b) { return a.description < b.description; });
    return result;
}

QKeySequence ShortcutManager::shortcutFor(const QString &id) const
{
    return m_entries.value(id).currentSequence;
}

bool ShortcutManager::isInUseByOther(const QString &id, const QKeySequence &sequence) const
{
    if (sequence.isEmpty())
        return false;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        if (it.key() != id && it.value().currentSequence == sequence)
            return true;
    }
    return false;
}

void ShortcutManager::applyToLiveActions(const QString &id, const QKeySequence &sequence)
{
    // Reinserting only the still-alive pointers doubles as pruning dead
    // ones -- avoids the multi-hash growing unbounded as tabs/panes open
    // and close over a long session.
    const QList<QPointer<QAction>> actions = m_liveActions.values(id);
    m_liveActions.remove(id);
    for (const QPointer<QAction> &action : actions) {
        if (!action)
            continue;
        action->setShortcut(sequence);
        m_liveActions.insert(id, action);
    }
}

void ShortcutManager::setShortcut(const QString &id, const QKeySequence &sequence)
{
    if (!m_entries.contains(id))
        return;
    m_entries[id].currentSequence = sequence;
    save(id, sequence);
    applyToLiveActions(id, sequence);
}

void ShortcutManager::resetToDefault(const QString &id)
{
    if (!m_entries.contains(id))
        return;
    const QKeySequence defaultSequence = m_entries.value(id).defaultSequence;
    m_entries[id].currentSequence = defaultSequence;
    clearSaved(id);
    applyToLiveActions(id, defaultSequence);
}

void ShortcutManager::resetAll()
{
    const QStringList ids = m_entries.keys();
    for (const QString &id : ids)
        resetToDefault(id);
}
