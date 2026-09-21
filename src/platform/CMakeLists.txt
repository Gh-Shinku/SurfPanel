add_library(surfpanel_platform STATIC
  window_effects.cpp
  window_effects.h
)
target_include_directories(surfpanel_platform PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(surfpanel_platform PUBLIC Qt6::Gui)
add_library(SurfPanel::Platform ALIAS surfpanel_platform)
