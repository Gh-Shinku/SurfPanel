#pragma once

#include "plugin.h"

#include <QStringList>
#include <filesystem>
#include <memory>
#include <vector>

struct PluginReloadReport {
  QStringList messages;
  bool hasErrors = false;
};

class PluginManager {
public:
  ~PluginManager();

  bool registerPlugin(std::unique_ptr<IPlugin> plugin);
  void initialize(PluginHostContext context);
  PluginReloadReport reload(const std::filesystem::path &configRoot);
  void shutdown();

  bool handleNativeEvent(const QByteArray &eventType, void *message,
                         qintptr *result);
  bool isActive(const QString &pluginId) const;
  std::size_t pluginCount() const;

private:
  struct Entry {
    std::unique_ptr<IPlugin> plugin;
    PluginMetadata metadata;
    bool active = false;
  };

  void stopEntry(Entry *entry);
  void log(QtMsgType type, const QString &message) const;

  std::vector<Entry> entries_;
  PluginHostContext hostContext_;
  bool initialized_ = false;
};
