#include "ui/dialogs/customactionsdialog.h"

#include "ui/customactions/customactionmanager.h"
#include "ui/dialogs/editcustomactiondialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString kindLabel(CustomAction::Kind kind)
{
    return kind == CustomAction::Kind::NewFile ? CustomActionsDialog::tr("New File")
                                                : CustomActionsDialog::tr("Open With");
}

QString detailsFor(const CustomAction &action)
{
    return action.kind == CustomAction::Kind::NewFile
        ? action.newFileName
        : CustomActionsDialog::tr("*.%1 → %2").arg(action.extensions.join(QStringLiteral(", *."))).arg(action.programPath);
}
} // namespace

CustomActionsDialog::CustomActionsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Custom Actions"));
    resize(560, 360);

    m_enabledCheck = new QCheckBox(tr("Enable custom actions"), this);
    m_enabledCheck->setChecked(CustomActionManager::instance().isEnabled());
    m_enabledCheck->setToolTip(
        tr("Adds a \"New\" submenu and, for matching files, \"Open With\" entries to the right-click menu."));
    connect(m_enabledCheck, &QCheckBox::toggled, this,
            [](bool checked) { CustomActionManager::instance().setEnabled(checked); });

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Kind"), tr("Details")});
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *addButton = new QPushButton(tr("Add…"), this);
    auto *editButton = new QPushButton(tr("Edit…"), this);
    auto *removeButton = new QPushButton(tr("Remove"), this);
    connect(addButton, &QPushButton::clicked, this, &CustomActionsDialog::addAction);
    connect(editButton, &QPushButton::clicked, this, &CustomActionsDialog::editSelected);
    connect(removeButton, &QPushButton::clicked, this, &CustomActionsDialog::removeSelected);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, &CustomActionsDialog::editSelected);

    auto *buttonColumn = new QVBoxLayout;
    buttonColumn->addWidget(addButton);
    buttonColumn->addWidget(editButton);
    buttonColumn->addWidget(removeButton);
    buttonColumn->addStretch();

    auto *tableRow = new QHBoxLayout;
    tableRow->addWidget(m_table, 1);
    tableRow->addLayout(buttonColumn);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_enabledCheck);
    layout->addLayout(tableRow, 1);
    layout->addWidget(buttons);

    connect(&CustomActionManager::instance(), &CustomActionManager::changed, this, &CustomActionsDialog::populate);
    populate();
}

void CustomActionsDialog::populate()
{
    const QList<CustomAction> actions = CustomActionManager::instance().actions();
    m_table->setRowCount(actions.size());
    m_rowIds.clear();

    for (int row = 0; row < actions.size(); ++row) {
        const CustomAction &action = actions.at(row);
        m_rowIds.append(action.id);
        m_table->setItem(row, 0, new QTableWidgetItem(action.name));
        m_table->setItem(row, 1, new QTableWidgetItem(kindLabel(action.kind)));
        m_table->setItem(row, 2, new QTableWidgetItem(detailsFor(action)));
    }
}

int CustomActionsDialog::selectedRow() const
{
    const auto selected = m_table->selectionModel()->selectedRows();
    return selected.isEmpty() ? -1 : selected.first().row();
}

void CustomActionsDialog::addAction()
{
    EditCustomActionDialog dialog(CustomAction{}, this);
    if (dialog.exec() == QDialog::Accepted)
        CustomActionManager::instance().addOrUpdate(dialog.result());
}

void CustomActionsDialog::editSelected()
{
    const int row = selectedRow();
    if (row < 0 || row >= m_rowIds.size())
        return;

    const QString id = m_rowIds.at(row);
    for (const CustomAction &action : CustomActionManager::instance().actions()) {
        if (action.id != id)
            continue;
        EditCustomActionDialog dialog(action, this);
        if (dialog.exec() == QDialog::Accepted)
            CustomActionManager::instance().addOrUpdate(dialog.result());
        break;
    }
}

void CustomActionsDialog::removeSelected()
{
    const int row = selectedRow();
    if (row < 0 || row >= m_rowIds.size())
        return;
    CustomActionManager::instance().remove(m_rowIds.at(row));
}
