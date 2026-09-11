.PHONY: check-env cg-debug cg-release build test pack clean

CMAKE ?= cmake
BUILD_DIR ?= build
export SURFPANEL_QT_ROOT

check-env:
	@if not defined SURFPANEL_QT_ROOT (echo error: SURFPANEL_QT_ROOT must point to the Qt6 installation prefix. & exit /b 1)

cg-debug: check-env
	@$(CMAKE) -G Ninja -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug

cg-release: check-env
	@$(CMAKE) -G Ninja -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

build: check-env
	@$(CMAKE) --build $(BUILD_DIR) --verbose

test:
	@ctest --test-dir $(BUILD_DIR) --output-on-failure

pack:
	cmd //c pack.bat

clean:
	@rm -rf build
