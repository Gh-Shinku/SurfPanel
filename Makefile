.PHONY: check-env check-pack-env check-layout cg-debug cg-release build test pack clean

CMAKE ?= cmake
BUILD_DIR ?= build
export SURFPANEL_QT_ROOT
export SURFPANEL_MINGW_ROOT
export SURFPANEL_ISCC

check-env:
	@if not defined SURFPANEL_QT_ROOT (echo error: SURFPANEL_QT_ROOT must point to the Qt6 installation prefix. & exit /b 1)

check-pack-env:
	@if not defined SURFPANEL_MINGW_ROOT (echo error: SURFPANEL_MINGW_ROOT must point to the MinGW installation prefix. & exit /b 1)

check-layout:
	@$(CMAKE) -DSURFPANEL_SOURCE_DIR="$(CURDIR)" -P cmake/verify_project_layout.cmake

cg-debug: check-env
	@$(CMAKE) -G Ninja -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug

cg-release: check-env
	@$(CMAKE) -G Ninja -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

build: check-env
	@$(CMAKE) --build $(BUILD_DIR) --verbose

test: check-layout
	@ctest --test-dir $(BUILD_DIR) --output-on-failure

pack: check-pack-env
	cmd //c pack.bat

clean:
	@rm -rf build
