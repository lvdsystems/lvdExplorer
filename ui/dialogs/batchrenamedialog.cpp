#include "ui/dialogs/batchrenamedialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

BatchRenameDialog::BatchRenameDialog(const QStringList &paths, QWidget *parent)
    : QDialog(parent)
    , m_paths(paths)
{
    setWindowTitle(tr("Batch Rename — %1 items").arg(paths.size()));
    resize(560, 420);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Pattern"));
    m_modeCombo->addItem(tr("Find && Replace"));

    // Pattern mode page
    auto *patternPage = new QWidget(this);
    m_patternEdit = new QLineEdit(patternPage);
    m_patternEdit->setText(QStringLiteral("{name}"));
    m_startNumberSpin = new QSpinBox(patternPage);
    m_startNumberSpin->setRange(0, 999999);
    auto *patternForm = new QFormLayout(patternPage);
    patternForm->addRow(tr("Pattern:"), m_patternEdit);
    patternForm->addRow(tr("Start number:"), m_startNumberSpin);
    patternForm->addRow(QString(), new QLabel(tr("Tokens: {name} {ext} {n} {n:3}"), patternPage));

    // Find & Replace mode page
    auto *findPage = new QWidget(this);
    m_findEdit = new QLineEdit(findPage);
    m_replaceEdit = new QLineEdit(findPage);
    m_regexCheck = new QCheckBox(tr("Regex"), findPage);
    m_caseSensitiveCheck = new QCheckBox(tr("Case sensitive"), findPage);
    auto *findForm = new QFormLayout(findPage);
    findForm->addRow(tr("Find:"), m_findEdit);
    findForm->addRow(tr("Replace with:"), m_replaceEdit);
    auto *findOptionsRow = new QWidget(findPage);
    auto *findOptionsLayout = new QHBoxLayout(findOptionsRow);
    findOptionsLayout->setContentsMargins(0, 0, 0, 0);
    findOptionsLayout->addWidget(m_regexCheck);
    findOptionsLayout->addWidget(m_caseSensitiveCheck);
    findOptionsLayout->addStretch();
    findForm->addRow(QString(), findOptionsRow);

    auto *stack = new QStackedWidget(this);
    stack->addWidget(patternPage);
    stack->addWidget(findPage);
    connect(m_modeCombo, &QComboBox::currentIndexChanged, stack, &QStackedWidget::setCurrentIndex);

    m_previewTable = new QTableWidget(0, 2, this);
    m_previewTable->setHorizontalHeaderLabels({tr("Original"), tr("New Name")});
    m_previewTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_previewTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previewTable->setSelectionMode(QAbstractItemView::NoSelection);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *applyButton = buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(applyButton, &QPushButton::clicked, this, &BatchRenameDialog::apply);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_modeCombo);
    layout->addWidget(stack);
    layout->addWidget(new QLabel(tr("Preview:"), this));
    layout->addWidget(m_previewTable, 1);
    layout->addWidget(buttons);

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_patternEdit, &QLineEdit::textChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_startNumberSpin, &QSpinBox::valueChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_findEdit, &QLineEdit::textChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_replaceEdit, &QLineEdit::textChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_regexCheck, &QCheckBox::toggled, this, &BatchRenameDialog::updatePreview);
    connect(m_caseSensitiveCheck, &QCheckBox::toggled, this, &BatchRenameDialog::updatePreview);

    updatePreview();
}

QString BatchRenameDialog::computeNewName(int index, const QFileInfo &info) const
{
    if (m_modeCombo->currentIndex() == 0) {
        QString result = m_patternEdit->text();
        result.replace(QStringLiteral("{name}"), info.completeBaseName());
        result.replace(QStringLiteral("{ext}"), info.suffix());

        static const QRegularExpression counterRe(QStringLiteral("\\{n(?::(\\d+))?\\}"));
        QRegularExpressionMatch match;
        while ((match = counterRe.match(result)).hasMatch()) {
            const int width = match.captured(1).isEmpty() ? 1 : match.captured(1).toInt();
            const QString number = QString::number(index + m_startNumberSpin->value()).rightJustified(width, QLatin1Char('0'));
            result.replace(match.capturedStart(), match.capturedLength(), number);
        }
        return result;
    }

    const QString find = m_findEdit->text();
    QString base = info.completeBaseName();
    if (!find.isEmpty()) {
        const Qt::CaseSensitivity caseSensitivity =
            m_caseSensitiveCheck->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
        if (m_regexCheck->isChecked()) {
            QRegularExpression re(find,
                m_caseSensitiveCheck->isChecked() ? QRegularExpression::NoPatternOption
                                                   : QRegularExpression::CaseInsensitiveOption);
            base.replace(re, m_replaceEdit->text());
        } else {
            base.replace(find, m_replaceEdit->text(), caseSensitivity);
        }
    }
    return info.suffix().isEmpty() ? base : base + QLatin1Char('.') + info.suffix();
}

void BatchRenameDialog::updatePreview()
{
    m_previewTable->setRowCount(m_paths.size());
    for (int i = 0; i < m_paths.size(); ++i) {
        const QFileInfo info(m_paths.at(i));
        const QString newName = computeNewName(i, info);

        auto *originalItem = new QTableWidgetItem(info.fileName());
        auto *newItem = new QTableWidgetItem(newName);
        if (newName.isEmpty() || newName.contains(QLatin1Char('/')) || newName.contains(QLatin1Char('\\')))
            newItem->setForeground(Qt::red);

        m_previewTable->setItem(i, 0, originalItem);
        m_previewTable->setItem(i, 1, newItem);
    }
}

void BatchRenameDialog::apply()
{
    QStringList failed;
    bool anySucceeded = false;

    for (int i = 0; i < m_paths.size(); ++i) {
        const QFileInfo info(m_paths.at(i));
        const QString newName = computeNewName(i, info);

        if (newName.isEmpty() || newName.contains(QLatin1Char('/')) || newName.contains(QLatin1Char('\\'))
            || newName == info.fileName())
            continue;

        const QString newPath = info.absoluteDir().filePath(newName);
        QDir dir;
        if (dir.rename(info.absoluteFilePath(), newPath))
            anySucceeded = true;
        else
            failed.append(info.fileName());
    }

    if (!failed.isEmpty()) {
        QMessageBox::warning(this, tr("Batch Rename"),
            tr("Could not rename the following items:\n%1").arg(failed.join(QLatin1Char('\n'))));
    }

    if (anySucceeded)
        emit renamed();

    accept();
}
