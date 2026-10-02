# Convenience wrapper around CMake (the original `make` workflow still works).
BUILD ?= build

.PHONY: all test benchmark wasm clean

all:
	cmake -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD) -j

test: all
	ctest --test-dir $(BUILD) --output-on-failure

benchmark: all
	./$(BUILD)/scheduler --benchmark 1000 --seed 42 --profile mixed

# Requires the Emscripten SDK (emcmake); outputs web/public/scheduler.mjs
wasm:
	emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
	cmake --build build-wasm --target scheduler_wasm -j
	mkdir -p web/src/wasm && cp build-wasm/scheduler.mjs web/src/wasm/scheduler.mjs

clean:
	rm -rf $(BUILD) build-wasm
