#pragma once

#include "ui/customactions/customaction.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QStackedWidget;

// Add/edit form for one CustomAction. Kind picks which page of
// kind-specific fields is shown, same QStackedWidget idiom as
// BatchRenameDialog's mode switch.
class EditCustomActionDialog : public QDialog
{
    Q_OBJECT

public:
    // action.id empty means "new action".
    explicit EditCustomActionDialog(const CustomAction &action, QWidget *parent = nullptr);

    CustomAction result() const;

private slots:
    void handleAccept();
    void browseForProgram();

private:
    CustomAction m_original;

    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_kindCombo = nullptr;
    QStackedWidget *m_stack = nullptr;

    QLineEdit *m_newFileNameEdit = nullptr;

    QLineEdit *m_extensionsEdit = nullptr;
    QLineEdit *m_programEdit = nullptr;
    QLineEdit *m_argumentsEdit = nullptr;
};
