#include "core/storage/app_paths.h"

#include <QStandardPaths>
#include <QString>

namespace fs = std::filesystem;

namespace {

fs::path ToFileSystemPath(const QString &path) {
#ifdef _WIN32
  return fs::path(path.toStdWString());
#else
  return fs::u8path(path.toUtf8().toStdString());
#endif
}

fs::path WritableLocation(QStandardPaths::StandardLocation location) {
  return ToFileSystemPath(QStandardPaths::writableLocation(location));
}

} // namespace

fs::path AppDataRoot() {
  return WritableLocation(QStandardPaths::AppDataLocation);
}

fs::path UserConfigRoot() {
  return WritableLocation(QStandardPaths::GenericConfigLocation) / "SurfPanel" /
         "config";
}
