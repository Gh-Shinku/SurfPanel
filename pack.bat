@echo off
setlocal

if not defined SURFPANEL_MINGW_ROOT (
  echo error: SURFPANEL_MINGW_ROOT must point to the MinGW installation prefix.
  exit /b 1
)

if not exist "%SURFPANEL_MINGW_ROOT%\bin\libstdc++-6.dll" (
  echo error: SURFPANEL_MINGW_ROOT does not contain bin\libstdc++-6.dll: "%SURFPANEL_MINGW_ROOT%"
  exit /b 1
)

if not exist "%~dp0build\SurfPanel.exe" (
  echo error: build\SurfPanel.exe not found. Build SurfPanel before packaging.
  exit /b 1
)

if defined SURFPANEL_ISCC (
  set "SURFPANEL_ISCC_PATH=%SURFPANEL_ISCC%"
) else (
  set "SURFPANEL_ISCC_PATH=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
)

if not exist "%SURFPANEL_ISCC_PATH%" (
  echo error: Inno Setup compiler not found: "%SURFPANEL_ISCC_PATH%"
  echo Set SURFPANEL_ISCC to the full path of ISCC.exe.
  exit /b 1
)

pushd "%~dp0" || exit /b 1
"%SURFPANEL_ISCC_PATH%" /Qp /O"Output" "installer.iss"
set "SURFPANEL_PACK_EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %SURFPANEL_PACK_EXIT_CODE%
