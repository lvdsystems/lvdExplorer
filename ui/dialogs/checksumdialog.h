#pragma once

#include "core/ops/checksumtask.h"

#include <QAtomicInt>
#include <QDialog>
#include <QPoint>
#include <QSharedPointer>
#include <QStringList>

class QLabel;
class QPushButton;
class QTableWidget;

// Computes and displays MD5/SHA-1/SHA-256 for a selection. Runs its own
// ChecksumTask directly on QThreadPool rather than through a shared
// engine -- unlike file ops, checksum requests aren't something that
// needs to queue behind other work app-wide, and this dialog only ever
// has one task in flight tied to its own lifetime. A folder in the
// selection is hashed recursively (every file it contains), not skipped.
class ChecksumDialog : public QDialog
{
    Q_OBJECT

public:
    ChecksumDialog(const QStringList &paths, QWidget *parent = nullptr);
    ~ChecksumDialog() override;

private slots:
    void handleResult(ChecksumResult result);
    void handleFinished(bool wasCancelled);
    void copySelectedRowToClipboard();
    void copyCurrentCell();
    void copyTableAsCsv();
    void showTableContextMenu(const QPoint &pos);

private:
    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QSharedPointer<QAtomicInt> m_cancelFlag;
};
