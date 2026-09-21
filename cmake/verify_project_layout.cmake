if(NOT DEFINED SURFPANEL_SOURCE_DIR)
  message(FATAL_ERROR "SURFPANEL_SOURCE_DIR is required")
endif()

file(REAL_PATH "${SURFPANEL_SOURCE_DIR}" SURFPANEL_SOURCE_DIR)
set(SOURCE_ROOT "${SURFPANEL_SOURCE_DIR}/src")
set(TEST_ROOT "${SURFPANEL_SOURCE_DIR}/tests")

set(ALLOWED_SOURCE_MODULES app core platform plugins ui)
set(ALLOWED_TEST_MODULES core plugins support ui)

foreach(module IN LISTS ALLOWED_SOURCE_MODULES)
  if(NOT IS_DIRECTORY "${SOURCE_ROOT}/${module}")
    message(FATAL_ERROR "Missing source module: src/${module}")
  endif()
endforeach()

if(IS_DIRECTORY "${SOURCE_ROOT}/plugin")
  message(FATAL_ERROR
    "src/plugin is forbidden; plugin API and host code belong under src/plugins")
endif()

file(GLOB ROOT_SOURCE_FILES RELATIVE "${SURFPANEL_SOURCE_DIR}"
  "${SOURCE_ROOT}/*.cc"
  "${SOURCE_ROOT}/*.cpp"
  "${SOURCE_ROOT}/*.h"
  "${SOURCE_ROOT}/*.qrc"
  "${SOURCE_ROOT}/*.qss"
  "${SOURCE_ROOT}/*.rc"
  "${SOURCE_ROOT}/*.ui")
if(ROOT_SOURCE_FILES)
  list(JOIN ROOT_SOURCE_FILES ", " ROOT_SOURCE_LIST)
  message(FATAL_ERROR
    "Source files must belong to a module, not src/: ${ROOT_SOURCE_LIST}")
endif()

file(GLOB ROOT_TEST_FILES RELATIVE "${SURFPANEL_SOURCE_DIR}"
  "${TEST_ROOT}/*.cc"
  "${TEST_ROOT}/*.cpp"
  "${TEST_ROOT}/*.h")
if(ROOT_TEST_FILES)
  list(JOIN ROOT_TEST_FILES ", " ROOT_TEST_LIST)
  message(FATAL_ERROR
    "Tests must mirror a source module, not tests/: ${ROOT_TEST_LIST}")
endif()

function(check_top_level_modules root)
  set(allowed_modules ${ARGN})
  file(GLOB children LIST_DIRECTORIES true "${root}/*")
  foreach(child IN LISTS children)
    if(IS_DIRECTORY "${child}")
      get_filename_component(name "${child}" NAME)
      if(NOT name IN_LIST allowed_modules)
        file(RELATIVE_PATH relative "${SURFPANEL_SOURCE_DIR}" "${child}")
        message(FATAL_ERROR "Unexpected top-level module: ${relative}")
      endif()
    endif()
  endforeach()
endfunction()

check_top_level_modules("${SOURCE_ROOT}" ${ALLOWED_SOURCE_MODULES})
check_top_level_modules("${TEST_ROOT}" ${ALLOWED_TEST_MODULES})

file(GLOB plugin_modules LIST_DIRECTORIES true "${SOURCE_ROOT}/plugins/*")
foreach(module IN LISTS plugin_modules)
  if(IS_DIRECTORY "${module}")
    get_filename_component(name "${module}" NAME)
    if(NOT name MATCHES "^[a-z][a-z0-9_]*$")
      message(FATAL_ERROR
        "Plugin module directories must use lowercase snake_case: ${name}")
    endif()
  endif()
endforeach()

message(STATUS "SurfPanel project layout is valid")
