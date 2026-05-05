.PHONY: cg-debug cg-release build test pack clean

cg-debug:
	@cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug

cg-release:
	@cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release

build:
	@cmake --build build --verbose

test:
	@ctest --test-dir build --output-on-failure

pack:
	cmd //c pack.bat

clean:
	@rm -rf build