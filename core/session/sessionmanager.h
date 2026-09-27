#pragma once

#include "core/session/sessiondata.h"

#include <QString>
#include <QStringList>

// Storage for named sessions plus the reserved autosave slot. Files are
// written atomically (via QSaveFile: temp file + commit) so a crash mid
// write can't corrupt a session.
class SessionManager
{
public:
    // Name reserved for the silent autosave-on-close / restore-on-launch
    // slot; kept out of listNamedSessions() so it doesn't clutter the
    // user-facing session manager.
    static const QString kAutosaveSessionName;

    static bool isPortableMode();
    static QString dataDirectory();
    static QString sessionsDirectory();

    static bool save(const QString &sessionName, const SessionData &data);
    static bool load(const QString &sessionName, SessionData &outData);
    static bool exists(const QString &sessionName);
    static bool remove(const QString &sessionName);
    static QStringList listNamedSessions();
};
