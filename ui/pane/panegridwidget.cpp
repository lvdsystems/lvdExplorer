#include "ui/pane/panegridwidget.h"

#include "ui/pane/panecontainer.h"

#include <QSplitter>
#include <QVBoxLayout>

namespace {

QString layoutModeToString(GridLayoutMode mode)
{
    switch (mode) {
    case GridLayoutMode::OnePane:
        return QStringLiteral("1");
    case GridLayoutMode::TwoPanesHorizontal:
        return QStringLiteral("2h");
    case GridLayoutMode::TwoPanesVertical:
        return QStringLiteral("2v");
    case GridLayoutMode::FourPanes:
        return QStringLiteral("4");
    }
    return QStringLiteral("1");
}

GridLayoutMode layoutModeFromString(const QString &text)
{
    if (text == QStringLiteral("2h"))
        return GridLayoutMode::TwoPanesHorizontal;
    if (text == QStringLiteral("2v"))
        return GridLayoutMode::TwoPanesVertical;
    if (text == QStringLiteral("4"))
        return GridLayoutMode::FourPanes;
    return GridLayoutMode::OnePane;
}

} // namespace

PaneGridWidget::PaneGridWidget(QWidget *parent)
    : QWidget(parent)
{
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);

    m_containers.append(createContainer());
    rebuild();
}

int PaneGridWidget::requiredCount(GridLayoutMode mode)
{
    switch (mode) {
    case GridLayoutMode::OnePane:
        return 1;
    case GridLayoutMode::FourPanes:
        return 4;
    case GridLayoutMode::TwoPanesHorizontal:
    case GridLayoutMode::TwoPanesVertical:
        return 2;
    }
    return 1;
}

PaneContainer *PaneGridWidget::createContainer()
{
    return new PaneContainer;
}

void PaneGridWidget::setLayoutMode(GridLayoutMode mode)
{
    m_mode = mode;

    const int required = requiredCount(mode);
    while (m_containers.size() < required)
        m_containers.append(createContainer());
    while (m_containers.size() > required)
        delete m_containers.takeLast();

    rebuild();
}

void PaneGridWidget::captureInto(SessionData &data) const
{
    data.paneLayout = layoutModeToString(m_mode);
    data.panes.clear();
    for (PaneContainer *container : m_containers)
        data.panes.append(container->captureState());
}

void PaneGridWidget::applyFrom(const SessionData &data)
{
    setLayoutMode(layoutModeFromString(data.paneLayout));
    for (int i = 0; i < m_containers.size() && i < data.panes.size(); ++i)
        m_containers.at(i)->applyState(data.panes.at(i));
}

void PaneGridWidget::rebuild()
{
    // Detach surviving containers before deleting the old splitter tree,
    // otherwise Qt's parent-child ownership would delete them along with it.
    for (PaneContainer *container : std::as_const(m_containers))
        container->setParent(nullptr);

    delete m_currentRoot;
    m_currentRoot = nullptr;

    QWidget *newRoot = nullptr;
    switch (m_mode) {
    case GridLayoutMode::OnePane: {
        // Deliberately wrapped rather than using the container itself as
        // the root: otherwise m_currentRoot would alias a widget we need
        // to keep, and the next rebuild()'s "delete m_currentRoot" would
        // destroy a container we're about to reuse (use-after-free when
        // switching from 1 pane to 2/4).
        auto *wrapper = new QWidget;
        auto *wrapperLayout = new QVBoxLayout(wrapper);
        wrapperLayout->setContentsMargins(0, 0, 0, 0);
        wrapperLayout->addWidget(m_containers.at(0));
        newRoot = wrapper;
        break;
    }
    case GridLayoutMode::TwoPanesHorizontal: {
        auto *splitter = new QSplitter(Qt::Horizontal);
        splitter->addWidget(m_containers.at(0));
        splitter->addWidget(m_containers.at(1));
        // Equal values here are proportions, not pixels -- QSplitter scales
        // them to fit whatever width it ends up with, so this is an exact
        // 50/50 split regardless of the window's actual size, rather than
        // leaving it to QSplitter's default size-hint-based distribution
        // (which is rarely actually even).
        splitter->setSizes({1, 1});
        newRoot = splitter;
        break;
    }
    case GridLayoutMode::TwoPanesVertical: {
        auto *splitter = new QSplitter(Qt::Vertical);
        splitter->addWidget(m_containers.at(0));
        splitter->addWidget(m_containers.at(1));
        splitter->setSizes({1, 1});
        newRoot = splitter;
        break;
    }
    case GridLayoutMode::FourPanes: {
        // Even split on *both* axes: each inner splitter divides its row
        // 50/50, and the outer splitter divides top/bottom 50/50, so every
        // quadrant ends up the same size.
        auto *topSplit = new QSplitter(Qt::Horizontal);
        topSplit->addWidget(m_containers.at(0));
        topSplit->addWidget(m_containers.at(1));
        topSplit->setSizes({1, 1});
        auto *bottomSplit = new QSplitter(Qt::Horizontal);
        bottomSplit->addWidget(m_containers.at(2));
        bottomSplit->addWidget(m_containers.at(3));
        bottomSplit->setSizes({1, 1});
        auto *outer = new QSplitter(Qt::Vertical);
        outer->addWidget(topSplit);
        outer->addWidget(bottomSplit);
        outer->setSizes({1, 1});
        newRoot = outer;
        break;
    }
    }

    m_currentRoot = newRoot;
    m_rootLayout->addWidget(m_currentRoot);
}
