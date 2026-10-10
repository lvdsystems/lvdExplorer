#pragma once

#include <QString>
#include <QStringList>

// One user-defined context-menu action. The framework currently covers two
// kinds; CustomActionsDialog lets the user add as many of each as they
// want, so this stays a flat struct with kind-specific fields left empty
// for the other kind, rather than a class hierarchy for just two shapes.
struct CustomAction
{
    enum class Kind {
        NewFile, // adds an entry to the pane's "New" menu
        OpenWith // adds an "Open With" entry for files whose extension matches
    };

    QString id; // stable (QUuid-derived), survives reordering/renaming
    Kind kind = Kind::NewFile;
    QString name; // shown in the menu
    bool enabled = true;

    // Kind::NewFile
    QString newFileName; // e.g. "New Text File.txt"

    // Kind::OpenWith
    QStringList extensions; // lower-case, no leading dot, e.g. {"zip", "7z"}
    QString programPath;
    QString arguments; // "%1" is replaced with the file path; empty means "just the path"
};
