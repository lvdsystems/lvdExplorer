#include "ui/dialogs/shortcuteditordialog.h"

#include "ui/shortcuts/shortcutmanager.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

ShortcutEditorDialog::ShortcutEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    resize(520, 420);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({tr("Action"), tr("Shortcut"), QString()});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);

    auto *resetAllButton = new QPushButton(tr("Reset All to Defaults"), this);
    connect(resetAllButton, &QPushButton::clicked, this, &ShortcutEditorDialog::resetAll);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->addWidget(resetAllButton);
    bottomRow->addStretch();
    bottomRow->addWidget(buttons);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_table, 1);
    layout->addLayout(bottomRow);

    populate();
}

void ShortcutEditorDialog::populate()
{
    m_table->setRowCount(0);
    m_rowIds.clear();

    const QList<ShortcutManager::Entry> entries = ShortcutManager::instance().allEntries();
    m_table->setRowCount(entries.size());

    for (int row = 0; row < entries.size(); ++row) {
        const ShortcutManager::Entry &entry = entries.at(row);
        m_rowIds.append(entry.id);

        m_table->setItem(row, 0, new QTableWidgetItem(entry.description));

        auto *editor = new QKeySequenceEdit(entry.currentSequence, m_table);
        connect(editor, &QKeySequenceEdit::editingFinished, this, [this, row] { handleSequenceChanged(row); });
        m_table->setCellWidget(row, 1, editor);

        auto *resetButton = new QPushButton(tr("Reset"), m_table);
        connect(resetButton, &QPushButton::clicked, this, [this, row] { resetRow(row); });
        m_table->setCellWidget(row, 2, resetButton);
    }
}

void ShortcutEditorDialog::handleSequenceChanged(int row)
{
    if (row < 0 || row >= m_rowIds.size())
        return;

    const QString id = m_rowIds.at(row);
    auto *editor = qobject_cast<QKeySequenceEdit *>(m_table->cellWidget(row, 1));
    if (!editor)
        return;

    const QKeySequence sequence = editor->keySequence();
    if (ShortcutManager::instance().isInUseByOther(id, sequence)) {
        QMessageBox::warning(this, tr("Keyboard Shortcuts"),
            tr("That shortcut is already used by another action. Choose a different one."));
        editor->setKeySequence(ShortcutManager::instance().shortcutFor(id));
        return;
    }

    ShortcutManager::instance().setShortcut(id, sequence);
}

void ShortcutEditorDialog::resetRow(int row)
{
    if (row < 0 || row >= m_rowIds.size())
        return;
    const QString id = m_rowIds.at(row);
    ShortcutManager::instance().resetToDefault(id);
    if (auto *editor = qobject_cast<QKeySequenceEdit *>(m_table->cellWidget(row, 1)))
        editor->setKeySequence(ShortcutManager::instance().shortcutFor(id));
}

void ShortcutEditorDialog::resetAll()
{
    ShortcutManager::instance().resetAll();
    populate();
}
