#include "ui/resources/iconfactory.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

#include <cmath>
#include <functional>

namespace {

const QColor kIconColor(0x4A, 0x6F, 0x8F);

QIcon buildIcon(const std::function<void(QPainter &, int)> &draw)
{
    QIcon icon;
    // Covers toolbar/menu sizes through large taskbar/Alt-Tab/window-icon
    // sizes. Requesting a size not in this list (e.g. QIcon::pixmap(256,256)
    // when only {16,24,32,48} were baked) silently falls back to the
    // nearest available size scaled up -- blurry, not an exact render --
    // since IconFactory's drawing is proportional to `size`, baking more
    // exact sizes directly is what actually fixes that, not a bigger
    // *single* size (a lone 256 bake would then blur when scaled *down*
    // for a 16px toolbar icon instead).
    for (int size : {16, 20, 24, 32, 40, 48, 64, 96, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        draw(painter, size);
        icon.addPixmap(pixmap);
    }
    return icon;
}

QIcon badgeGlyphIcon(const QString &glyph)
{
    return buildIcon([glyph](QPainter &painter, int size) {
        const qreal margin = size * 0.08;
        const QRectF rect(margin, margin, size - 2 * margin, size - 2 * margin);

        painter.setPen(QPen(kIconColor, size * 0.06));
        painter.setBrush(QColor(kIconColor.red(), kIconColor.green(), kIconColor.blue(), 30));
        painter.drawRoundedRect(rect, size * 0.2, size * 0.2);

        QFont font = painter.font();
        const qreal scale = glyph.size() <= 1 ? 0.46 : (glyph.size() == 2 ? 0.36 : 0.28);
        font.setPixelSize(static_cast<int>(size * scale));
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(kIconColor);
        painter.drawText(rect, Qt::AlignCenter, glyph);
    });
}

QIcon layoutIcon(int rows, int cols)
{
    return buildIcon([rows, cols](QPainter &painter, int size) {
        const qreal margin = size * 0.12;
        const QRectF outer(margin, margin, size - 2 * margin, size - 2 * margin);

        painter.setPen(QPen(kIconColor, size * 0.06));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(outer, size * 0.08, size * 0.08);

        painter.setPen(QPen(kIconColor, size * 0.045));
        const qreal cellW = outer.width() / cols;
        const qreal cellH = outer.height() / rows;
        for (int c = 1; c < cols; ++c) {
            const qreal x = outer.left() + c * cellW;
            painter.drawLine(QPointF(x, outer.top()), QPointF(x, outer.bottom()));
        }
        for (int r = 1; r < rows; ++r) {
            const qreal y = outer.top() + r * cellH;
            painter.drawLine(QPointF(outer.left(), y), QPointF(outer.right(), y));
        }
    });
}

} // namespace

QIcon IconFactory::appIcon()
{
    return buildIcon([](QPainter &painter, int size) {
        const QColor back(0x2E, 0x5B, 0x8A);
        const QColor front(0x4F, 0x8E, 0xC7);

        QPainterPath tab;
        const qreal tabWidth = size * 0.36;
        const qreal tabHeight = size * 0.12;
        tab.addRoundedRect(QRectF(size * 0.1, size * 0.18, tabWidth, tabHeight), size * 0.03, size * 0.03);

        QPainterPath body;
        body.addRoundedRect(QRectF(size * 0.1, size * 0.18 + tabHeight * 0.6, size * 0.8, size * 0.58), size * 0.06,
                             size * 0.06);

        painter.setPen(Qt::NoPen);
        painter.setBrush(back);
        painter.drawPath(tab);
        painter.setBrush(front);
        painter.drawPath(body);
    });
}

QIcon IconFactory::search()
{
    return buildIcon([](QPainter &painter, int size) {
        painter.setPen(QPen(kIconColor, size * 0.09));
        painter.setBrush(Qt::NoBrush);
        const qreal r = size * 0.32;
        const QPointF center(size * 0.42, size * 0.42);
        painter.drawEllipse(center, r, r);
        painter.drawLine(center + QPointF(r * 0.75, r * 0.75), QPointF(size * 0.88, size * 0.88));
    });
}

QIcon IconFactory::lock()
{
    return buildIcon([](QPainter &painter, int size) {
        painter.setPen(QPen(kIconColor, size * 0.07));
        painter.setBrush(Qt::NoBrush);
        const QRectF shackle(size * 0.28, size * 0.14, size * 0.44, size * 0.36);
        painter.drawArc(shackle, 0, 180 * 16);

        painter.setPen(Qt::NoPen);
        painter.setBrush(kIconColor);
        const QRectF body(size * 0.2, size * 0.42, size * 0.6, size * 0.42);
        painter.drawRoundedRect(body, size * 0.08, size * 0.08);
    });
}

QIcon IconFactory::terminal()
{
    return badgeGlyphIcon(QStringLiteral(">_"));
}

QIcon IconFactory::properties()
{
    return badgeGlyphIcon(QStringLiteral("i"));
}

QIcon IconFactory::shellMenu()
{
    return buildIcon([](QPainter &painter, int size) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(kIconColor);
        const qreal r = size * 0.07;
        const qreal cy = size * 0.5;
        for (int i = -1; i <= 1; ++i)
            painter.drawEllipse(QPointF(size * 0.5 + i * size * 0.24, cy), r, r);
    });
}

QIcon IconFactory::checksum()
{
    return badgeGlyphIcon(QStringLiteral("#"));
}

QIcon IconFactory::batchRename()
{
    return badgeGlyphIcon(QStringLiteral("Aa"));
}

QIcon IconFactory::sessions()
{
    return badgeGlyphIcon(QStringLiteral("S"));
}

QIcon IconFactory::drives()
{
    return buildIcon([](QPainter &painter, int size) {
        painter.setPen(QPen(kIconColor, size * 0.07));
        painter.setBrush(QColor(kIconColor.red(), kIconColor.green(), kIconColor.blue(), 30));
        painter.drawRoundedRect(QRectF(size * 0.12, size * 0.28, size * 0.76, size * 0.5), size * 0.06, size * 0.06);
        painter.drawLine(QPointF(size * 0.2, size * 0.66), QPointF(size * 0.6, size * 0.66));
        painter.setPen(Qt::NoPen);
        painter.setBrush(kIconColor);
        painter.drawEllipse(QPointF(size * 0.76, size * 0.66), size * 0.045, size * 0.045);
    });
}

QIcon IconFactory::compareFolders()
{
    return buildIcon([](QPainter &painter, int size) {
        painter.setPen(QPen(kIconColor, size * 0.06));
        painter.setBrush(QColor(kIconColor.red(), kIconColor.green(), kIconColor.blue(), 30));
        const qreal boxSize = size * 0.36;
        const qreal y = size * 0.32;
        painter.drawRoundedRect(QRectF(size * 0.08, y, boxSize, boxSize), size * 0.05, size * 0.05);
        painter.drawRoundedRect(QRectF(size * 0.56, y, boxSize, boxSize), size * 0.05, size * 0.05);
        painter.drawLine(QPointF(size * 0.44, y + boxSize / 2), QPointF(size * 0.56, y + boxSize / 2));
    });
}

QIcon IconFactory::newFolder()
{
    return buildIcon([](QPainter &painter, int size) {
        // Same folder-tab motif as appIcon(), but monochrome outline to
        // match the rest of the in-app icon set.
        painter.setPen(QPen(kIconColor, size * 0.055));
        painter.setBrush(QColor(kIconColor.red(), kIconColor.green(), kIconColor.blue(), 30));

        QPainterPath folder;
        const qreal bodyTop = size * 0.32;
        folder.addRoundedRect(QRectF(size * 0.08, bodyTop - size * 0.09, size * 0.32, size * 0.09), size * 0.02,
                               size * 0.02);
        folder.addRoundedRect(QRectF(size * 0.08, bodyTop, size * 0.68, size * 0.46), size * 0.05, size * 0.05);
        painter.drawPath(folder.simplified());

        // "+" badge, filled solid so it stays legible over the folder outline.
        painter.setPen(Qt::NoPen);
        painter.setBrush(kIconColor);
        const qreal cx = size * 0.76;
        const qreal cy = size * 0.74;
        const qreal armLength = size * 0.15;
        const qreal armThickness = size * 0.06;
        painter.drawRoundedRect(QRectF(cx - armLength, cy - armThickness / 2, armLength * 2, armThickness),
                                 armThickness / 2, armThickness / 2);
        painter.drawRoundedRect(QRectF(cx - armThickness / 2, cy - armLength, armThickness, armLength * 2),
                                 armThickness / 2, armThickness / 2);
    });
}

QIcon IconFactory::bookmark()
{
    return buildIcon([](QPainter &painter, int size) {
        // Five-point star: a silhouette distinct from the rest of this
        // icon set, and the conventional "favorite this" glyph.
        constexpr double kPi = 3.14159265358979323846;
        const QPointF center(size * 0.5, size * 0.52);
        const qreal outerR = size * 0.42;
        const qreal innerR = outerR * 0.42;

        QPolygonF star;
        for (int i = 0; i < 10; ++i) {
            const qreal r = (i % 2 == 0) ? outerR : innerR;
            const qreal angle = -kPi / 2 + i * kPi / 5;
            star << QPointF(center.x() + r * std::cos(angle), center.y() + r * std::sin(angle));
        }

        painter.setPen(QPen(kIconColor, size * 0.045));
        painter.setBrush(QColor(kIconColor.red(), kIconColor.green(), kIconColor.blue(), 60));
        painter.drawPolygon(star);
    });
}

QIcon IconFactory::layoutOnePane()
{
    return layoutIcon(1, 1);
}

QIcon IconFactory::layoutTwoPanesHorizontal()
{
    return layoutIcon(1, 2);
}

QIcon IconFactory::layoutTwoPanesVertical()
{
    return layoutIcon(2, 1);
}

QIcon IconFactory::layoutFourPanes()
{
    return layoutIcon(2, 2);
}
