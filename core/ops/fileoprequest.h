#pragma once

#include <QString>
#include <QStringList>

enum class FileOpKind { Copy, Move, Delete };

// One request submitted to OpsEngine. sourceReadOnly/destReadOnly are
// plain booleans rather than FilePane pointers deliberately: core/ must
// stay UI-agnostic, so the UI layer computes these before submitting and
// OpsEngine::submit() does the actual centralized enforcement 
struct FileOpRequest
{
    FileOpKind kind = FileOpKind::Copy;
    QStringList sourcePaths;
    QString destDir; // Copy/Move only
    bool sourceReadOnly = false;
    bool destReadOnly = false; // Copy/Move only
};
