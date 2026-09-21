add_library(surfpanel_ui STATIC
  palette/fluent_panel.cpp
  palette/fluent_panel.h
  palette/palette_geometry.h
  palette/palette_search_input.cpp
  palette/palette_search_input.h
  palette/palette_theme.h
  palette/search_result_view.cpp
  palette/search_result_view.h
  tray/tray_menu.cpp
  tray/tray_menu.h
  window/about_dialog.cpp
  window/about_dialog.h
  window/mainwindow.cpp
  window/mainwindow.h
)
target_include_directories(surfpanel_ui PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(surfpanel_ui
  PUBLIC SURFPANEL_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(surfpanel_ui
  PUBLIC
  Qt6::Widgets
  SurfPanel::BuiltinPlugins
  SurfPanel::Core
  SurfPanel::Platform
  SurfPanel::PluginRuntime
)
add_library(SurfPanel::Ui ALIAS surfpanel_ui)
