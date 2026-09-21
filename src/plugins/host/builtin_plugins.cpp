#include "plugins/host/builtin_plugins.h"

#include "plugins/clipboard_filter/clipboard_filter_plugin.h"
#include "plugins/host/plugin_manager.h"

#include <memory>

bool RegisterBuiltinPlugins(PluginManager *manager) {
  return manager != nullptr &&
         manager->registerPlugin(std::make_unique<ClipboardFilterPlugin>());
}
