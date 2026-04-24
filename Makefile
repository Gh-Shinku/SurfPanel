.PHONY: configure build test clean

configure:
	cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug

build:
	cmake --build build --verbose

test:
	@ctest --output-on-failure

clean:
	rm -rf build