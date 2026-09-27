#pragma once

#include "core/fsmodel/directoryscantask.h" // FileEntry
#include "core/search/contentmatch.h"

#include <QElapsedTimer>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QModelIndex;
class QPushButton;
class QTabWidget;
class QTreeView;
class SearchEngine;
class SearchResultsModel;
class ContentSearchEngine;
class ContentResultsModel;

// The dockable file finder, with two tabs sharing one
// scope (root/recursive/case-sensitive): "Filename" for the original
// substring/regex name search, and "Content" for a grep-style search of
// file contents. Entry points (Ctrl+F, a toolbar action, and the Tools
// menu) all live in MainWindow, which toggles the QDockWidget this panel
// is placed inside.
class FinderPanel : public QWidget
{
    Q_OBJECT

public:
    explicit FinderPanel(QWidget *parent = nullptr);

    // Called when the panel is opened, so the scope field defaults to
    // wherever the user was actually looking.
    void setInitialRoot(const QString &path);

signals:
    // folderPath is the result's containing directory; filePath is the
    // full path to select once that directory has been navigated to.
    void resultActivated(const QString &folderPath, const QString &filePath);

private slots:
    void browseForRoot();

    void startNameSearch();
    void stopNameSearch();
    void handleNameStarted();
    void handleNameMatches(const QVector<FileEntry> &matches);
    void handleNameFinished(bool wasCancelled, int totalMatches);
    void handleNameActivated(const QModelIndex &index);

    void startContentSearch();
    void stopContentSearch();
    void handleContentStarted();
    void handleContentMatches(const QVector<ContentMatch> &matches);
    void handleContentFinished(bool wasCancelled, int totalMatches);
    void handleContentActivated(const QModelIndex &index);

private:
    // Shared scope, common to both tabs.
    QLineEdit *m_rootEdit = nullptr;
    QCheckBox *m_recursiveCheck = nullptr;
    QCheckBox *m_caseSensitiveCheck = nullptr;
    QTabWidget *m_tabs = nullptr;

    // Filename tab.
    QLineEdit *m_patternEdit = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QPushButton *m_searchButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTreeView *m_resultsView = nullptr;
    SearchEngine *m_engine = nullptr;
    SearchResultsModel *m_resultsModel = nullptr;
    QElapsedTimer m_elapsed;

    // Content tab.
    QLineEdit *m_contentPatternEdit = nullptr;
    QComboBox *m_contentModeCombo = nullptr;
    QPushButton *m_contentSearchButton = nullptr;
    QPushButton *m_contentStopButton = nullptr;
    QLabel *m_contentStatusLabel = nullptr;
    QTreeView *m_contentResultsView = nullptr;
    ContentSearchEngine *m_contentEngine = nullptr;
    ContentResultsModel *m_contentResultsModel = nullptr;
    QElapsedTimer m_contentElapsed;
};
