#include "ui/widgets/breadcrumbbar.h"

#include <QDir>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPair>
#include <QStackedWidget>
#include <QToolButton>
#include <QVector>

namespace {

// Splits an absolute path into (label, cumulativePath) segments, handling
// both POSIX roots ("/") and Windows drive roots ("C:/").
QVector<QPair<QString, QString>> splitPathSegments(const QString &absolutePath)
{
    QVector<QPair<QString, QString>> segments;
    QString cleanPath = QDir::fromNativeSeparators(absolutePath);
    if (cleanPath.isEmpty())
        return segments;

    if (cleanPath.length() >= 2 && cleanPath.at(1) == QLatin1Char(':')) {
        const QString driveLabel = cleanPath.left(2);
        segments.append({driveLabel, driveLabel + QLatin1Char('/')});
        cleanPath = cleanPath.mid(3);
    } else if (cleanPath.startsWith(QLatin1Char('/'))) {
        segments.append({QStringLiteral("/"), QStringLiteral("/")});
        cleanPath = cleanPath.mid(1);
    }

    QString accumulated = segments.isEmpty() ? QString() : segments.last().second;
    const QStringList parts = cleanPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (!accumulated.isEmpty() && !accumulated.endsWith(QLatin1Char('/')))
            accumulated += QLatin1Char('/');
        accumulated += part;
        segments.append({part, accumulated});
    }
    return segments;
}

} // namespace

BreadcrumbBar::BreadcrumbBar(QWidget *parent)
    : QWidget(parent)
{
    m_segmentsWidget = new QWidget(this);
    m_segmentsLayout = new QHBoxLayout(m_segmentsWidget);
    m_segmentsLayout->setContentsMargins(4, 2, 4, 2);
    m_segmentsLayout->setSpacing(2);
    m_segmentsLayout->addStretch();
    m_segmentsWidget->installEventFilter(this);

    m_editField = new QLineEdit(this);
    m_editField->installEventFilter(this);
    connect(m_editField, &QLineEdit::returnPressed, this, &BreadcrumbBar::commitEditing);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_segmentsWidget);
    m_stack->addWidget(m_editField);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);
}

void BreadcrumbBar::setPath(const QString &path)
{
    m_path = path;
    rebuildSegments();
    m_stack->setCurrentWidget(m_segmentsWidget);
}

void BreadcrumbBar::rebuildSegments()
{
    QLayoutItem *item = nullptr;
    while ((item = m_segmentsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    const auto segments = splitPathSegments(m_path);
    for (const auto &segment : segments) {
        auto *button = new QToolButton(m_segmentsWidget);
        button->setText(segment.first);
        button->setAutoRaise(true);
        const QString target = segment.second;
        connect(button, &QToolButton::clicked, this, [this, target] { emit pathActivated(target); });
        m_segmentsLayout->addWidget(button);
    }
    m_segmentsLayout->addStretch();
}

bool BreadcrumbBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_segmentsWidget && event->type() == QEvent::MouseButtonPress) {
        beginEditing();
        return true;
    }
    if (watched == m_editField && event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            cancelEditing();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void BreadcrumbBar::beginEditing()
{
    m_editField->setText(QDir::toNativeSeparators(m_path));
    m_stack->setCurrentWidget(m_editField);
    m_editField->setFocus();
    m_editField->selectAll();
}

void BreadcrumbBar::commitEditing()
{
    const QString typed = m_editField->text().trimmed();
    m_stack->setCurrentWidget(m_segmentsWidget);
    if (!typed.isEmpty())
        emit pathActivated(QDir::fromNativeSeparators(typed));
}

void BreadcrumbBar::cancelEditing()
{
    m_stack->setCurrentWidget(m_segmentsWidget);
}
