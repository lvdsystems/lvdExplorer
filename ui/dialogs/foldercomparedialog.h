#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;
class QTableWidget;

// Top-level folder diff by name/size/date (§2 feature list). Prefilled
// with two paths when available (e.g. the two most recently focused
// distinct panes) but always editable/browsable, so it works from any
// pane layout, not just a 2-pane one.
class FolderCompareDialog : public QDialog
{
    Q_OBJECT

public:
    FolderCompareDialog(const QString &folderA, const QString &folderB, QWidget *parent = nullptr);

private slots:
    void browseA();
    void browseB();
    void compare();

private:
    QLineEdit *m_folderAEdit = nullptr;
    QLineEdit *m_folderBEdit = nullptr;
    QTableWidget *m_resultsTable = nullptr;
};
