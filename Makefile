.PHONY: cg-debug cg-release build test pack clean

CMAKE_ARGS ?=

cg-debug:
	@cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug $(CMAKE_ARGS)

cg-release:
	@cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release $(CMAKE_ARGS)

build:
	@cmake --build build --verbose

test:
	@ctest --test-dir build --output-on-failure

pack:
	cmd //c pack.bat

clean:
	@rm -rf build
