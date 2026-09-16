#pragma once

#include "plugin.h"

#include <QAbstractNativeEventFilter>
#include <QStringList>
#include <filesystem>
#include <memory>
#include <vector>

struct PluginReloadReport {
  QStringList messages;
  bool hasErrors = false;
};

class PluginManager : public QAbstractNativeEventFilter {
public:
  ~PluginManager();

  bool registerPlugin(std::unique_ptr<IPlugin> plugin);
  void initialize(PluginHostContext context);
  PluginReloadReport reload(const std::filesystem::path &configRoot);
  void shutdown();

  bool handleNativeEvent(const QByteArray &eventType, void *message,
                         qintptr *result);
  bool nativeEventFilter(const QByteArray &eventType, void *message,
                         qintptr *result) override;
  bool isActive(const QString &pluginId) const;
  std::size_t pluginCount() const;
  std::vector<PluginFunction> functions(const QString &pluginId) const;
  void invokeFunction(const QString &pluginId, const QString &functionName,
                      PluginFunctionCompletion completion);

private:
  struct PendingFunction {
    std::shared_ptr<bool> completed;
    PluginFunctionCompletion finish;
  };
  struct Entry {
    std::unique_ptr<IPlugin> plugin;
    PluginMetadata metadata;
    bool active = false;
    bool usedFunctions = false;
    std::vector<PendingFunction> pendingFunctions;
  };

  void stopEntry(Entry *entry);
  void log(QtMsgType type, const QString &message) const;

  std::vector<Entry> entries_;
  PluginHostContext hostContext_;
  bool initialized_ = false;
  bool nativeEventFilterInstalled_ = false;
};
