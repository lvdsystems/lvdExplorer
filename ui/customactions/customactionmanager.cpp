#include "ui/customactions/customactionmanager.h"

#include <QFileInfo>
#include <QSettings>
#include <QUuid>

namespace {
QString settingsGroup()
{
    return QStringLiteral("customActions");
}
} // namespace

CustomActionManager &CustomActionManager::instance()
{
    static CustomActionManager manager;
    return manager;
}

CustomActionManager::CustomActionManager()
{
    load();
}

void CustomActionManager::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    save();
    emit changed();
}

QList<CustomAction> CustomActionManager::newFileActions() const
{
    QList<CustomAction> result;
    for (const CustomAction &action : m_actions) {
        if (action.enabled && action.kind == CustomAction::Kind::NewFile)
            result.append(action);
    }
    return result;
}

QList<CustomAction> CustomActionManager::openWithActionsFor(const QString &filePath) const
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    QList<CustomAction> result;
    if (suffix.isEmpty())
        return result;
    for (const CustomAction &action : m_actions) {
        if (action.enabled && action.kind == CustomAction::Kind::OpenWith && action.extensions.contains(suffix))
            result.append(action);
    }
    return result;
}

void CustomActionManager::addOrUpdate(const CustomAction &action)
{
    CustomAction toStore = action;
    if (toStore.id.isEmpty())
        toStore.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    bool replaced = false;
    for (CustomAction &existing : m_actions) {
        if (existing.id == toStore.id) {
            existing = toStore;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        m_actions.append(toStore);

    save();
    emit changed();
}

void CustomActionManager::remove(const QString &id)
{
    const qsizetype removed = m_actions.removeIf([&](const CustomAction &a) { return a.id == id; });
    if (removed > 0) {
        save();
        emit changed();
    }
}

void CustomActionManager::load()
{
    QSettings settings;
    settings.beginGroup(settingsGroup());

    const bool initialized = settings.value(QStringLiteral("initialized"), false).toBool();
    m_enabled = settings.value(QStringLiteral("masterEnabled"), true).toBool();

    const int count = settings.beginReadArray(QStringLiteral("list"));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        CustomAction action;
        action.id = settings.value(QStringLiteral("id")).toString();
        action.kind = settings.value(QStringLiteral("kind")).toInt() == static_cast<int>(CustomAction::Kind::OpenWith)
            ? CustomAction::Kind::OpenWith
            : CustomAction::Kind::NewFile;
        action.name = settings.value(QStringLiteral("name")).toString();
        action.enabled = settings.value(QStringLiteral("enabled"), true).toBool();
        action.newFileName = settings.value(QStringLiteral("newFileName")).toString();
        action.extensions = settings.value(QStringLiteral("extensions")).toStringList();
        action.programPath = settings.value(QStringLiteral("programPath")).toString();
        action.arguments = settings.value(QStringLiteral("arguments")).toString();
        if (!action.id.isEmpty())
            m_actions.append(action);
    }
    settings.endArray();
    settings.endGroup();

    if (!initialized)
        seedDefaults();
}

void CustomActionManager::seedDefaults()
{
    // The one action the context menu ships with: matches Explorer's
    // "New > Text Document" for the common case of wanting a blank file.
    CustomAction newTextFile;
    newTextFile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    newTextFile.kind = CustomAction::Kind::NewFile;
    newTextFile.name = tr("New Text File.txt");
    newTextFile.newFileName = QStringLiteral("New Text File.txt");
    m_actions.append(newTextFile);
    save();
}

void CustomActionManager::save()
{
    QSettings settings;
    settings.beginGroup(settingsGroup());
    settings.setValue(QStringLiteral("initialized"), true);
    settings.setValue(QStringLiteral("masterEnabled"), m_enabled);

    settings.beginWriteArray(QStringLiteral("list"));
    for (int i = 0; i < m_actions.size(); ++i) {
        settings.setArrayIndex(i);
        const CustomAction &action = m_actions.at(i);
        settings.setValue(QStringLiteral("id"), action.id);
        settings.setValue(QStringLiteral("kind"), static_cast<int>(action.kind));
        settings.setValue(QStringLiteral("name"), action.name);
        settings.setValue(QStringLiteral("enabled"), action.enabled);
        settings.setValue(QStringLiteral("newFileName"), action.newFileName);
        settings.setValue(QStringLiteral("extensions"), action.extensions);
        settings.setValue(QStringLiteral("programPath"), action.programPath);
        settings.setValue(QStringLiteral("arguments"), action.arguments);
    }
    settings.endArray();
    settings.endGroup();
}
