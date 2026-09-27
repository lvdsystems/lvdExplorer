#include "ui/pane/panecontainer.h"

#include "ui/pane/filepane.h"
#include "ui/pane/panetabbar.h"

#include <QDir>
#include <QFileInfo>
#include <QToolButton>

PaneContainer::PaneContainer(QWidget *parent)
    : QTabWidget(parent)
{
    // Replaces QTabBar's own drag handling entirely -- PaneTabBar supports
    // both in-place reordering and moving a tab to a different pane.
    setTabBar(new PaneTabBar(this, this));
    setTabsClosable(true);
    setDocumentMode(true);

    auto *addButton = new QToolButton(this);
    addButton->setText(QStringLiteral("+"));
    addButton->setAutoRaise(true);
    addButton->setToolTip(tr("New tab"));
    connect(addButton, &QToolButton::clicked, this, &PaneContainer::addNewTab);
    setCornerWidget(addButton, Qt::TopRightCorner);

    connect(this, &QTabWidget::tabCloseRequested, this, &PaneContainer::closeTab);

    createPane(QDir::homePath());
}

FilePane *PaneContainer::createPane(const QString &initialPath)
{
    auto *pane = new FilePane(this);
    const int index = addTab(pane, tr("New Tab"));
    connect(pane, &FilePane::pathChanged, this, [this, pane] { updateTabLabel(pane); });
    setCurrentIndex(index);
    pane->navigateTo(initialPath);
    return pane;
}

void PaneContainer::updateTabLabel(FilePane *pane)
{
    const int index = indexOf(pane);
    if (index < 0)
        return;

    const QString path = pane->currentPath();
    QString label = QFileInfo(path).fileName();
    if (label.isEmpty())
        label = QDir::toNativeSeparators(path); // e.g. a drive root like "C:/"

    setTabText(index, label);
    setTabToolTip(index, QDir::toNativeSeparators(path));
}

void PaneContainer::receiveTab(PaneContainer *source, int sourceIndex, int destIndex)
{
    if (!source || sourceIndex < 0 || sourceIndex >= source->count())
        return;

    if (source == this) {
        tabBar()->moveTab(sourceIndex, destIndex < 0 ? count() - 1 : destIndex);
        return;
    }

    auto *pane = qobject_cast<FilePane *>(source->widget(sourceIndex));
    if (!pane)
        return;

    const QString tabText = source->tabText(sourceIndex);
    const QString tabTooltip = source->tabToolTip(sourceIndex);

    if (source->count() <= 1)
        source->createPane(QDir::homePath()); // keep the source pane non-empty

    QObject::disconnect(pane, &FilePane::pathChanged, source, nullptr);
    source->removeTab(source->indexOf(pane));
    pane->setParent(this);

    const int insertIndex = destIndex < 0 ? count() : destIndex;
    insertTab(insertIndex, pane, tabText);
    setTabToolTip(insertIndex, tabTooltip);
    setCurrentIndex(insertIndex);
    connect(pane, &FilePane::pathChanged, this, [this, pane] { updateTabLabel(pane); });
}

PaneSessionState PaneContainer::captureState() const
{
    PaneSessionState state;
    state.activeTab = currentIndex();
    for (int i = 0; i < count(); ++i) {
        if (auto *pane = qobject_cast<FilePane *>(widget(i)))
            state.tabs.append(pane->captureState());
    }
    return state;
}

void PaneContainer::applyState(const PaneSessionState &state)
{
    while (count() > 0) {
        QWidget *tabWidget = widget(0);
        removeTab(0);
        delete tabWidget;
    }

    if (state.tabs.isEmpty()) {
        createPane(QDir::homePath());
        return;
    }

    for (const TabSessionState &tabState : state.tabs) {
        // createPane() already navigates to tabState.path; applyState()'s
        // own navigateTo() call is then a harmless no-op (same path), and
        // goes on to restore header/sort/read-only state.
        FilePane *pane = createPane(tabState.path);
        pane->applyState(tabState);
    }

    setCurrentIndex(qBound(0, state.activeTab, count() - 1));
}

void PaneContainer::addNewTab()
{
    auto *current = qobject_cast<FilePane *>(currentWidget());
    const QString path = current ? current->currentPath() : QDir::homePath();
    createPane(path);
}

void PaneContainer::closeTab(int index)
{
    if (count() <= 1)
        return; // always keep at least one tab per pane

    QWidget *tabWidget = widget(index);
    removeTab(index);
    tabWidget->deleteLater();
}
