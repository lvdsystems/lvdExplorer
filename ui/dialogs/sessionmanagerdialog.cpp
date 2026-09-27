#include "ui/dialogs/sessionmanagerdialog.h"

#include "core/session/sessionmanager.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

SessionManagerDialog::SessionManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Sessions"));
    resize(360, 320);

    m_list = new QListWidget(this);

    auto *saveButton = new QPushButton(tr("Save Current As..."), this);
    auto *loadButton = new QPushButton(tr("Load"), this);
    auto *deleteButton = new QPushButton(tr("Delete"), this);
    auto *closeButton = new QPushButton(tr("Close"), this);

    connect(saveButton, &QPushButton::clicked, this, &SessionManagerDialog::handleSaveClicked);
    connect(loadButton, &QPushButton::clicked, this, &SessionManagerDialog::handleLoadClicked);
    connect(deleteButton, &QPushButton::clicked, this, &SessionManagerDialog::handleDeleteClicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &SessionManagerDialog::handleLoadClicked);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(loadButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttonLayout);

    refreshList();
}

void SessionManagerDialog::refreshList()
{
    m_list->clear();
    m_list->addItems(SessionManager::listNamedSessions());
}

void SessionManagerDialog::handleSaveClicked()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Save Session"), tr("Session name:"),
                                                QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    emit saveRequested(name.trimmed());
    refreshList();
}

void SessionManagerDialog::handleLoadClicked()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return;

    emit loadRequested(item->text());
    accept();
}

void SessionManagerDialog::handleDeleteClicked()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return;

    if (QMessageBox::question(this, tr("Delete Session"),
            tr("Delete session “%1”?").arg(item->text())) != QMessageBox::Yes)
        return;

    SessionManager::remove(item->text());
    refreshList();
}
