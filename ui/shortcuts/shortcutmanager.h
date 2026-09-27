#pragma once

#include <QHash>
#include <QKeySequence>
#include <QMultiHash>
#include <QPointer>
#include <QString>

class QAction;

// App-wide keyboard shortcut registry. The same logical
// shortcut (e.g. "Rename") exists as N live QAction instances -- one per
// open pane -- so this can't just be QAction::setShortcut() called once.
// registerAction() looks up any saved override and applies it; setShortcut()
// (from the editor dialog) updates every currently-live QAction sharing
// that id, plus persists the choice via QSettings.
class ShortcutManager
{
public:
    struct Entry
    {
        QString id;
        QString description;
        QKeySequence defaultSequence;
        QKeySequence currentSequence;
    };

    static ShortcutManager &instance();

    void registerAction(QAction *action, const QString &id, const QString &description,
                         const QKeySequence &defaultSequence);

    QList<Entry> allEntries() const;
    QKeySequence shortcutFor(const QString &id) const;
    bool isInUseByOther(const QString &id, const QKeySequence &sequence) const;

    void setShortcut(const QString &id, const QKeySequence &sequence);
    void resetToDefault(const QString &id);
    void resetAll();

private:
    ShortcutManager() = default;

    void applyToLiveActions(const QString &id, const QKeySequence &sequence);
    static void save(const QString &id, const QKeySequence &sequence);
    static void clearSaved(const QString &id);
    static QKeySequence loadOverride(const QString &id, const QKeySequence &defaultSequence);

    QHash<QString, Entry> m_entries;
    QMultiHash<QString, QPointer<QAction>> m_liveActions;
};
