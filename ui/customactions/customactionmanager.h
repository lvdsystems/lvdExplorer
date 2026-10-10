#pragma once

#include "ui/customactions/customaction.h"

#include <QList>
#include <QObject>
#include <QString>

// Persisted list of user-defined context-menu actions (QSettings-backed,
// same convention as BookmarkManager/ShortcutManager). A QObject singleton
// so every open pane's context menu reflects an edit made in
// CustomActionsDialog without needing to be told individually.
class CustomActionManager : public QObject
{
    Q_OBJECT

public:
    static CustomActionManager &instance();

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    QList<CustomAction> actions() const { return m_actions; }
    // Enabled Kind::NewFile actions, in definition order.
    QList<CustomAction> newFileActions() const;
    // Enabled Kind::OpenWith actions whose extensions contain filePath's
    // suffix (case-insensitive), in definition order.
    QList<CustomAction> openWithActionsFor(const QString &filePath) const;

    void addOrUpdate(const CustomAction &action);
    void remove(const QString &id);

signals:
    void changed();

private:
    CustomActionManager();

    void load();
    void save();
    void seedDefaults();

    bool m_enabled = true;
    QList<CustomAction> m_actions;
};
