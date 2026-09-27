#pragma once

#include "platform/iplatformshell.h"

namespace PlatformShell {

// The concrete implementation is chosen at compile time (Windows/Linux
// source files are mutually exclusive in platform/CMakeLists.txt), so this
// is a plain function rather than a class the caller has to construct.
IPlatformShell &instance();

} // namespace PlatformShell
