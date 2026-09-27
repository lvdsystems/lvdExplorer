#include "platform/win/windowsplatformshell.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>
#include <wrl/client.h>

#include <QDir>
#include <QMimeData>
#include <QProcess>
#include <QStandardPaths>
#include <QWidget>

#include <cstring>
#include <memory>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

struct CoTaskMemDeleter
{
    void operator()(void *p) const
    {
        if (p)
            CoTaskMemFree(p);
    }
};
using ItemIdListPtr = std::unique_ptr<ITEMIDLIST, CoTaskMemDeleter>;

// Binds the shared parent IShellFolder for a set of same-directory paths
// and returns an IContextMenu covering all of them, via the standard
// SHParseDisplayName + SHBindToParent + IShellFolder::GetUIObjectOf
// sequence. pidlStorage must outlive contextMenu's use: GetUIObjectOf's
// result holds references into that PIDL memory.
bool buildContextMenu(const QStringList &absolutePaths, ComPtr<IContextMenu> &contextMenu,
                       std::vector<ItemIdListPtr> &pidlStorage)
{
    if (absolutePaths.isEmpty())
        return false;

    ComPtr<IShellFolder> parentFolder;

    for (const QString &path : absolutePaths) {
        const std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();

        ITEMIDLIST *rawFullPidl = nullptr;
        if (FAILED(SHParseDisplayName(wpath.c_str(), nullptr, &rawFullPidl, 0, nullptr)) || !rawFullPidl)
            return false;
        ItemIdListPtr fullPidl(rawFullPidl);

        ComPtr<IShellFolder> folder;
        PCUITEMID_CHILD childRelative = nullptr;
        if (FAILED(SHBindToParent(fullPidl.get(), IID_PPV_ARGS(&folder), &childRelative)))
            return false;

        if (!parentFolder)
            parentFolder = folder;

        auto *clonedChild = ILCloneChild(childRelative);
        if (!clonedChild)
            return false;
        pidlStorage.emplace_back(static_cast<ITEMIDLIST *>(clonedChild));
    }

    std::vector<PCITEMID_CHILD> children;
    children.reserve(pidlStorage.size());
    for (const auto &pidl : pidlStorage)
        children.push_back(static_cast<PCITEMID_CHILD>(pidl.get()));

    const HRESULT hr = parentFolder->GetUIObjectOf(nullptr, static_cast<UINT>(children.size()), children.data(),
                                                    IID_IContextMenu, nullptr,
                                                    reinterpret_cast<void **>(contextMenu.GetAddressOf()));
    return SUCCEEDED(hr);
}

void showPropertiesForSingle(const QString &path, HWND owner)
{
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();

    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_INVOKEIDLIST;
    info.hwnd = owner;
    info.lpVerb = L"properties";
    info.lpFile = nativePath.c_str();
    info.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&info);
}

} // namespace

void WindowsPlatformShell::openTerminalHere(const QString &path)
{
    const QString nativePath = QDir::toNativeSeparators(path);

    // Prefer Windows Terminal when it's on PATH (the App Execution Alias
    // registers wt.exe there automatically for Store installs).
    const QString windowsTerminal = QStandardPaths::findExecutable(QStringLiteral("wt.exe"));
    if (!windowsTerminal.isEmpty()) {
        QProcess::startDetached(windowsTerminal, {QStringLiteral("-d"), nativePath});
        return;
    }

    QProcess::startDetached(QStringLiteral("cmd.exe"),
                             {QStringLiteral("/K"), QStringLiteral("cd /d \"%1\"").arg(nativePath)});
}

void WindowsPlatformShell::showProperties(const QStringList &paths, QWidget *parentWidget)
{
    if (paths.isEmpty())
        return;

    const HWND owner = parentWidget ? reinterpret_cast<HWND>(parentWidget->winId()) : nullptr;

    if (paths.size() == 1) {
        showPropertiesForSingle(paths.first(), owner);
        return;
    }

    // A combined multi-select Properties dialog needs the IContextMenu
    // path (ShellExecuteEx's "properties" verb only takes one lpFile).
    ComPtr<IContextMenu> contextMenu;
    std::vector<ItemIdListPtr> pidlStorage;
    if (buildContextMenu(paths, contextMenu, pidlStorage)) {
        CMINVOKECOMMANDINFO invoke = {};
        invoke.cbSize = sizeof(invoke);
        invoke.hwnd = owner;
        invoke.lpVerb = "properties";
        invoke.nShow = SW_SHOWNORMAL;
        if (SUCCEEDED(contextMenu->InvokeCommand(&invoke)))
            return;
    }

    // Fallback: one dialog per item if the combined invocation above
    // didn't pan out for some reason.
    for (const QString &path : paths)
        showPropertiesForSingle(path, owner);
}

bool WindowsPlatformShell::showNativeContextMenu(const QStringList &paths, const QString &parentDir,
                                                  const QPoint &globalPos, QWidget *parentWidget)
{
    Q_UNUSED(parentDir);
    if (paths.isEmpty())
        return false;

    ComPtr<IContextMenu> contextMenu;
    std::vector<ItemIdListPtr> pidlStorage;
    if (!buildContextMenu(paths, contextMenu, pidlStorage))
        return false;

    HMENU hmenu = CreatePopupMenu();
    if (!hmenu)
        return false;

    if (FAILED(contextMenu->QueryContextMenu(hmenu, 0, 1, 0x7FFF, CMF_NORMAL))) {
        DestroyMenu(hmenu);
        return false;
    }

    const HWND hwnd = parentWidget ? reinterpret_cast<HWND>(parentWidget->winId()) : GetActiveWindow();
    // TrackPopupMenuEx needs the owner to be the foreground window for the
    // menu to dismiss correctly on an outside click.
    SetForegroundWindow(hwnd);

    const int cmd = TrackPopupMenuEx(hmenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, globalPos.x(), globalPos.y(), hwnd,
                                      nullptr);
    DestroyMenu(hmenu);

    if (cmd <= 0)
        return true; // shown and dismissed with no selection -- still "handled"

    // Menu ids below 0x8000 are ordinal-verb offsets from idCmdFirst (1
    // here), per IContextMenu::InvokeCommand's documented CMINVOKECOMMANDINFO
    // verb rules: MAKEINTRESOURCE with a zero HIWORD means "verb by ordinal".
    CMINVOKECOMMANDINFOEX invoke = {};
    invoke.cbSize = sizeof(invoke);
    invoke.fMask = CMIC_MASK_UNICODE;
    invoke.hwnd = hwnd;
    invoke.lpVerb = MAKEINTRESOURCEA(cmd - 1);
    invoke.lpVerbW = MAKEINTRESOURCEW(cmd - 1);
    invoke.nShow = SW_SHOWNORMAL;
    contextMenu->InvokeCommand(reinterpret_cast<CMINVOKECOMMANDINFO *>(&invoke));
    return true;
}

void WindowsPlatformShell::markCutMimeData(QMimeData *mimeData, bool cut) const
{
    // CFSTR_PREFERREDDROPEFFECT: the format Explorer (and most other
    // Windows shell apps) reads on paste to tell a Cut apart from a Copy.
    // DROPEFFECT_COPY = 1, DROPEFFECT_MOVE = 2.
    const quint32 effect = cut ? 2u : 1u;
    mimeData->setData(QStringLiteral("Preferred DropEffect"),
                       QByteArray(reinterpret_cast<const char *>(&effect), sizeof(effect)));
}

bool WindowsPlatformShell::isCutMimeData(const QMimeData *mimeData) const
{
    const QByteArray bytes = mimeData->data(QStringLiteral("Preferred DropEffect"));
    if (bytes.size() < static_cast<int>(sizeof(quint32)))
        return false;
    quint32 effect = 0;
    memcpy(&effect, bytes.constData(), sizeof(effect));
    return effect == 2u;
}
