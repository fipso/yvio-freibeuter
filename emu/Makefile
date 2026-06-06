# Thin wrapper around CMake. Run inside `nix-shell`.
BUILD ?= build
ROOT  ?= ..
ARGS  ?=

all: build-cmake

$(BUILD)/CMakeCache.txt:
	cmake -S . -B $(BUILD)

build-cmake: $(BUILD)/CMakeCache.txt
	cmake --build $(BUILD) -j

# Run against the SD-card dump at the repo root (../ from emu/).
run: build-cmake
	./$(BUILD)/yvio-emu --root $(ROOT) $(ARGS)

clean:
	rm -rf $(BUILD)

.PHONY: all build-cmake run clean
