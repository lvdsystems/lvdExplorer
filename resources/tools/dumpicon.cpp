// Tiny packaging-only tool: renders IconFactory::appIcon() (the same
// QPainter-generated icon used for the window/taskbar icon at runtime) to
// a PNG file at a given size, so packaging steps (AppImage .desktop icon,
// Windows .ico, README logo) have real image assets without a separately
// maintained source image. Compiled standalone (see packaging/linux/
// build-appimage.sh and resources/tools/generate-icons.ps1), not part of
// the main CMake project -- rendering at the target size directly (rather
// than downscaling one large raster) keeps small sizes crisp, since
// IconFactory's drawing is proportional to the requested size, not a
// fixed-resolution image.
#include <QGuiApplication>
#include <QIcon>

#include "ui/resources/iconfactory.h"

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 2)
        return 1;

    const int size = argc >= 3 ? QString::fromLocal8Bit(argv[2]).toInt() : 256;
    const QIcon icon = IconFactory::appIcon();
    const QPixmap pixmap = icon.pixmap(size, size);
    return pixmap.save(QString::fromLocal8Bit(argv[1])) ? 0 : 1;
}
