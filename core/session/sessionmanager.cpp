#include "core/session/sessionmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

const QString SessionManager::kAutosaveSessionName = QStringLiteral("last");

namespace {

QJsonObject tabToJson(const TabSessionState &tab)
{
    QJsonObject obj;
    obj[QStringLiteral("path")] = tab.path;
    obj[QStringLiteral("headerState")] = QString::fromLatin1(tab.headerState.toBase64());
    obj[QStringLiteral("readOnly")] = tab.readOnly;
    return obj;
}

TabSessionState tabFromJson(const QJsonObject &obj)
{
    TabSessionState tab;
    tab.path = obj.value(QStringLiteral("path")).toString();
    tab.headerState = QByteArray::fromBase64(obj.value(QStringLiteral("headerState")).toString().toLatin1());
    tab.readOnly = obj.value(QStringLiteral("readOnly")).toBool(false);
    return tab;
}

QJsonObject paneToJson(const PaneSessionState &pane)
{
    QJsonArray tabs;
    for (const TabSessionState &tab : pane.tabs)
        tabs.append(tabToJson(tab));

    QJsonObject obj;
    obj[QStringLiteral("activeTab")] = pane.activeTab;
    obj[QStringLiteral("tabs")] = tabs;
    return obj;
}

PaneSessionState paneFromJson(const QJsonObject &obj)
{
    PaneSessionState pane;
    pane.activeTab = obj.value(QStringLiteral("activeTab")).toInt(0);
    const QJsonArray tabs = obj.value(QStringLiteral("tabs")).toArray();
    for (const QJsonValue &value : tabs)
        pane.tabs.append(tabFromJson(value.toObject()));
    return pane;
}

QJsonDocument sessionToJson(const SessionData &data)
{
    QJsonObject window;
    window[QStringLiteral("geometry")] = QString::fromLatin1(data.window.geometry.toBase64());
    window[QStringLiteral("state")] = QString::fromLatin1(data.window.state.toBase64());

    QJsonArray panes;
    for (const PaneSessionState &pane : data.panes)
        panes.append(paneToJson(pane));

    QJsonObject root;
    root[QStringLiteral("window")] = window;
    root[QStringLiteral("paneLayout")] = data.paneLayout;
    root[QStringLiteral("panes")] = panes;
    return QJsonDocument(root);
}

SessionData sessionFromJson(const QJsonObject &root)
{
    SessionData data;

    const QJsonObject window = root.value(QStringLiteral("window")).toObject();
    data.window.geometry = QByteArray::fromBase64(window.value(QStringLiteral("geometry")).toString().toLatin1());
    data.window.state = QByteArray::fromBase64(window.value(QStringLiteral("state")).toString().toLatin1());

    data.paneLayout = root.value(QStringLiteral("paneLayout")).toString();

    const QJsonArray panes = root.value(QStringLiteral("panes")).toArray();
    for (const QJsonValue &value : panes)
        data.panes.append(paneFromJson(value.toObject()));

    return data;
}

QString sessionFilePath(const QString &sessionName)
{
    return SessionManager::sessionsDirectory() + QLatin1Char('/') + sessionName + QStringLiteral(".json");
}

} // namespace

bool SessionManager::isPortableMode()
{
    // Presence of a companion .ini next to the executable is the same
    // portable-mode marker convention described in README.md.
    return QFile::exists(QCoreApplication::applicationDirPath() + QStringLiteral("/lvdExplorer.ini"));
}

QString SessionManager::dataDirectory()
{
    if (isPortableMode())
        return QCoreApplication::applicationDirPath() + QStringLiteral("/lvdExplorer-data");
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString SessionManager::sessionsDirectory()
{
    return dataDirectory() + QStringLiteral("/sessions");
}

bool SessionManager::save(const QString &sessionName, const SessionData &data)
{
    if (!QDir().mkpath(sessionsDirectory()))
        return false;

    QSaveFile file(sessionFilePath(sessionName));
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(sessionToJson(data).toJson(QJsonDocument::Indented));
    return file.commit();
}

bool SessionManager::load(const QString &sessionName, SessionData &outData)
{
    QFile file(sessionFilePath(sessionName));
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    outData = sessionFromJson(doc.object());
    return true;
}

bool SessionManager::exists(const QString &sessionName)
{
    return QFile::exists(sessionFilePath(sessionName));
}

bool SessionManager::remove(const QString &sessionName)
{
    return QFile::remove(sessionFilePath(sessionName));
}

QStringList SessionManager::listNamedSessions()
{
    QDir dir(sessionsDirectory());
    QStringList names;
    for (const QFileInfo &info : dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
        if (info.completeBaseName() != kAutosaveSessionName)
            names.append(info.completeBaseName());
    }
    return names;
}
