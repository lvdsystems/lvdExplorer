#pragma once

#include <QDialog>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QCheckBox;
class QFileInfo;
class QSpinBox;
class QTableWidget;

// Batch rename for a multi-selection within one pane (§2 feature list).
// Two modes: token-based Pattern ({name}, {ext}, {n}, {n:3} zero-padded
// counter) and plain/regex Find & Replace on the base name -- mirrors the
// Simple/Regex convention already used by the filter box and Finder panel.
// Renames are applied synchronously: same-directory renames are fast
// metadata operations that don't need OpsEngine's async job queue.
class BatchRenameDialog : public QDialog
{
    Q_OBJECT

public:
    BatchRenameDialog(const QStringList &paths, QWidget *parent = nullptr);

signals:
    // Emitted after Apply performs at least one successful rename, so the
    // caller knows to refresh.
    void renamed();

private slots:
    void updatePreview();
    void apply();

private:
    QString computeNewName(int index, const QFileInfo &info) const;

    QStringList m_paths;

    QComboBox *m_modeCombo = nullptr;
    QLineEdit *m_patternEdit = nullptr;
    QSpinBox *m_startNumberSpin = nullptr;

    QLineEdit *m_findEdit = nullptr;
    QLineEdit *m_replaceEdit = nullptr;
    QCheckBox *m_regexCheck = nullptr;
    QCheckBox *m_caseSensitiveCheck = nullptr;

    QTableWidget *m_previewTable = nullptr;
};
