#pragma once

#include <QDialog>
#include <QStringList>

class QTableWidget;

// Lets the user view/customize every registered keyboard shortcut
// (ShortcutManager's catalog). Changes apply live via
// QKeySequenceEdit::keySequenceChanged, with a same-sequence conflict
// check before committing.
class ShortcutEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutEditorDialog(QWidget *parent = nullptr);

private slots:
    void handleSequenceChanged(int row);
    void resetRow(int row);
    void resetAll();

private:
    void populate();

    QTableWidget *m_table = nullptr;
    QStringList m_rowIds; // row index -> shortcut id
};
