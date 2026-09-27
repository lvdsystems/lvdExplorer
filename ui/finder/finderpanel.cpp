#include "ui/finder/finderpanel.h"

#include "core/search/contentsearchengine.h"
#include "core/search/contentsearchoptions.h"
#include "core/search/searchengine.h"
#include "core/search/searchoptions.h"
#include "ui/finder/contentresultsmodel.h"
#include "ui/finder/searchresultsmodel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeView>
#include <QVBoxLayout>

FinderPanel::FinderPanel(QWidget *parent)
    : QWidget(parent)
{
    m_rootEdit = new QLineEdit(this);
    auto *browseButton = new QPushButton(tr("Browse…"), this);
    auto *rootRow = new QWidget(this);
    auto *rootRowLayout = new QHBoxLayout(rootRow);
    rootRowLayout->setContentsMargins(0, 0, 0, 0);
    rootRowLayout->addWidget(m_rootEdit, 1);
    rootRowLayout->addWidget(browseButton);

    // Recursive/case-sensitive apply to whichever tab's search is run --
    // there's no reason those two options would differ between a filename
    // search and a content search of the same folder.
    m_recursiveCheck = new QCheckBox(tr("Include subfolders"), this);
    m_recursiveCheck->setChecked(true);
    m_caseSensitiveCheck = new QCheckBox(tr("Case sensitive"), this);

    auto *optionsRow = new QWidget(this);
    auto *optionsRowLayout = new QHBoxLayout(optionsRow);
    optionsRowLayout->setContentsMargins(0, 0, 0, 0);
    optionsRowLayout->addWidget(m_recursiveCheck);
    optionsRowLayout->addWidget(m_caseSensitiveCheck);
    optionsRowLayout->addStretch();

    auto *scopeForm = new QFormLayout;
    scopeForm->addRow(tr("In:"), rootRow);
    scopeForm->addRow(QString(), optionsRow);

    // --- Filename tab ---
    auto *nameTab = new QWidget(this);
    m_patternEdit = new QLineEdit(nameTab);
    m_patternEdit->setPlaceholderText(tr("Filename to find…"));
    m_modeCombo = new QComboBox(nameTab);
    m_modeCombo->addItem(tr("Simple (substring)"));
    m_modeCombo->addItem(tr("Regular expression"));

    auto *nameForm = new QFormLayout;
    nameForm->addRow(tr("Find:"), m_patternEdit);
    nameForm->addRow(tr("Mode:"), m_modeCombo);

    m_searchButton = new QPushButton(tr("Search"), nameTab);
    m_stopButton = new QPushButton(tr("Stop"), nameTab);
    m_stopButton->setEnabled(false);
    auto *nameButtonRow = new QHBoxLayout;
    nameButtonRow->addWidget(m_searchButton);
    nameButtonRow->addWidget(m_stopButton);
    nameButtonRow->addStretch();

    m_statusLabel = new QLabel(tr("Enter a filename and press Search."), nameTab);
    m_statusLabel->setWordWrap(true);

    m_resultsModel = new SearchResultsModel(nameTab);
    m_resultsView = new QTreeView(nameTab);
    m_resultsView->setModel(m_resultsModel);
    m_resultsView->setRootIsDecorated(false);
    m_resultsView->setUniformRowHeights(true);
    m_resultsView->setAlternatingRowColors(true);
    m_resultsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsView->header()->setSectionResizeMode(SearchResultsModel::NameColumn, QHeaderView::Stretch);

    auto *nameLayout = new QVBoxLayout(nameTab);
    nameLayout->addLayout(nameForm);
    nameLayout->addLayout(nameButtonRow);
    nameLayout->addWidget(m_statusLabel);
    nameLayout->addWidget(m_resultsView, 1);

    m_engine = new SearchEngine(this);

    // --- Content tab ---
    auto *contentTab = new QWidget(this);
    m_contentPatternEdit = new QLineEdit(contentTab);
    m_contentPatternEdit->setPlaceholderText(tr("Text to find inside files…"));
    m_contentModeCombo = new QComboBox(contentTab);
    m_contentModeCombo->addItem(tr("Simple (substring)"));
    m_contentModeCombo->addItem(tr("Regular expression"));

    auto *contentForm = new QFormLayout;
    contentForm->addRow(tr("Find:"), m_contentPatternEdit);
    contentForm->addRow(tr("Mode:"), m_contentModeCombo);

    m_contentSearchButton = new QPushButton(tr("Search"), contentTab);
    m_contentStopButton = new QPushButton(tr("Stop"), contentTab);
    m_contentStopButton->setEnabled(false);
    auto *contentButtonRow = new QHBoxLayout;
    contentButtonRow->addWidget(m_contentSearchButton);
    contentButtonRow->addWidget(m_contentStopButton);
    contentButtonRow->addStretch();

    m_contentStatusLabel = new QLabel(tr("Enter text and press Search. Binary files are skipped automatically."),
                                       contentTab);
    m_contentStatusLabel->setWordWrap(true);

    m_contentResultsModel = new ContentResultsModel(contentTab);
    m_contentResultsView = new QTreeView(contentTab);
    m_contentResultsView->setModel(m_contentResultsModel);
    m_contentResultsView->setRootIsDecorated(false);
    m_contentResultsView->setUniformRowHeights(true);
    m_contentResultsView->setAlternatingRowColors(true);
    m_contentResultsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_contentResultsView->header()->setSectionResizeMode(ContentResultsModel::SnippetColumn, QHeaderView::Stretch);

    auto *contentLayout = new QVBoxLayout(contentTab);
    contentLayout->addLayout(contentForm);
    contentLayout->addLayout(contentButtonRow);
    contentLayout->addWidget(m_contentStatusLabel);
    contentLayout->addWidget(m_contentResultsView, 1);

    m_contentEngine = new ContentSearchEngine(this);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(nameTab, tr("Filename"));
    m_tabs->addTab(contentTab, tr("Content"));

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(scopeForm);
    layout->addWidget(m_tabs, 1);

    connect(m_patternEdit, &QLineEdit::returnPressed, this, &FinderPanel::startNameSearch);
    connect(m_searchButton, &QPushButton::clicked, this, &FinderPanel::startNameSearch);
    connect(m_stopButton, &QPushButton::clicked, this, &FinderPanel::stopNameSearch);
    connect(browseButton, &QPushButton::clicked, this, &FinderPanel::browseForRoot);
    connect(m_resultsView, &QTreeView::activated, this, &FinderPanel::handleNameActivated);
    connect(m_engine, &SearchEngine::started, this, &FinderPanel::handleNameStarted);
    connect(m_engine, &SearchEngine::matchesReady, this, &FinderPanel::handleNameMatches);
    connect(m_engine, &SearchEngine::finished, this, &FinderPanel::handleNameFinished);

    connect(m_contentPatternEdit, &QLineEdit::returnPressed, this, &FinderPanel::startContentSearch);
    connect(m_contentSearchButton, &QPushButton::clicked, this, &FinderPanel::startContentSearch);
    connect(m_contentStopButton, &QPushButton::clicked, this, &FinderPanel::stopContentSearch);
    connect(m_contentResultsView, &QTreeView::activated, this, &FinderPanel::handleContentActivated);
    connect(m_contentEngine, &ContentSearchEngine::started, this, &FinderPanel::handleContentStarted);
    connect(m_contentEngine, &ContentSearchEngine::matchesReady, this, &FinderPanel::handleContentMatches);
    connect(m_contentEngine, &ContentSearchEngine::finished, this, &FinderPanel::handleContentFinished);
}

void FinderPanel::setInitialRoot(const QString &path)
{
    if (!path.isEmpty())
        m_rootEdit->setText(QDir::toNativeSeparators(path));

    QLineEdit *activePattern = m_tabs->currentIndex() == 1 ? m_contentPatternEdit : m_patternEdit;
    activePattern->setFocus();
    activePattern->selectAll();
}

void FinderPanel::browseForRoot()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Search In"), m_rootEdit->text());
    if (!dir.isEmpty())
        m_rootEdit->setText(QDir::toNativeSeparators(dir));
}

void FinderPanel::startNameSearch()
{
    const QString pattern = m_patternEdit->text().trimmed();
    const QString root = QDir::fromNativeSeparators(m_rootEdit->text().trimmed());
    if (pattern.isEmpty() || !QFileInfo(root).isDir()) {
        m_statusLabel->setText(tr("Enter a filename and a valid folder to search in."));
        return;
    }

    SearchOptions options;
    options.rootPath = root;
    options.pattern = pattern;
    options.mode = m_modeCombo->currentIndex() == 1 ? SearchOptions::Mode::Regex : SearchOptions::Mode::Substring;
    options.caseSensitive = m_caseSensitiveCheck->isChecked();
    options.recursive = m_recursiveCheck->isChecked();

    m_engine->start(options);
}

void FinderPanel::stopNameSearch()
{
    m_engine->cancel();
}

void FinderPanel::handleNameStarted()
{
    m_resultsModel->clear();
    m_elapsed.start();
    m_searchButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_statusLabel->setText(tr("Searching…"));
}

void FinderPanel::handleNameMatches(const QVector<FileEntry> &matches)
{
    m_resultsModel->addEntries(matches);
    m_statusLabel->setText(tr("Searching… %1 found").arg(m_resultsModel->rowCount()));
}

void FinderPanel::handleNameFinished(bool wasCancelled, int totalMatches)
{
    m_searchButton->setEnabled(true);
    m_stopButton->setEnabled(false);

    const double seconds = m_elapsed.elapsed() / 1000.0;
    m_statusLabel->setText(wasCancelled
        ? tr("Stopped: %1 found in %2 s").arg(totalMatches).arg(seconds, 0, 'f', 1)
        : tr("%1 found in %2 s").arg(totalMatches).arg(seconds, 0, 'f', 1));
}

void FinderPanel::handleNameActivated(const QModelIndex &index)
{
    const QString path = m_resultsModel->filePath(index);
    if (path.isEmpty())
        return;

    emit resultActivated(QFileInfo(path).absolutePath(), path);
}

void FinderPanel::startContentSearch()
{
    const QString pattern = m_contentPatternEdit->text().trimmed();
    const QString root = QDir::fromNativeSeparators(m_rootEdit->text().trimmed());
    if (pattern.isEmpty() || !QFileInfo(root).isDir()) {
        m_contentStatusLabel->setText(tr("Enter some text and a valid folder to search in."));
        return;
    }

    ContentSearchOptions options;
    options.rootPath = root;
    options.pattern = pattern;
    options.mode = m_contentModeCombo->currentIndex() == 1 ? ContentSearchOptions::Mode::Regex
                                                            : ContentSearchOptions::Mode::Substring;
    options.caseSensitive = m_caseSensitiveCheck->isChecked();
    options.recursive = m_recursiveCheck->isChecked();

    m_contentEngine->start(options);
}

void FinderPanel::stopContentSearch()
{
    m_contentEngine->cancel();
}

void FinderPanel::handleContentStarted()
{
    m_contentResultsModel->clear();
    m_contentElapsed.start();
    m_contentSearchButton->setEnabled(false);
    m_contentStopButton->setEnabled(true);
    m_contentStatusLabel->setText(tr("Searching…"));
}

void FinderPanel::handleContentMatches(const QVector<ContentMatch> &matches)
{
    m_contentResultsModel->addEntries(matches);
    m_contentStatusLabel->setText(tr("Searching… %1 found").arg(m_contentResultsModel->rowCount()));
}

void FinderPanel::handleContentFinished(bool wasCancelled, int totalMatches)
{
    m_contentSearchButton->setEnabled(true);
    m_contentStopButton->setEnabled(false);

    const double seconds = m_contentElapsed.elapsed() / 1000.0;
    m_contentStatusLabel->setText(wasCancelled
        ? tr("Stopped: %1 found in %2 s").arg(totalMatches).arg(seconds, 0, 'f', 1)
        : tr("%1 found in %2 s").arg(totalMatches).arg(seconds, 0, 'f', 1));
}

void FinderPanel::handleContentActivated(const QModelIndex &index)
{
    const QString path = m_contentResultsModel->filePath(index);
    if (path.isEmpty())
        return;

    emit resultActivated(QFileInfo(path).absolutePath(), path);
}
