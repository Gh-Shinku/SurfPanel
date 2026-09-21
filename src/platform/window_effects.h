#pragma once

#include <QtGui/qwindowdefs.h>

bool SupportsNativeBackdrop();
bool ApplyNativeWindowTheme(WId window, bool darkMode);
bool ApplyNativeBackdrop(WId window, bool darkMode);
bool SystemAnimationsEnabled();
