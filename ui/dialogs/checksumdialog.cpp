#include "ui/dialogs/checksumdialog.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QTableWidget>
#include <QThreadPool>
#include <QVBoxLayout>

namespace {

// Minimal RFC 4180-style escaping: quote the field and double up any
// embedded quotes whenever it contains a comma, quote, or newline.
QString csvField(const QString &value)
{
    if (!value.contains(QLatin1Char(',')) && !value.contains(QLatin1Char('"')) && !value.contains(QLatin1Char('\n')))
        return value;

    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

} // namespace

ChecksumDialog::ChecksumDialog(const QStringList &paths, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Checksums"));
    resize(720, 320);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({tr("File"), tr("MD5"), tr("SHA-1"), tr("SHA-256")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    // Individual cells (not whole rows) are selectable, so a single hash
    // value can be selected and copied on its own rather than only ever
    // the whole row.
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &ChecksumDialog::showTableContextMenu);

    auto *copyCellShortcut = new QShortcut(QKeySequence::Copy, m_table);
    connect(copyCellShortcut, &QShortcut::activated, this, &ChecksumDialog::copyCurrentCell);

    m_statusLabel = new QLabel(tr("Computing…"), this);

    auto *copyRowButton = new QPushButton(tr("Copy Row to Clipboard"), this);
    connect(copyRowButton, &QPushButton::clicked, this, &ChecksumDialog::copySelectedRowToClipboard);

    auto *copyCsvButton = new QPushButton(tr("Copy as CSV"), this);
    connect(copyCsvButton, &QPushButton::clicked, this, &ChecksumDialog::copyTableAsCsv);

    // "Close" rather than "Cancel", and always enabled: Qt::WA_DeleteOnClose
    // (set by whoever opens this dialog) means closing it destroys it,
    // which the destructor below already turns into a cancel if a scan is
    // still running -- so this one button correctly means "stop and
    // dismiss" while computing and "dismiss" once finished, with no need
    // to disable it or relabel it after the fact.
    auto *closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(copyRowButton);
    buttonRow->addWidget(copyCsvButton);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_table, 1);
    layout->addWidget(m_statusLabel);
    layout->addLayout(buttonRow);

    m_cancelFlag = QSharedPointer<QAtomicInt>::create(0);
    auto *task = new ChecksumTask(paths, m_cancelFlag);
    connect(task, &ChecksumTask::resultReady, this, &ChecksumDialog::handleResult);
    connect(task, &ChecksumTask::finished, this, &ChecksumDialog::handleFinished);
    QThreadPool::globalInstance()->start(task);
}

ChecksumDialog::~ChecksumDialog()
{
    if (m_cancelFlag)
        m_cancelFlag->storeRelaxed(1);
}

void ChecksumDialog::handleResult(ChecksumResult result)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto *nameItem = new QTableWidgetItem(result.displayName);
    nameItem->setToolTip(result.path);
    m_table->setItem(row, 0, nameItem);
    m_table->setItem(row, 1, new QTableWidgetItem(result.ok ? result.md5 : tr("(error)")));
    m_table->setItem(row, 2, new QTableWidgetItem(result.ok ? result.sha1 : QString()));
    m_table->setItem(row, 3, new QTableWidgetItem(result.ok ? result.sha256 : QString()));
}

void ChecksumDialog::handleFinished(bool wasCancelled)
{
    m_statusLabel->setText(wasCancelled ? tr("Cancelled.") : tr("Done: %1 file(s).").arg(m_table->rowCount()));
}

void ChecksumDialog::copySelectedRowToClipboard()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;

    const QString text = QStringLiteral("%1\nMD5: %2\nSHA-1: %3\nSHA-256: %4")
        .arg(m_table->item(row, 0)->text(), m_table->item(row, 1)->text(),
             m_table->item(row, 2)->text(), m_table->item(row, 3)->text());
    QGuiApplication::clipboard()->setText(text);
}

void ChecksumDialog::copyCurrentCell()
{
    const QTableWidgetItem *item = m_table->currentItem();
    if (item)
        QGuiApplication::clipboard()->setText(item->text());
}

void ChecksumDialog::copyTableAsCsv()
{
    QStringList lines;

    QStringList header;
    for (int col = 0; col < m_table->columnCount(); ++col)
        header << csvField(m_table->horizontalHeaderItem(col)->text());
    lines << header.join(QLatin1Char(','));

    for (int row = 0; row < m_table->rowCount(); ++row) {
        QStringList fields;
        for (int col = 0; col < m_table->columnCount(); ++col) {
            const QTableWidgetItem *item = m_table->item(row, col);
            fields << csvField(item ? item->text() : QString());
        }
        lines << fields.join(QLatin1Char(','));
    }

    QGuiApplication::clipboard()->setText(lines.join(QStringLiteral("\n")));
}

void ChecksumDialog::showTableContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = m_table->itemAt(pos);
    if (!item)
        return;
    m_table->setCurrentItem(item);

    QMenu menu(this);
    QAction *copyCellAction = menu.addAction(tr("Copy"));
    QAction *copyRowAction = menu.addAction(tr("Copy Row"));
    QAction *chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (chosen == copyCellAction)
        copyCurrentCell();
    else if (chosen == copyRowAction)
        copySelectedRowToClipboard();
}
