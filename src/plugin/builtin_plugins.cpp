#include "builtin_plugins.h"

#include "plugin_manager.h"
#include "plugins/clipboard_filter/clipboard_filter_plugin.h"

#include <memory>

bool RegisterBuiltinPlugins(PluginManager *manager) {
  return manager != nullptr &&
         manager->registerPlugin(std::make_unique<ClipboardFilterPlugin>());
}
