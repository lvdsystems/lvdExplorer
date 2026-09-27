#include "ui/dialogs/foldercomparedialog.h"

#include "core/ops/foldercompare.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString formatSize(qint64 bytes)
{
    if (bytes < 0)
        return {};
    return QLocale::system().formattedDataSize(bytes);
}

} // namespace

FolderCompareDialog::FolderCompareDialog(const QString &folderA, const QString &folderB, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Compare Folders"));
    resize(700, 450);

    m_folderAEdit = new QLineEdit(QDir::toNativeSeparators(folderA), this);
    m_folderBEdit = new QLineEdit(QDir::toNativeSeparators(folderB), this);
    auto *browseAButton = new QPushButton(tr("Browse…"), this);
    auto *browseBButton = new QPushButton(tr("Browse…"), this);
    auto *compareButton = new QPushButton(tr("Compare"), this);

    connect(browseAButton, &QPushButton::clicked, this, &FolderCompareDialog::browseA);
    connect(browseBButton, &QPushButton::clicked, this, &FolderCompareDialog::browseB);
    connect(compareButton, &QPushButton::clicked, this, &FolderCompareDialog::compare);

    auto *rowA = new QHBoxLayout;
    rowA->addWidget(new QLabel(tr("Folder A:"), this));
    rowA->addWidget(m_folderAEdit, 1);
    rowA->addWidget(browseAButton);

    auto *rowB = new QHBoxLayout;
    rowB->addWidget(new QLabel(tr("Folder B:"), this));
    rowB->addWidget(m_folderBEdit, 1);
    rowB->addWidget(browseBButton);

    m_resultsTable = new QTableWidget(0, 5, this);
    m_resultsTable->setHorizontalHeaderLabels({tr("Name"), tr("Status"), tr("Size A"), tr("Size B"), tr("Modified")});
    m_resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultsTable->setSortingEnabled(true);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(rowA);
    layout->addLayout(rowB);
    layout->addWidget(compareButton, 0, Qt::AlignLeft);
    layout->addWidget(m_resultsTable, 1);

    if (!folderA.isEmpty() && !folderB.isEmpty())
        compare();
}

void FolderCompareDialog::browseA()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Folder A"), m_folderAEdit->text());
    if (!dir.isEmpty())
        m_folderAEdit->setText(QDir::toNativeSeparators(dir));
}

void FolderCompareDialog::browseB()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Folder B"), m_folderBEdit->text());
    if (!dir.isEmpty())
        m_folderBEdit->setText(QDir::toNativeSeparators(dir));
}

void FolderCompareDialog::compare()
{
    const QString folderA = QDir::fromNativeSeparators(m_folderAEdit->text());
    const QString folderB = QDir::fromNativeSeparators(m_folderBEdit->text());
    if (!QFileInfo(folderA).isDir() || !QFileInfo(folderB).isDir())
        return;

    const QVector<FolderCompareEntry> entries = compareFolders(folderA, folderB);

    m_resultsTable->setSortingEnabled(false);
    m_resultsTable->setRowCount(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        const FolderCompareEntry &entry = entries.at(i);
        const QString status = entry.onlyInA()  ? tr("Only in A")
                                : entry.onlyInB() ? tr("Only in B")
                                : entry.differs() ? tr("Different")
                                                    : tr("Identical");

        m_resultsTable->setItem(i, 0, new QTableWidgetItem(entry.name));
        m_resultsTable->setItem(i, 1, new QTableWidgetItem(status));
        m_resultsTable->setItem(i, 2, new QTableWidgetItem(entry.inA ? formatSize(entry.sizeA) : QString()));
        m_resultsTable->setItem(i, 3, new QTableWidgetItem(entry.inB ? formatSize(entry.sizeB) : QString()));
        const QDateTime modified = entry.inA ? entry.modifiedA : entry.modifiedB;
        m_resultsTable->setItem(i, 4, new QTableWidgetItem(QLocale::system().toString(modified, QLocale::ShortFormat)));
    }
    m_resultsTable->setSortingEnabled(true);
}
