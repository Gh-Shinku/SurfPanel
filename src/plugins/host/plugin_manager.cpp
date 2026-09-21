#include "plugins/host/plugin_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <algorithm>
#include <exception>
#include <utility>

namespace fs = std::filesystem;

namespace {

bool IsValidPluginId(const QString &id) {
  if (id.isEmpty()) {
    return false;
  }
  return std::all_of(id.cbegin(), id.cend(), [](QChar character) {
    const ushort value = character.unicode();
    return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
           value == '-';
  });
}

QString Diagnostic(const PluginMetadata &metadata, const QString &message) {
  return QString("Plugin '%1': %2").arg(metadata.id, message);
}

} // namespace

PluginManager::~PluginManager() { shutdown(); }

bool PluginManager::registerPlugin(std::unique_ptr<IPlugin> plugin) {
  if (plugin == nullptr) {
    return false;
  }

  const PluginMetadata metadata = plugin->metadata();
  if (!IsValidPluginId(metadata.id) || metadata.displayName.isEmpty() ||
      metadata.apiVersion != kSurfPanelPluginApiVersion ||
      std::any_of(entries_.cbegin(), entries_.cend(),
                  [&metadata](const Entry &entry) {
                    return entry.metadata.id == metadata.id;
                  })) {
    return false;
  }

  try {
    const auto available = plugin->functions();
    std::vector<QString> names;
    for (const auto &function : available) {
      if (function.name.trimmed().isEmpty() ||
          std::find(names.begin(), names.end(), function.name) != names.end()) {
        return false;
      }
      names.push_back(function.name);
    }
  } catch (...) {
    return false;
  }

  entries_.push_back(Entry{std::move(plugin), metadata, false});
  return true;
}

void PluginManager::initialize(PluginHostContext context) {
  shutdown();
  hostContext_ = std::move(context);
  if (QCoreApplication::instance() != nullptr) {
    QCoreApplication::instance()->installNativeEventFilter(this);
    nativeEventFilterInstalled_ = true;
  }
  initialized_ = true;
}

PluginReloadReport PluginManager::reload(const fs::path &configRoot) {
  PluginReloadReport report;
  for (auto &entry : entries_) {
    stopEntry(&entry);

    const PluginConfigurationContext configContext{
        configRoot,
        configRoot / "plugins" / (entry.metadata.id.toStdString() + ".toml"),
        configRoot / "items.toml"};

    PluginConfigurationResult configuration;
    try {
      configuration = entry.plugin->configure(configContext);
    } catch (const std::exception &error) {
      configuration.state = PluginConfigurationState::Invalid;
      configuration.message =
          QString("configuration threw an exception: %1").arg(error.what());
    } catch (...) {
      configuration.state = PluginConfigurationState::Invalid;
      configuration.message = "configuration threw an unknown exception";
    }

    if (!configuration.message.isEmpty()) {
      const QString message = Diagnostic(entry.metadata, configuration.message);
      report.messages.push_back(message);
      log(configuration.state == PluginConfigurationState::Invalid
              ? QtCriticalMsg
              : QtWarningMsg,
          message);
    }

    if (configuration.state == PluginConfigurationState::Invalid) {
      report.hasErrors = true;
      continue;
    }
    if (configuration.state == PluginConfigurationState::Disabled) {
      continue;
    }
    if (!initialized_) {
      const QString message =
          Diagnostic(entry.metadata, "manager has not been initialized");
      report.messages.push_back(message);
      report.hasErrors = true;
      log(QtCriticalMsg, message);
      continue;
    }

    bool started = false;
    try {
      started = entry.plugin->start(hostContext_);
    } catch (...) {
      started = false;
    }
    if (!started) {
      const QString message = Diagnostic(entry.metadata, "failed to start");
      report.messages.push_back(message);
      report.hasErrors = true;
      log(QtCriticalMsg, message);
      entry.active = true;
      stopEntry(&entry);
      continue;
    }
    entry.active = true;
  }
  return report;
}

void PluginManager::shutdown() {
  for (auto entry = entries_.rbegin(); entry != entries_.rend(); ++entry) {
    stopEntry(&*entry);
  }
  if (nativeEventFilterInstalled_ && QCoreApplication::instance() != nullptr) {
    QCoreApplication::instance()->removeNativeEventFilter(this);
  }
  nativeEventFilterInstalled_ = false;
  initialized_ = false;
  hostContext_ = PluginHostContext{};
}

bool PluginManager::handleNativeEvent(const QByteArray &eventType,
                                      void *message, qintptr *result) {
  bool handled = false;
  for (auto &entry : entries_) {
    if (!entry.active) {
      continue;
    }
    try {
      handled = entry.plugin->handleNativeEvent(eventType, message, result) ||
                handled;
    } catch (...) {
      const QString diagnostic =
          Diagnostic(entry.metadata, "native event handler failed; disabling");
      log(QtCriticalMsg, diagnostic);
      stopEntry(&entry);
    }
  }
  return handled;
}

bool PluginManager::nativeEventFilter(const QByteArray &eventType,
                                      void *message, qintptr *result) {
  return handleNativeEvent(eventType, message, result);
}

bool PluginManager::isActive(const QString &pluginId) const {
  const auto entry = std::find_if(entries_.cbegin(), entries_.cend(),
                                  [&pluginId](const Entry &candidate) {
                                    return candidate.metadata.id == pluginId;
                                  });
  return entry != entries_.cend() && entry->active;
}

std::size_t PluginManager::pluginCount() const { return entries_.size(); }

std::vector<PluginFunction>
PluginManager::functions(const QString &pluginId) const {
  for (const auto &entry : entries_) {
    if (entry.metadata.id == pluginId) {
      try {
        return entry.plugin->functions();
      } catch (...) {
        return {};
      }
    }
  }
  return {};
}

void PluginManager::invokeFunction(const QString &pluginId,
                                   const QString &functionName,
                                   PluginFunctionCompletion completion) {
  if (!completion) {
    return;
  }
  if (!initialized_) {
    completion({false, "Plugin runtime is not initialized."});
    return;
  }
  for (auto &entry : entries_) {
    if (entry.metadata.id != pluginId) {
      continue;
    }
    auto completed = std::make_shared<bool>(false);
    const auto finish = [completed, completion](PluginFunctionResult result) {
      if (!*completed) {
        *completed = true;
        completion(std::move(result));
      }
    };
    try {
      const auto available = entry.plugin->functions();
      if (std::none_of(available.begin(), available.end(),
                       [&functionName](const PluginFunction &function) {
                         return function.name == functionName;
                       })) {
        finish({false, "Unknown plugin function: " + functionName});
        return;
      }
      entry.usedFunctions = true;
      entry.pendingFunctions.erase(
          std::remove_if(entry.pendingFunctions.begin(),
                         entry.pendingFunctions.end(),
                         [](const PendingFunction &pending) {
                           return *pending.completed;
                         }),
          entry.pendingFunctions.end());
      entry.pendingFunctions.push_back({completed, finish});
      entry.plugin->invokeFunction(functionName, hostContext_, finish);
    } catch (...) {
      finish({false, "Plugin function threw an exception."});
    }
    return;
  }
  completion({false, "Unknown plugin: " + pluginId});
}

void PluginManager::stopEntry(Entry *entry) {
  if (entry == nullptr || (!entry->active && !entry->usedFunctions)) {
    return;
  }
  auto pending = std::move(entry->pendingFunctions);
  entry->pendingFunctions.clear();
  for (const auto &function : pending) {
    function.finish({false, "Plugin function was cancelled."});
  }
  try {
    entry->plugin->stop();
  } catch (...) {
    log(QtCriticalMsg, Diagnostic(entry->metadata, "threw while stopping"));
  }
  entry->active = false;
  entry->usedFunctions = false;
}

void PluginManager::log(QtMsgType type, const QString &message) const {
  if (hostContext_.log) {
    hostContext_.log(type, message);
    return;
  }
  if (type == QtCriticalMsg || type == QtFatalMsg) {
    qCritical().noquote() << message;
  } else {
    qWarning().noquote() << message;
  }
}
