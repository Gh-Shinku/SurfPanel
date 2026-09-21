#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtGlobal>
#include <QtGui/qwindowdefs.h>
#include <filesystem>
#include <functional>
#include <vector>

inline constexpr int kSurfPanelPluginApiVersion = 1;

struct PluginMetadata {
  QString id;
  QString displayName;
  int apiVersion = kSurfPanelPluginApiVersion;
};

enum class PluginConfigurationState {
  Disabled,
  Enabled,
  Invalid,
};

struct PluginConfigurationResult {
  PluginConfigurationState state = PluginConfigurationState::Disabled;
  QString message;
};

struct PluginConfigurationContext {
  std::filesystem::path configRoot;
  std::filesystem::path pluginConfigPath;
  std::filesystem::path legacyMainConfigPath;
};

struct PluginHostContext {
  QObject *eventLoopOwner = nullptr;
  WId nativeWindow = 0;
  std::function<void(QtMsgType, const QString &)> log;
};

struct PluginFunction {
  QString name;
  QString description;
};

struct PluginFunctionResult {
  bool succeeded = false;
  QString message;
};

using PluginFunctionCompletion = std::function<void(PluginFunctionResult)>;

class IPlugin {
public:
  virtual ~IPlugin() = default;

  virtual PluginMetadata metadata() const = 0;
  virtual PluginConfigurationResult
  configure(const PluginConfigurationContext &context) = 0;
  virtual bool start(const PluginHostContext &context) = 0;
  virtual void stop() = 0;

  virtual std::vector<PluginFunction> functions() const { return {}; }
  virtual void invokeFunction(const QString &name,
                              const PluginHostContext &context,
                              PluginFunctionCompletion completion) {
    Q_UNUSED(name);
    Q_UNUSED(context);
    completion({false, "Function is not supported."});
  }

  virtual bool handleNativeEvent(const QByteArray &eventType, void *message,
                                 qintptr *result) {
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
  }
};
