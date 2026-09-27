#pragma once

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;
class FileOpHandle;

// Non-modal progress UI for one OpsEngine submission. Non-modal deliberately: a copy/move/delete running in
// the background must not block browsing in other panes, consistent with
// every other async operation in this app.
class ProgressDialog : public QDialog
{
    Q_OBJECT

public:
    // title is shown as the window title (e.g. "Copying", "Moving",
    // "Deleting"); the dialog wires itself to handle's signals and deletes
    // itself once the job finishes.
    ProgressDialog(const QString &title, FileOpHandle *handle, QWidget *parent = nullptr);

private slots:
    void handleProgress(int itemsDone, int itemsTotal, const QString &currentItem);
    void handleFinished(bool wasCancelled, const QStringList &failedItems);

private:
    QLabel *m_itemLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPushButton *m_cancelButton = nullptr;
};
