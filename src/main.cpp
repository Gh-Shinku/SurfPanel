#include "app_logging.h"
#include "mainwindow.h"

#include <QApplication>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

void RegisterForInstallerRestart() {
#ifdef Q_OS_WIN
  const HRESULT status = RegisterApplicationRestart(
      L"--restart-after-update",
      RESTART_NO_CRASH | RESTART_NO_HANG | RESTART_NO_REBOOT);
  if (FAILED(status)) {
    qWarning() << "Failed to register SurfPanel with Windows Restart Manager:"
               << status;
  }
#endif
}

} // namespace

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName("SurfPanel");
  QCoreApplication::setApplicationVersion(SURFPANEL_VERSION);
  QCoreApplication::setOrganizationName("Shinku");
  InstallFileLogger();
  RegisterForInstallerRestart();
  MainWindow mainWindow;
  return app.exec();
}
