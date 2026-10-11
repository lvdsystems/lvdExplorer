#include "core/session/portablesettings.h"

#include "core/session/sessionmanager.h"

#include <QDir>

QSettings portableAwareSettings()
{
    if (!SessionManager::isPortableMode())
        return QSettings();

    const QString dir = SessionManager::dataDirectory();
    QDir().mkpath(dir);
    return QSettings(dir + QStringLiteral("/settings.ini"), QSettings::IniFormat);
}
