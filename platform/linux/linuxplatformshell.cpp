#include "platform/linux/linuxplatformshell.h"

#include <QDBusInterface>
#include <QDBusMessage>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QMimeData>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace {

struct TerminalCandidate
{
    QString exeName;
    QStringList extraArgs;
    bool passWorkingDir = false;
};

// Backstop for when no desktop-environment file manager answers on the
// bus (minimal window managers, some sandboxes): a small in-app dialog
// with the basics, per §7's documented fallback design.
void showFallbackPropertiesDialog(const QStringList &paths, QWidget *parentWidget)
{
    auto *dialog = new QDialog(parentWidget);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QObject::tr("Properties"));

    auto *layout = new QVBoxLayout(dialog);
    for (const QString &path : paths) {
        const QFileInfo info(path);
        auto *form = new QFormLayout;
        form->addRow(QObject::tr("Name:"), new QLabel(info.fileName(), dialog));
        form->addRow(QObject::tr("Location:"), new QLabel(info.absolutePath(), dialog));
        form->addRow(QObject::tr("Size:"),
                      new QLabel(info.isDir() ? QObject::tr("(folder)") : QLocale::system().formattedDataSize(info.size()),
                                 dialog));
        form->addRow(QObject::tr("Modified:"),
                      new QLabel(QLocale::system().toString(info.lastModified(), QLocale::ShortFormat), dialog));
        layout->addLayout(form);
        if (paths.size() > 1)
            layout->addSpacing(8);
    }

    auto *closeButton = new QPushButton(QObject::tr("Close"), dialog);
    QObject::connect(closeButton, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeButton, 0, Qt::AlignRight);

    dialog->show();
}

} // namespace

void LinuxPlatformShell::openTerminalHere(const QString &path)
{
    QVector<TerminalCandidate> candidates;
    const QByteArray envTerminal = qgetenv("TERMINAL");
    if (!envTerminal.isEmpty())
        candidates.append({QString::fromLocal8Bit(envTerminal), {}, true});
    candidates.append({QStringLiteral("gnome-terminal"), {QStringLiteral("--working-directory=") + path}, false});
    candidates.append({QStringLiteral("konsole"), {QStringLiteral("--workdir"), path}, false});
    candidates.append({QStringLiteral("xfce4-terminal"), {QStringLiteral("--working-directory=") + path}, false});
    candidates.append({QStringLiteral("x-terminal-emulator"), {}, true});
    candidates.append({QStringLiteral("xterm"), {}, true});

    for (const auto &candidate : candidates) {
        const QString exe = QStandardPaths::findExecutable(candidate.exeName);
        if (exe.isEmpty())
            continue;
        QProcess::startDetached(exe, candidate.extraArgs, candidate.passWorkingDir ? path : QString());
        return;
    }
}

void LinuxPlatformShell::showProperties(const QStringList &paths, QWidget *parentWidget)
{
    QStringList uris;
    for (const QString &path : paths)
        uris << QUrl::fromLocalFile(path).toString();

    QDBusInterface fileManager(QStringLiteral("org.freedesktop.FileManager1"),
                                QStringLiteral("/org/freedesktop/FileManager1"),
                                QStringLiteral("org.freedesktop.FileManager1"));
    if (fileManager.isValid()) {
        const QDBusMessage reply = fileManager.call(QStringLiteral("ShowItemProperties"), uris, QString());
        if (reply.type() != QDBusMessage::ErrorMessage)
            return;
    }

    showFallbackPropertiesDialog(paths, parentWidget);
}

bool LinuxPlatformShell::showNativeContextMenu(const QStringList &, const QString &, const QPoint &, QWidget *)
{
    // Unsupported for now. The
    // app-native menu tier is the only one shown on Linux.
    return false;
}

void LinuxPlatformShell::markCutMimeData(QMimeData *mimeData, bool cut) const
{
    // The GNOME/Nautilus clipboard convention, also honored by KDE's
    // Dolphin and most other Linux file managers: a "x-special/gnome-
    // copied-files" entry of "cut\n" or "copy\n" followed by one file://
    // URI per line. Following it means a Cut here still moves when pasted
    // into the system file manager, and vice versa.
    QByteArray payload = cut ? "cut\n" : "copy\n";
    for (const QUrl &url : mimeData->urls())
        payload += url.toString().toUtf8() + '\n';
    mimeData->setData(QStringLiteral("x-special/gnome-copied-files"), payload);
}

bool LinuxPlatformShell::isCutMimeData(const QMimeData *mimeData) const
{
    return mimeData->data(QStringLiteral("x-special/gnome-copied-files")).startsWith("cut");
}
