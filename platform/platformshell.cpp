#include "platform/platformshell.h"

#if defined(Q_OS_WIN)
#include "platform/win/windowsplatformshell.h"
#elif defined(Q_OS_LINUX)
#include "platform/linux/linuxplatformshell.h"
#else
namespace {
class NullPlatformShell : public IPlatformShell
{
public:
    void openTerminalHere(const QString &) override {}
    void showProperties(const QStringList &, QWidget *) override {}
    bool showNativeContextMenu(const QStringList &, const QString &, const QPoint &, QWidget *) override
    {
        return false;
    }
};
} // namespace
#endif

namespace PlatformShell {

IPlatformShell &instance()
{
#if defined(Q_OS_WIN)
    static WindowsPlatformShell shell;
#elif defined(Q_OS_LINUX)
    static LinuxPlatformShell shell;
#else
    static NullPlatformShell shell;
#endif
    return shell;
}

} // namespace PlatformShell
