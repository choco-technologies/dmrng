# Use the same driver/port build as CMake, including dependency headers.
DMOD_CPU_FAMILY ?= stm32f7
BUILD_DIR ?= build/$(DMOD_CPU_FAMILY)
CMAKE ?= cmake
.PHONY: all configure test clean
all: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel
configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" -DDMOD_CPU_FAMILY="$(DMOD_CPU_FAMILY)" $(if $(DMOD_DIR),-DDMOD_DIR="$(DMOD_DIR)")
test: all
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure
clean:
	$(CMAKE) --build "$(BUILD_DIR)" --target clean
