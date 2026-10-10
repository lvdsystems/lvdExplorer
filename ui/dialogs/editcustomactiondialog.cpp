#include "ui/dialogs/editcustomactiondialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

EditCustomActionDialog::EditCustomActionDialog(const CustomAction &action, QWidget *parent)
    : QDialog(parent)
    , m_original(action)
{
    const bool isNew = action.id.isEmpty();
    setWindowTitle(isNew ? tr("Add Custom Action") : tr("Edit Custom Action"));
    resize(420, 260);

    m_nameEdit = new QLineEdit(action.name, this);

    m_kindCombo = new QComboBox(this);
    m_kindCombo->addItem(tr("New File"), static_cast<int>(CustomAction::Kind::NewFile));
    m_kindCombo->addItem(tr("Open With"), static_cast<int>(CustomAction::Kind::OpenWith));
    m_kindCombo->setCurrentIndex(action.kind == CustomAction::Kind::OpenWith ? 1 : 0);
    // Changing kind after the fields below are filled in for an existing
    // action is an edge case (switching an action's whole purpose), but
    // disabling the combo would hide what kind a new action will be before
    // any page is visible -- simplest to just let the switch happen and
    // keep whatever the other page's fields already hold.

    // "New File" page
    auto *newFilePage = new QWidget(this);
    m_newFileNameEdit = new QLineEdit(action.newFileName, newFilePage);
    m_newFileNameEdit->setPlaceholderText(tr("New Text File.txt"));
    auto *newFileForm = new QFormLayout(newFilePage);
    newFileForm->setContentsMargins(0, 0, 0, 0);
    newFileForm->addRow(tr("Default file name:"), m_newFileNameEdit);

    // "Open With" page
    auto *openWithPage = new QWidget(this);
    m_extensionsEdit = new QLineEdit(action.extensions.join(QStringLiteral(", ")), openWithPage);
    m_extensionsEdit->setPlaceholderText(tr("zip, 7z"));
    m_programEdit = new QLineEdit(action.programPath, openWithPage);
    auto *browseButton = new QPushButton(tr("Browse…"), openWithPage);
    connect(browseButton, &QPushButton::clicked, this, &EditCustomActionDialog::browseForProgram);
    auto *programRow = new QHBoxLayout;
    programRow->addWidget(m_programEdit, 1);
    programRow->addWidget(browseButton);
    m_argumentsEdit = new QLineEdit(action.arguments, openWithPage);
    m_argumentsEdit->setPlaceholderText(tr("%1 (the file path)"));
    auto *openWithForm = new QFormLayout(openWithPage);
    openWithForm->setContentsMargins(0, 0, 0, 0);
    openWithForm->addRow(tr("Extensions (comma-separated):"), m_extensionsEdit);
    openWithForm->addRow(tr("Program:"), programRow);
    openWithForm->addRow(tr("Arguments:"), m_argumentsEdit);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(newFilePage);
    m_stack->addWidget(openWithPage);
    m_stack->setCurrentIndex(m_kindCombo->currentIndex());
    connect(m_kindCombo, &QComboBox::currentIndexChanged, m_stack, &QStackedWidget::setCurrentIndex);

    auto *topForm = new QFormLayout;
    topForm->addRow(tr("Name:"), m_nameEdit);
    topForm->addRow(tr("Kind:"), m_kindCombo);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &EditCustomActionDialog::handleAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(topForm);
    layout->addWidget(m_stack);
    layout->addStretch();
    layout->addWidget(buttons);
}

void EditCustomActionDialog::browseForProgram()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose Program"), m_programEdit->text());
    if (!path.isEmpty())
        m_programEdit->setText(QDir::toNativeSeparators(path));
}

void EditCustomActionDialog::handleAccept()
{
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("Enter a name for this action."));
        return;
    }

    const auto kind = static_cast<CustomAction::Kind>(m_kindCombo->currentData().toInt());
    if (kind == CustomAction::Kind::NewFile && m_newFileNameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("Enter the default file name to create."));
        return;
    }
    if (kind == CustomAction::Kind::OpenWith) {
        if (m_extensionsEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, windowTitle(), tr("Enter at least one file extension."));
            return;
        }
        if (m_programEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, windowTitle(), tr("Choose a program to open with."));
            return;
        }
    }

    accept();
}

CustomAction EditCustomActionDialog::result() const
{
    CustomAction action = m_original;
    action.name = m_nameEdit->text().trimmed();
    action.kind = static_cast<CustomAction::Kind>(m_kindCombo->currentData().toInt());
    action.newFileName = m_newFileNameEdit->text().trimmed();
    action.programPath = m_programEdit->text().trimmed();
    action.arguments = m_argumentsEdit->text().trimmed();

    QStringList extensions;
    for (const QString &raw : m_extensionsEdit->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        QString cleaned = raw.trimmed().toLower();
        // Accepts "zip", ".zip", and "*.zip" alike -- whichever way the
        // user is used to typing a file pattern.
        if (cleaned.startsWith(QLatin1String("*.")))
            cleaned.remove(0, 2);
        else if (cleaned.startsWith(QLatin1Char('*')))
            cleaned.remove(0, 1);
        if (cleaned.startsWith(QLatin1Char('.')))
            cleaned.remove(0, 1);
        if (!cleaned.isEmpty())
            extensions.append(cleaned);
    }
    action.extensions = extensions;

    return action;
}
