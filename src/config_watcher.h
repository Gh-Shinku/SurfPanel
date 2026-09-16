#pragma once

#include <QFileSystemWatcher>
#include <QMap>
#include <QObject>
#include <QTimer>

// Watches source TOML files, not generated caches. Directory watches restore
// file watches after editor atomic saves and discover new imported/plugin
// files.
class ConfigWatcher : public QObject {
  Q_OBJECT
public:
  explicit ConfigWatcher(QObject *parent = nullptr);
  void start(const QString &root);
  bool isWatching() const { return !root_.isEmpty(); }

signals:
  void configurationChanged();

private:
  QMap<QString, QByteArray> scan(QStringList *paths) const;
  void refreshWatches(const QStringList &paths);
  void checkChanges();
  QString root_;
  QFileSystemWatcher watcher_;
  QTimer debounce_;
  QMap<QString, QByteArray> snapshot_;
};
