#pragma once

#include <QIcon>

// Small monochrome icon set generated at runtime via QPainter rather than
// bundled image assets -- there are no design assets in this repo yet, and
// this keeps every icon visually consistent (one accent color, one style)
// for free. Revisit if/when real iconography is commissioned.
namespace IconFactory {

QIcon appIcon();

QIcon search();
QIcon lock();
QIcon terminal();
QIcon properties();
QIcon shellMenu();
QIcon checksum();
QIcon batchRename();
QIcon sessions();
QIcon drives();
QIcon compareFolders();
QIcon newFolder();
QIcon bookmark();

QIcon layoutOnePane();
QIcon layoutTwoPanesHorizontal();
QIcon layoutTwoPanesVertical();
QIcon layoutFourPanes();

} // namespace IconFactory
