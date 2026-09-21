add_library(surfpanel_core STATIC
  action/action_manager.cpp
  action/action_manager.h
  action/variable_resolver.cpp
  action/variable_resolver.h
  config/config.cpp
  config/config.h
  config/config_watcher.cpp
  config/config_watcher.h
  search/item.h
  search/search_engine.cpp
  search/search_engine.h
  storage/app_paths.cpp
  storage/app_paths.h
  storage/atomic_file.cpp
  storage/atomic_file.h
  storage/recent_items_store.cpp
  storage/recent_items_store.h
)
target_include_directories(surfpanel_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(surfpanel_core PUBLIC Qt6::Gui toml11::toml11)
add_library(SurfPanel::Core ALIAS surfpanel_core)
