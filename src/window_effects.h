#pragma once

#include <QtGui/qwindowdefs.h>

bool SupportsNativeBackdrop();
bool ApplyNativeBackdrop(WId window, bool darkMode);
bool SystemAnimationsEnabled();
