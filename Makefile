.PHONY: cg-debug cg-release build test clean

cg-debug:
	@cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug

cg-release:
	@cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release

build:
	@cmake --build build --verbose

test:
	@ctest --test-dir build --output-on-failure

clean:
	@rm -rf build