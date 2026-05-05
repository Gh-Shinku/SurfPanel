#include "app_logging.h"
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  InstallFileLogger();
  MainWindow mainWindow;
  return app.exec();
}
