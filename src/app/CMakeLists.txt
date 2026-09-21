add_executable(SurfPanel WIN32
  app_logging.cpp
  app_logging.h
  main.cpp
  resources.qrc
)
set_target_properties(SurfPanel PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_link_libraries(SurfPanel PRIVATE SurfPanel::Core SurfPanel::Ui)

if(WIN32)
  target_sources(SurfPanel PRIVATE app_icon.rc)
  target_include_directories(SurfPanel PRIVATE "${PROJECT_SOURCE_DIR}/assets")
  set_property(SOURCE app_icon.rc PROPERTY OBJECT_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/SurfPanel.ico")
endif()

if(MINGW)
  target_link_options(SurfPanel PRIVATE $<$<CONFIG:Release>:-s>)
endif()

find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
if(WINDEPLOYQT_EXECUTABLE)
  set(WINDEPLOYQT_ARGS
    --no-opengl-sw
    --no-system-d3d-compiler
    --no-system-dxc-compiler
    --no-compiler-runtime
    --no-translations
    --skip-plugin-types generic,networkinformation,tls
    --exclude-plugins qgif,qjpeg
  )
  add_custom_command(TARGET SurfPanel POST_BUILD
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" ${WINDEPLOYQT_ARGS} "$<TARGET_FILE:SurfPanel>"
    COMMENT "Running windeployqt to copy Qt plugins..."
  )
endif()
