#include "core/config/config_watcher.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

ConfigWatcher::ConfigWatcher(QObject *parent) : QObject(parent) {
  debounce_.setSingleShot(true);
  debounce_.setInterval(250);
  const auto changed = [this](const QString &) { debounce_.start(); };
  connect(&watcher_, &QFileSystemWatcher::fileChanged, this, changed);
  connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, changed);
  connect(&debounce_, &QTimer::timeout, this, &ConfigWatcher::checkChanges);
}

void ConfigWatcher::start(const QString &root) {
  debounce_.stop();
  root_ = QDir(root).absolutePath();
  QStringList paths;
  snapshot_ = scan(&paths);
  refreshWatches(paths);
  qInfo() << "Config hot reload started:" << root_
          << "source files:" << snapshot_.size()
          << "debounce ms:" << debounce_.interval();
}

QMap<QString, QByteArray> ConfigWatcher::scan(QStringList *paths) const {
  QMap<QString, QByteArray> result;
  // Keep the closest existing ancestor watched if the config root is removed.
  QString ancestor = QFileInfo(root_).absolutePath();
  while (!QDir(ancestor).exists()) {
    const QString parent = QFileInfo(ancestor).absolutePath();
    if (parent == ancestor) {
      break;
    }
    ancestor = parent;
  }
  paths->append(ancestor);
  QStringList pending{root_};
  while (!pending.isEmpty()) {
    const QString current = pending.takeLast();
    QDir dir(current);
    if (!dir.exists()) {
      continue;
    }
    paths->append(current);
    for (const auto &entry :
         dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot |
                           QDir::NoSymLinks | QDir::Hidden)) {
      if (entry.isDir()) {
        if (current != root_ ||
            entry.fileName().compare("cache", Qt::CaseInsensitive) != 0) {
          pending.append(entry.absoluteFilePath());
        }
      } else if (entry.suffix().compare("toml", Qt::CaseInsensitive) == 0) {
        const QString path = entry.absoluteFilePath();
        paths->append(path);
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
          result[path] = QCryptographicHash::hash(file.readAll(),
                                                  QCryptographicHash::Sha256);
        } else {
          result[path] = QByteArray("unreadable");
          qWarning() << "Cannot read watched config:" << path
                     << file.errorString();
        }
      }
    }
  }
  return result;
}

void ConfigWatcher::refreshWatches(const QStringList &paths) {
  const QSet<QString> desired(paths.begin(), paths.end());
  const QStringList existing = watcher_.files() + watcher_.directories();
  for (const auto &path : existing) {
    if (!desired.contains(path)) {
      watcher_.removePath(path);
    }
  }
  const QStringList retained = watcher_.files() + watcher_.directories();
  for (const auto &path : desired) {
    if (!retained.contains(path) && !watcher_.addPath(path)) {
      qWarning() << "Failed to watch config path:" << path;
    }
  }
}

void ConfigWatcher::checkChanges() {
  QStringList paths;
  const auto next = scan(&paths);
  refreshWatches(paths);
  if (next == snapshot_) {
    return;
  }
  QStringList changed;
  for (auto it = next.begin(); it != next.end(); ++it) {
    if (!snapshot_.contains(it.key()) ||
        snapshot_.value(it.key()) != it.value()) {
      changed.append(QDir(root_).relativeFilePath(it.key()));
    }
  }
  for (auto it = snapshot_.begin(); it != snapshot_.end(); ++it) {
    if (!next.contains(it.key())) {
      changed.append(QDir(root_).relativeFilePath(it.key()));
    }
  }
  snapshot_ = next;
  qInfo() << "Config sources changed:" << changed;
  emit configurationChanged();
}
