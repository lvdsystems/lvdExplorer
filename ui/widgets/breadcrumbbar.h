#pragma once

#include <QWidget>

class QHBoxLayout;
class QLineEdit;
class QStackedWidget;

// Clickable path-segment bar (like Explorer's address bar): shows the
// current path as separate buttons per segment, and swaps to a plain
// editable text field when the empty area is clicked, so a raw path can
// be typed and navigated to directly.
class BreadcrumbBar : public QWidget
{
    Q_OBJECT

public:
    explicit BreadcrumbBar(QWidget *parent = nullptr);

    void setPath(const QString &path);
    QString path() const { return m_path; }

signals:
    void pathActivated(const QString &path);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuildSegments();
    void beginEditing();
    void commitEditing();
    void cancelEditing();

    QString m_path;
    QWidget *m_segmentsWidget = nullptr;
    QHBoxLayout *m_segmentsLayout = nullptr;
    QLineEdit *m_editField = nullptr;
    QStackedWidget *m_stack = nullptr;
};
