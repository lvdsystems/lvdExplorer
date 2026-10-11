#pragma once

#include <QSettings>

// A QSettings instance that, in portable mode
// (SessionManager::isPortableMode()), writes to an .ini file under the
// same lvdExplorer-data/ folder portable sessions already use, instead of
// the registry (Windows) or ~/.config (Linux) -- so a portable install
// run from a USB stick/network share really does keep bookmarks,
// shortcuts, theme, and custom actions local to it rather than leaking
// them onto whatever machine it's run on. Falls back to a plain
// default-constructed QSettings (QSettings::NativeFormat) when not
// portable, identical to every caller's previous behavior.
//
// Safe to return by value: C++17 guaranteed copy elision constructs the
// QSettings directly at the call site, so QSettings not being copyable
// never comes into play.
QSettings portableAwareSettings();
