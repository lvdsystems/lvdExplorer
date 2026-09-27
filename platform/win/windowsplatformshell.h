#pragma once

#include "platform/iplatformshell.h"

class WindowsPlatformShell : public IPlatformShell
{
public:
    void openTerminalHere(const QString &path) override;
    void showProperties(const QStringList &paths, QWidget *parentWidget) override;
    bool showNativeContextMenu(const QStringList &paths, const QString &parentDir, const QPoint &globalPos,
                                QWidget *parentWidget) override;
    void markCutMimeData(QMimeData *mimeData, bool cut) const override;
    bool isCutMimeData(const QMimeData *mimeData) const override;
};
