#include "mainwindow.h"
#include <QApplication>
#include <filesystem>

namespace fs = std::filesystem;

extern void load_config(const fs::path &path);

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  load_config("C:\\Users\\shinku\\Code\\SurfPanel\\config\\test.toml");
  MainWindow mainWindow;
  mainWindow.show();
  return app.exec();
}
