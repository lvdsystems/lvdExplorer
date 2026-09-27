#include "ui/dialogs/checksumdialog.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QThreadPool>
#include <QVBoxLayout>

ChecksumDialog::ChecksumDialog(const QStringList &paths, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Checksums"));
    resize(720, 320);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({tr("File"), tr("MD5"), tr("SHA-1"), tr("SHA-256")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_statusLabel = new QLabel(tr("Computing…"), this);

    auto *copyButton = new QPushButton(tr("Copy Row to Clipboard"), this);
    connect(copyButton, &QPushButton::clicked, this, &ChecksumDialog::copySelectedToClipboard);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, [this] {
        if (m_cancelFlag)
            m_cancelFlag->storeRelaxed(1);
    });

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(copyButton);
    buttonRow->addStretch();
    buttonRow->addWidget(m_cancelButton);

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
    m_table->setItem(row, 0, new QTableWidgetItem(QFileInfo(result.path).fileName()));
    m_table->setItem(row, 1, new QTableWidgetItem(result.ok ? result.md5 : tr("(error)")));
    m_table->setItem(row, 2, new QTableWidgetItem(result.ok ? result.sha1 : QString()));
    m_table->setItem(row, 3, new QTableWidgetItem(result.ok ? result.sha256 : QString()));
}

void ChecksumDialog::handleFinished(bool wasCancelled)
{
    m_cancelButton->setEnabled(false);
    m_statusLabel->setText(wasCancelled ? tr("Cancelled.") : tr("Done: %1 file(s).").arg(m_table->rowCount()));
}

void ChecksumDialog::copySelectedToClipboard()
{
    const auto selected = m_table->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return;

    const int row = selected.first().row();
    const QString text = QStringLiteral("%1\nMD5: %2\nSHA-1: %3\nSHA-256: %4")
        .arg(m_table->item(row, 0)->text(), m_table->item(row, 1)->text(),
             m_table->item(row, 2)->text(), m_table->item(row, 3)->text());
    QGuiApplication::clipboard()->setText(text);
}
