# Rack SDK Makefile interface for this CMake plugin.
#
# The VCV Library builds open-source plugins itself rather than taking the
# author's binaries: its toolchain runs `make clean`, `make cleandep`,
# `make dep` and `make dist` in this directory, with RACK_DIR, CC, CXX and
# STRIP set for the platform it is cross-compiling to (see
# VCVRack/rack-plugin-toolchain). Those targets are what this file provides.
#
# It builds nothing itself: it translates that handful of variables and hands
# the work to CMake, so the sources are described in one place only. Everyday
# development does not need it -- see the pixi tasks in README.md.

ifndef RACK_DIR
$(error RACK_DIR is not defined)
endif

SLUG := $(shell jq -r .slug plugin.json)
VERSION := $(shell jq -r .version plugin.json)

# MACHINE (from `$(CC) -dumpmachine`), ARCH_OS, ARCH_CPU and ARCH_NAME, plus
# DEP_PATH and the dep/cleandep targets the toolchain calls.
include $(RACK_DIR)/arch.mk
include $(RACK_DIR)/dep.mk

# Not `build`, so that running this by hand leaves the pixi build directory
# alone, and so that two architectures can be built from one checkout.
BUILD_DIR ?= build-$(ARCH_NAME)

# ------------------------------------------------------------- CMake flags ---

CMAKE_FLAGS += -DCMAKE_BUILD_TYPE=Release
CMAKE_FLAGS += -DRACK_SDK_DIR="$(abspath $(RACK_DIR))"
CMAKE_FLAGS += -DCMAKE_C_COMPILER="$(CC)" -DCMAKE_CXX_COMPILER="$(CXX)"

# The tools the Rack plugin toolchain exports for the platform it targets. Its
# CODESIGN is a command rather than a program ("rcodesign sign", which ad-hoc
# signs Mach-O binaries off a Mac), so it is passed to CMake as a list.
empty :=
space := $(empty) $(empty)
ifdef STRIP
	CMAKE_FLAGS += -DCMAKE_STRIP="$(STRIP)"
endif
ifdef INSTALL_NAME_TOOL
	CMAKE_FLAGS += -DCMAKE_INSTALL_NAME_TOOL="$(INSTALL_NAME_TOOL)"
endif
ifdef CODESIGN
	CMAKE_FLAGS += -DRACK_CODESIGN="$(subst $(space),;,$(strip $(CODESIGN)))"
endif

# Which operating system this make is running on, as arch.mk names them.
HOST_KERNEL := $(shell uname -s)
ifneq (,$(findstring Darwin,$(HOST_KERNEL)))
	HOST_OS := mac
else ifneq (,$(findstring Linux,$(HOST_KERNEL)))
	HOST_OS := lin
else
	HOST_OS := win
endif

# Only when the target is not the host: setting CMAKE_SYSTEM_NAME is what puts
# CMake in cross-compiling mode, and a native build should not be in it.
ifneq ($(ARCH_OS),$(HOST_OS))
	ifdef ARCH_WIN
		CMAKE_FLAGS += -DCMAKE_SYSTEM_NAME=Windows
	endif
	ifdef ARCH_MAC
		CMAKE_FLAGS += -DCMAKE_SYSTEM_NAME=Darwin
	endif
	ifdef ARCH_LIN
		CMAKE_FLAGS += -DCMAKE_SYSTEM_NAME=Linux
	endif
	CMAKE_FLAGS += -DCMAKE_SYSTEM_PROCESSOR=$(firstword $(subst -, ,$(MACHINE)))
endif

ifdef ARCH_MAC
	ifdef ARCH_ARM64
		CMAKE_FLAGS += -DCMAKE_OSX_ARCHITECTURES=arm64
	else
		CMAKE_FLAGS += -DCMAKE_OSX_ARCHITECTURES=x86_64
	endif
endif

# SAF's BLAS/LAPACK back end: the Accelerate framework on macOS, and elsewhere
# the OpenBLAS built by the dep target below. SAF_ENABLE_SIMD is off there
# because SAF compiles itself with -march=native on those platforms, which
# would tie the plugin to the machine that built it.
ifdef ARCH_MAC
	CMAKE_FLAGS += -DSAF_PERFORMANCE_LIB=SAF_USE_APPLE_ACCELERATE_LP64
else
	CMAKE_FLAGS += -DSAF_PERFORMANCE_LIB=SAF_USE_OPEN_BLAS_AND_LAPACKE
	CMAKE_FLAGS += -DSAF_ENABLE_SIMD=OFF
	CMAKE_FLAGS += -DOPENBLAS_HEADER_PATH="$(DEP_PATH)/include"
	CMAKE_FLAGS += -DLAPACKE_HEADER_PATH="$(DEP_PATH)/include"
	CMAKE_FLAGS += -DOPENBLAS_LIBRARY="$(DEP_PATH)/lib/libopenblas.a;-lm"
	CMAKE_FLAGS += -DLAPACKE_LIBRARY=
endif

# ------------------------------------------------------------ dependencies ---

# OpenBLAS, built here rather than taken from the system: the distributions'
# builds are compiled with gfortran, whose static runtime is not position
# independent on Ubuntu and so cannot be linked into the plugin. NOFORTRAN=1
# builds LAPACK (and LAPACKE) from the f2c-translated C sources shipped with
# it, which removes that runtime from the picture entirely.
OPENBLAS_VERSION := 0.3.34
OPENBLAS_SHA256 := cd7e129868320cc2d033afa920e31202dfe0b8066a5b66661900ccc0f197dfed
OPENBLAS_DIR := OpenBLAS-$(OPENBLAS_VERSION)
OPENBLAS_TAR := $(OPENBLAS_DIR).tar.gz

ifdef ARCH_X64
	OPENBLAS_TARGET := NEHALEM
else
	OPENBLAS_TARGET := ARMV8
endif

# One CPU baseline, the one Rack targets, instead of every kernel OpenBLAS
# knows, and no threading layer under the audio thread. USE_LOCKING, because a
# single-threaded build still has to be safe to call from several threads.
OPENBLAS_FLAGS := NOFORTRAN=1 NO_SHARED=1 NEED_PIC=1 DYNAMIC_ARCH=0 \
	TARGET=$(OPENBLAS_TARGET) USE_THREAD=0 USE_LOCKING=1 USE_OPENMP=0 \
	NO_AFFINITY=1 CC="$(CC)" HOSTCC=cc

ifndef ARCH_MAC
DEPS += $(DEP_PATH)/lib/libopenblas.a
endif

$(DEP_PATH)/lib/libopenblas.a:
	$(WGET) "https://github.com/OpenMathLib/OpenBLAS/releases/download/v$(OPENBLAS_VERSION)/$(OPENBLAS_TAR)"
	$(SHA256) $(OPENBLAS_TAR) $(OPENBLAS_SHA256)
	$(UNTAR) $(OPENBLAS_TAR)
	$(MAKE) -C $(OPENBLAS_DIR) $(OPENBLAS_FLAGS)
	$(MAKE) -C $(OPENBLAS_DIR) $(OPENBLAS_FLAGS) PREFIX="$(DEP_PATH)" install

# ------------------------------------------------------------------ targets ---

all: $(DEPS)
	cmake -S . -B "$(BUILD_DIR)" $(CMAKE_FLAGS)
	cmake --build "$(BUILD_DIR)" --parallel

# The plugin directory and the .vcvplugin archive, where the SDK's own dist
# target leaves them.
dist: $(DEPS)
	cmake -S . -B "$(BUILD_DIR)" $(CMAKE_FLAGS)
	cmake --build "$(BUILD_DIR)" --parallel --target plugin_dist
	rm -rf dist
	mkdir -p dist
	cp -r "$(BUILD_DIR)/dist/$(SLUG)" dist/
	cp "$(BUILD_DIR)/dist/$(SLUG)-$(VERSION)-$(ARCH_NAME).vcvplugin" dist/

install: all
	cmake --install "$(BUILD_DIR)"

clean:
	rm -rf "$(BUILD_DIR)" dist

.PHONY: all dist install clean
.DEFAULT_GOAL := all
