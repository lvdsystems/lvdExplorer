#include "ui/dialogs/progressdialog.h"

#include "core/ops/fileophandle.h"
#include "core/ops/opsengine.h"

#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

ProgressDialog::ProgressDialog(const QString &title, FileOpHandle *handle, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(360, 120);

    m_itemLabel = new QLabel(tr("Starting…"), this);
    m_itemLabel->setWordWrap(true);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0); // indeterminate until the first progress signal arrives

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, [] { OpsEngine::instance().cancelCurrent(); });

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_itemLabel);
    layout->addWidget(m_progressBar);
    layout->addWidget(m_cancelButton, 0, Qt::AlignRight);

    connect(handle, &FileOpHandle::progress, this, &ProgressDialog::handleProgress);
    connect(handle, &FileOpHandle::finished, this, &ProgressDialog::handleFinished);

    show();
}

void ProgressDialog::handleProgress(int itemsDone, int itemsTotal, const QString &currentItem)
{
    if (itemsTotal > 0) {
        m_progressBar->setRange(0, itemsTotal);
        m_progressBar->setValue(itemsDone);
    }
    m_itemLabel->setText(currentItem);
}

void ProgressDialog::handleFinished(bool wasCancelled, const QStringList &failedItems)
{
    m_cancelButton->setEnabled(false);

    if (!failedItems.isEmpty()) {
        QMessageBox::warning(this, windowTitle(),
            tr("Some items could not be processed:\n%1").arg(failedItems.join(QLatin1Char('\n'))));
    } else if (wasCancelled) {
        // Brief visible confirmation before closing rather than vanishing.
        m_itemLabel->setText(tr("Cancelled."));
        QTimer::singleShot(800, this, &QDialog::close);
        return;
    }

    close();
}
