add_library(surfpanel_plugin_runtime STATIC
  api/plugin.h
  host/plugin_manager.cpp
  host/plugin_manager.h
)
target_include_directories(surfpanel_plugin_runtime
  PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(surfpanel_plugin_runtime PUBLIC Qt6::Core)
add_library(SurfPanel::PluginRuntime ALIAS surfpanel_plugin_runtime)

add_library(surfpanel_clipboard_plugin STATIC
  clipboard_filter/clipboard_filter_plugin.cpp
  clipboard_filter/clipboard_filter_plugin.h
  clipboard_filter/text_transformer.cpp
  clipboard_filter/text_transformer.h
)
target_include_directories(surfpanel_clipboard_plugin
  PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(surfpanel_clipboard_plugin
  PRIVATE
  toml11::toml11
  PUBLIC
  SurfPanel::PluginRuntime
)
add_library(SurfPanel::ClipboardPlugin ALIAS surfpanel_clipboard_plugin)

add_library(surfpanel_builtin_plugins STATIC
  host/builtin_plugins.cpp
  host/builtin_plugins.h
)
target_include_directories(surfpanel_builtin_plugins
  PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(surfpanel_builtin_plugins
  PUBLIC SurfPanel::PluginRuntime
  PRIVATE SurfPanel::ClipboardPlugin
)
add_library(SurfPanel::BuiltinPlugins ALIAS surfpanel_builtin_plugins)
