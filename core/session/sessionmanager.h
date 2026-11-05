#pragma once

#include "core/session/sessiondata.h"

#include <QString>
#include <QStringList>

// Storage for named sessions. Files are written atomically (via QSaveFile:
// temp file + commit) so a crash mid write can't corrupt a session.
//
// Saving/loading is always an explicit, user-triggered action (MainWindow's
// "Save Session"/"Save Current As..."/Session-menu entries) -- there is no
// silent autosave slot. The only thing persisted automatically, at close,
// is *which* named session was last active (lastSessionName()/
// setLastSessionName()), so launch can restore that same named session's
// own on-disk content; it carries no window/pane state of its own.
class SessionManager
{
public:
    static bool isPortableMode();
    static QString dataDirectory();
    static QString sessionsDirectory();

    static bool save(const QString &sessionName, const SessionData &data);
    static bool load(const QString &sessionName, SessionData &outData);
    static bool exists(const QString &sessionName);
    // Also drops sessionName out of recentSessionNames() and clears
    // lastSessionName() if it was the one removed, so neither can go on
    // pointing at a session that no longer exists.
    static bool remove(const QString &sessionName);
    static QStringList listNamedSessions();

    // Most-recently-used session names, most recent first, capped at 10 --
    // backs the Session menu's recent-sessions list.
    static QStringList recentSessionNames();
    // Moves sessionName to the front of recentSessionNames() (inserting it
    // if new), trimming back down to 10 entries.
    static void noteSessionUsed(const QString &sessionName);

    // The session to restore at next launch -- just a name; see the class
    // comment above for why no state is stored alongside it.
    static QString lastSessionName();
    static void setLastSessionName(const QString &sessionName);
};
