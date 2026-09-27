#pragma once

#include <QDialog>

class QListWidget;

// The "session manager UI". Lists named
// sessions (the autosave slot is deliberately excluded) and lets the user
// save the live layout under a new name, load one, or delete one. Actually
// capturing/applying live UI state is left to MainWindow, which connects
// to saveRequested()/loadRequested() -- this dialog only knows about names.
class SessionManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SessionManagerDialog(QWidget *parent = nullptr);

signals:
    void saveRequested(const QString &name);
    void loadRequested(const QString &name);

private slots:
    void refreshList();
    void handleSaveClicked();
    void handleLoadClicked();
    void handleDeleteClicked();

private:
    QListWidget *m_list = nullptr;
};
