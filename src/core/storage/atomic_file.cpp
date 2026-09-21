#include "core/storage/atomic_file.h"

#include <QSaveFile>
#include <QString>

namespace fs = std::filesystem;

namespace {

QString ToQString(const fs::path &path) {
#ifdef _WIN32
  return QString::fromStdWString(path.wstring());
#else
  return QString::fromUtf8(path.u8string().c_str());
#endif
}

} // namespace

bool WriteFileAtomically(const fs::path &path, const QByteArray &contents) {
  QSaveFile file(ToQString(path));
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(contents) != contents.size()) {
    file.cancelWriting();
    return false;
  }

  return file.commit();
}
