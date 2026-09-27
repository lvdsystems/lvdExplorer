#pragma once

#include <QPoint>
#include <QString>
#include <QStringList>

class QMimeData;
class QWidget;

// The one place OS-specific shell integration lives
class IPlatformShell
{
public:
    virtual ~IPlatformShell() = default;

    virtual void openTerminalHere(const QString &path) = 0;

    virtual void showProperties(const QStringList &paths, QWidget *parentWidget) = 0;

    // Returns false if native shell context menus aren't available on this
    // platform/build (Linux v1, per §7's documented scope decision -- no
    // cross-desktop-environment equivalent exists). The caller's own menu
    // is then all there is.
    virtual bool showNativeContextMenu(const QStringList &paths, const QString &parentDir,
                                        const QPoint &globalPos, QWidget *parentWidget) = 0;

    // Stamps mimeData with this platform's own "this was a Cut, not a
    // Copy" hint (Windows: CFSTR_PREFERREDDROPEFFECT; Linux: the GNOME
    // clipboard convention honored by most file managers), so a Cut made
    // in lvdExplorer still moves when pasted into the system file manager,
    // and a Cut made there is recognized as a move when pasted back here.
    virtual void markCutMimeData(QMimeData *mimeData, bool cut) const = 0;
    virtual bool isCutMimeData(const QMimeData *mimeData) const = 0;
};
