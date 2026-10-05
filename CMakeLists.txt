cmake_minimum_required(VERSION 3.19)

# Must be set before project(). Rack plugins target 10.9 (see the SDK compile.mk),
# but SAF forces macOS 12.0 (and uses macOS 12 Accelerate APIs), so match it.
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS deployment version")

# The slug and version are single-sourced from the plugin manifest.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json" _manifest)
string(JSON PLUGIN_SLUG GET "${_manifest}" slug)
string(JSON PLUGIN_VERSION GET "${_manifest}" version)

project(${PLUGIN_SLUG} VERSION ${PLUGIN_VERSION} LANGUAGES C CXX)

# Rack requires the plugin directory to be named after the slug.
set(PLUGIN_NAME ${PLUGIN_SLUG})

# The SDK is vendored under SDKs; override with -DRACK_SDK_DIR=<path>.
set(RACK_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDKs/Rack-SDK"
    CACHE PATH "Path to the VCV Rack SDK")

# Spatial Audio Framework
#
# Configured and added *before* RackSDK.cmake: RackSDK.cmake sets plugin-only
# compiler flags (notably -march for the Rack plugin's single target arch) via
# add_compile_options(), which is directory-scoped and would otherwise leak
# into SAF's build. SAF itself always targets a universal arm64+x86_64 binary
# on macOS, so an inherited arm64-only -march flag breaks its x86_64 slice.
set(SAF_BUILD_TESTS OFF CACHE BOOL "Build SAF unit tests.")
set(SAF_BUILD_EXAMPLES OFF CACHE BOOL "Build SAF examples.")
set(SAF_ENABLE_SIMD ON CACHE BOOL "Enable the use of SSE3, AVX2, AVX512")
add_subdirectory(SDKs/Spatial_Audio_Framework)

# Files shipped inside the plugin directory besides plugin.json, res/ and LICENSE*.
set(ADDITIONAL_PLUGIN_DISTRIBUTABLES "")
# Files not shipped: panel sources with text not yet converted to paths, the
# prototype panels together with the script that turns them into staging panels,
# and miscellaneous files that are not runtime assets.
set(PLUGIN_DISTRIBUTABLES_EXCLUDE res/staging res/proto res/misc)

# Rack SDK
include(cmake/RackSDK.cmake)

target_sources(${RACK_PLUGIN_LIB} PRIVATE
    src/plugin.cpp
    src/AmbiEncoder.cpp
    src/AmbiStereoDecoder.cpp
    src/AmbiBinauralDecoder.cpp
    src/AmbiRotator.cpp
    src/Decorrelator.cpp
    src/AmbisonicDisplay.cpp
    src/RotationDisplay.cpp
    src/SP4TStereoIn.cpp
    src/SP4TStereoOut.cpp
    src/SP4TEarsOut.cpp
    src/SP4TRotate.cpp
    src/SP4TSplit.cpp
    src/SP4TMerge.cpp
    src/SP4TGain.cpp
    src/SP4TMute.cpp
    src/SP4TFlip.cpp
    src/SP4TSmear.cpp)

target_link_libraries(${RACK_PLUGIN_LIB} PRIVATE saf)
