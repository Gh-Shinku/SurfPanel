#include "platform/window_effects.h"

#include <QLibrary>
#include <QOperatingSystemVersion>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>

namespace {
struct DwmApi {
  QLibrary library{"dwmapi"};
  using SetAttribute = HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
  using ExtendFrame = HRESULT(WINAPI *)(HWND, const MARGINS *);
  SetAttribute setAttribute =
      reinterpret_cast<SetAttribute>(library.resolve("DwmSetWindowAttribute"));
  ExtendFrame extendFrame = reinterpret_cast<ExtendFrame>(
      library.resolve("DwmExtendFrameIntoClientArea"));
};
DwmApi &Api() {
  static DwmApi api;
  return api;
}
} // namespace
#endif

bool SupportsNativeBackdrop() {
#ifdef Q_OS_WIN
  return QOperatingSystemVersion::current() >=
             QOperatingSystemVersion(QOperatingSystemVersion::Windows, 10, 0,
                                     22621) &&
         Api().setAttribute && Api().extendFrame;
#else
  return false;
#endif
}

bool ApplyNativeBackdrop(WId window, bool darkMode) {
#ifdef Q_OS_WIN
  if (!SupportsNativeBackdrop() || !window) {
    return false;
  }
  HWND hwnd = reinterpret_cast<HWND>(window);
  const int corners = DWMWCP_ROUNDSMALL;
  const int backdrop = DWMSBT_TRANSIENTWINDOW;
  const MARGINS margins{-1, -1, -1, -1};
  auto &api = Api();
  ApplyNativeWindowTheme(window, darkMode);
  api.setAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners,
                   sizeof(corners));
  return SUCCEEDED(api.extendFrame(hwnd, &margins)) &&
         SUCCEEDED(api.setAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop,
                                    sizeof(backdrop)));
#else
  Q_UNUSED(window);
  Q_UNUSED(darkMode);
  return false;
#endif
}

bool ApplyNativeWindowTheme(WId window, bool darkMode) {
#ifdef Q_OS_WIN
  if (!window || !Api().setAttribute) {
    return false;
  }
  const BOOL dark = darkMode;
  return SUCCEEDED(Api().setAttribute(reinterpret_cast<HWND>(window),
                                      DWMWA_USE_IMMERSIVE_DARK_MODE, &dark,
                                      sizeof(dark)));
#else
  Q_UNUSED(window);
  Q_UNUSED(darkMode);
  return false;
#endif
}

bool SystemAnimationsEnabled() {
#ifdef Q_OS_WIN
  BOOL enabled = TRUE;
  SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0);
  return enabled;
#else
  return true;
#endif
}
