.PHONY: cg-debug cg-release build test pack clean

CMAKE_PREFIX_PATH = "C:\Users\shinku\AppData\Local\msys2\home\shinku\lib\Qt6.11.1"

cg-debug:
	@cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$(CMAKE_PREFIX_PATH)

cg-release:
	@cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(CMAKE_PREFIX_PATH)

build:
	@cmake --build build --verbose

test:
	@ctest --test-dir build --output-on-failure

pack:
	cmd //c pack.bat

clean:
	@rm -rf build