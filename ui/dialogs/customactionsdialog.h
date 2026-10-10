#pragma once

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QTableWidget;

// Tools > Custom Actions...: the master enable toggle plus an editable list
// of CustomActionManager's actions. Edits apply immediately through the
// manager (same pattern as ShortcutEditorDialog), so there's no separate
// Apply/Cancel to reconcile.
class CustomActionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomActionsDialog(QWidget *parent = nullptr);

private slots:
    void addAction();
    void editSelected();
    void removeSelected();

private:
    void populate();
    int selectedRow() const;

    QCheckBox *m_enabledCheck = nullptr;
    QTableWidget *m_table = nullptr;
    QStringList m_rowIds; // row index -> CustomAction id
};
