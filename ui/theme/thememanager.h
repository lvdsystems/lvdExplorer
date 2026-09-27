#pragma once

#include <QString>

// Explicit light/dark/system theming. "System" means
// "leave Qt's own default style/palette alone" -- captured once on first
// use so switching back to it after Light/Dark is a true revert, not a
// guess at what the OS default looked like.
namespace ThemeManager {

enum class Theme { System, Light, Dark };

void apply(Theme theme);

Theme loadSaved();
void save(Theme theme);

} // namespace ThemeManager
