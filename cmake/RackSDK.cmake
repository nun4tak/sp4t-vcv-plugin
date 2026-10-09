# Mapping of the Rack-SDK build definitions (arch.mk, compile.mk, dep.mk, plugin.mk) to CMake.
#
# Based on https://github.com/qno/vcv-plugin-cmake-example (MIT).
#
# Expects, set before including this file:
#   RACK_SDK_DIR  path to the Rack SDK (the directory holding include/rack.hpp)
#   PLUGIN_NAME   plugin slug, exactly as in plugin.json
# Optional:
#   ADDITIONAL_PLUGIN_DISTRIBUTABLES  extra files/dirs to ship inside the plugin dir
#   PLUGIN_DISTRIBUTABLES_EXCLUDE     files/dirs within the distributables not to
#                                     ship (e.g. res/staging)
#
# Defines the target ${RACK_PLUGIN_LIB} ("plugin"); add your sources to it with
# target_sources().

set(RACK_SDK_VERSION 2.6.6)
message(STATUS "Load RackSDK.cmake (mapping based on Rack-SDK ${RACK_SDK_VERSION})")

# ---------------------------------------------------------------- Rack SDK ---

if (RACK_SDK_DIR STREQUAL "")
  message(FATAL_ERROR "Path to the Rack SDK is missing! Add -DRACK_SDK_DIR=<PATH> to the cmake call.")
endif ()
get_filename_component(RACK_SDK_DIR "${RACK_SDK_DIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
if (EXISTS "${RACK_SDK_DIR}/include/rack.hpp")
  message(STATUS "Using Rack SDK in '${RACK_SDK_DIR}'")
else ()
  message(FATAL_ERROR "Couldn't find 'include/rack.hpp' in '${RACK_SDK_DIR}'")
endif ()

if (PLUGIN_NAME STREQUAL "")
  message(FATAL_ERROR "PLUGIN_NAME variable not set! Set it in the project CMakeLists.txt before including RackSDK.cmake.\
 It must match the plugin slug defined in plugin.json.")
endif ()
message(STATUS "Using PLUGIN_NAME '${PLUGIN_NAME}'")

# The plugin mixes C and C++ in one target. If the two compilers come from
# different toolchains, CMake links the other toolchain's runtime into the
# plugin, which fails late with a missing libgcc helper such as 'emutls_w'.
if (NOT CMAKE_C_COMPILER_ID STREQUAL CMAKE_CXX_COMPILER_ID)
  message(FATAL_ERROR "The C compiler (${CMAKE_C_COMPILER_ID}, ${CMAKE_C_COMPILER}) and the C++ compiler\
 (${CMAKE_CXX_COMPILER_ID}, ${CMAKE_CXX_COMPILER}) are from different toolchains. Configure a fresh build\
 directory with matching compilers, e.g. -DCMAKE_C_COMPILER=cc -DCMAKE_CXX_COMPILER=c++.")
endif ()
# Do not change the RACK_PLUGIN_LIB: Rack loads the shared object by this name.
set(RACK_PLUGIN_LIB plugin)

# ------------------------------------------------------------- arch.mk -------

if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}"
      CACHE STRING "macOS build architecture" FORCE)
endif ()

if (APPLE)
  set(_rack_cpu "${CMAKE_OSX_ARCHITECTURES}")
else ()
  set(_rack_cpu "${CMAKE_SYSTEM_PROCESSOR}")
endif ()

if (_rack_cpu MATCHES "^(arm64|aarch64)$")
  set(RACK_ARCH_CPU arm64)
  set(_rack_march -march=armv8-a+fp+simd)
  set(_rack_arch_define ARCH_ARM64)
elseif (_rack_cpu MATCHES "^(x86_64|amd64|AMD64)$")
  set(RACK_ARCH_CPU x64)
  set(_rack_march -march=nehalem)
  set(_rack_arch_define ARCH_X64)
else ()
  message(FATAL_ERROR "Could not determine the CPU architecture from '${_rack_cpu}'.\
 Rack plugins are built one architecture at a time; universal builds are not supported here.")
endif ()

if (APPLE)
  set(RACK_ARCH_OS mac)
  set(_rack_os_define ARCH_MAC)
elseif (WIN32)
  set(RACK_ARCH_OS win)
  set(_rack_os_define ARCH_WIN)
elseif (UNIX)
  set(RACK_ARCH_OS lin)
  set(_rack_os_define ARCH_LIN)
else ()
  message(FATAL_ERROR "Could not determine the operating system of '${CMAKE_SYSTEM_NAME}'")
endif ()

set(RACK_ARCH_NAME ${RACK_ARCH_OS}-${RACK_ARCH_CPU})
message(STATUS "Building for ${RACK_ARCH_NAME}")

# plugin.mk's RACK_USER_DIR / PLUGINS_DIR: where Rack loads user plugins from.
# `cmake --install <build dir>` drops the plugin directory in there by default.
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if (APPLE)
    set(_rack_user_dir "$ENV{HOME}/Library/Application Support/Rack2")
  elseif (WIN32)
    set(_rack_user_dir "$ENV{LOCALAPPDATA}/Rack2")
  elseif (DEFINED ENV{XDG_DATA_HOME})
    set(_rack_user_dir "$ENV{XDG_DATA_HOME}/Rack2")
  else ()
    set(_rack_user_dir "$ENV{HOME}/.local/share/Rack2")
  endif ()
  set(CMAKE_INSTALL_PREFIX "${_rack_user_dir}/plugins-${RACK_ARCH_NAME}"
      CACHE PATH "Rack user plugin directory" FORCE)
  message(STATUS "Defaulting CMAKE_INSTALL_PREFIX to '${CMAKE_INSTALL_PREFIX}'")
endif ()

# ---------------------------------------------------------- compile.mk -------

# Rack for DAWs loads plugins as shared objects, so everything needs -fPIC.
# Keep default symbol visibility, as the SDK plugin.mk does: Rack looks up the
# plugin callbacks (init(), ...) by name, which fails if they are hidden.
add_compile_options(-fPIC)
# Debugger symbols. These are removed by `strip` in the dist target.
add_compile_options(-g)
add_compile_options(${_rack_march})
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Skipping optimizations for a Debug build")
else ()
  add_compile_options(-O3 -funsafe-math-optimizations -fno-omit-frame-pointer)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wno-vla-extension)
else ()
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wsuggest-override>)
endif ()

if (NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 11)
endif ()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "Using CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}")

# The plugin's compiler may differ from Rack's, so link the C++ runtime
# statically to avoid ABI mismatches.
add_link_options($<$<CXX_COMPILER_ID:GNU>:-static-libstdc++>
                 $<$<PLATFORM_ID:Linux>:-static-libgcc>)

# ------------------------------------------------------- SDK interface -------

add_library(RackSDK INTERFACE)
target_include_directories(RackSDK INTERFACE
    "${RACK_SDK_DIR}/include" "${RACK_SDK_DIR}/dep/include")
target_link_directories(RackSDK INTERFACE "${RACK_SDK_DIR}")
# Plugins link to libRack because when Rack itself is a plugin of another
# application, its symbols are not available to later loaded shared libraries.
target_link_libraries(RackSDK INTERFACE Rack)
target_compile_definitions(RackSDK INTERFACE ${_rack_arch_define} ${_rack_os_define})

if (APPLE)
  target_compile_options(RackSDK INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
elseif (WIN32)
  if (NOT MINGW)
    message(FATAL_ERROR "The Rack plugin development environment is only supported for MSYS2/MinGW")
  endif ()
  target_compile_definitions(RackSDK INTERFACE _USE_MATH_DEFINES)
  target_compile_options(RackSDK INTERFACE -municode)
else ()
  # Prevents static variables in the shared object from surviving dlclose().
  target_compile_options(RackSDK INTERFACE -fno-gnu-unique)
  # Rack symlinks /tmp/Rack2 to its system dir so the plugin can find libRack.
  target_link_options(RackSDK INTERFACE "-Wl,-rpath=/tmp/Rack2")
endif ()

# ------------------------------------------------------- plugin target -------

add_library(${RACK_PLUGIN_LIB} MODULE)
target_link_libraries(${RACK_PLUGIN_LIB} PRIVATE RackSDK)

# Rack looks for exactly plugin.dylib / plugin.so / plugin.dll.
set(_rack_plugin_dir "${PROJECT_BINARY_DIR}/${PLUGIN_NAME}")
set_target_properties(${RACK_PLUGIN_LIB} PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${_rack_plugin_dir}")
if (APPLE)
  set_target_properties(${RACK_PLUGIN_LIB} PROPERTIES SUFFIX ".dylib")
  target_link_options(${RACK_PLUGIN_LIB} PRIVATE -undefined dynamic_lookup)
  # The SDK's libRack.dylib has the bare install name "libRack.dylib", which dyld
  # can't resolve. Rack symlinks /tmp/Rack2 to its system dir at startup, so load
  # libRack through it. Done on the built library (not only in dist), so that the
  # build and install outputs are loadable too; re-sign after the edit.
  # Through the toolchain's tools, not the host's: a cross build (the VCV
  # Library builds plugins with one) has no `install_name_tool` of its own, and
  # no `codesign` at all, in which case the signature is left to whoever
  # notarizes the result.
  find_program(RACK_CODESIGN codesign)
  if (RACK_CODESIGN)
    set(_rack_codesign COMMAND ${RACK_CODESIGN} -f -s - $<TARGET_FILE:${RACK_PLUGIN_LIB}>)
  else ()
    set(_rack_codesign "")
    message(STATUS "No codesign: leaving ${RACK_PLUGIN_LIB} unsigned")
  endif ()
  add_custom_command(TARGET ${RACK_PLUGIN_LIB} POST_BUILD
      COMMAND "${CMAKE_INSTALL_NAME_TOOL}" -change libRack.dylib /tmp/Rack2/libRack.dylib $<TARGET_FILE:${RACK_PLUGIN_LIB}>
      ${_rack_codesign}
      COMMENT "Resolving libRack.dylib through /tmp/Rack2")
endif ()

# ------------------------------------------------ distributables (dist) ------

# Relative, as the other distributables below: the copy commands prefix them
# with ${CMAKE_SOURCE_DIR}.
file(GLOB _rack_license RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/LICENSE*")
set(PLUGIN_DISTRIBUTABLES plugin.json res ${_rack_license} ${ADDITIONAL_PLUGIN_DISTRIBUTABLES})
message(STATUS "PLUGIN_DISTRIBUTABLES: ${PLUGIN_DISTRIBUTABLES}")

# Mirror the distributables next to the built library, so ${_rack_plugin_dir} is
# a ready-to-load Rack plugin directory. Refreshed on every build, since panel
# SVGs and plugin.json change more often than the code: custom targets are always
# out of date, unlike POST_BUILD steps of the library that only run on relink.
set(RACK_PLUGIN_DISTRIBUTABLES_TARGET ${RACK_PLUGIN_LIB}_distributables)
add_custom_target(${RACK_PLUGIN_DISTRIBUTABLES_TARGET} ALL)
add_dependencies(${RACK_PLUGIN_DISTRIBUTABLES_TARGET} ${RACK_PLUGIN_LIB})

foreach (_item IN LISTS PLUGIN_DISTRIBUTABLES)
  if (IS_DIRECTORY "${CMAKE_SOURCE_DIR}/${_item}")
    add_custom_command(TARGET ${RACK_PLUGIN_DISTRIBUTABLES_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_SOURCE_DIR}/${_item}" "${_rack_plugin_dir}/${_item}"
        COMMENT "Copying ${_item} into ${PLUGIN_NAME}/")
  else ()
    add_custom_command(TARGET ${RACK_PLUGIN_DISTRIBUTABLES_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_SOURCE_DIR}/${_item}" "${_rack_plugin_dir}/"
        COMMENT "Copying ${_item} into ${PLUGIN_NAME}/")
  endif ()
endforeach ()

# Drop excluded paths from the plugin dir, and thus from install and dist too.
foreach (_item IN LISTS PLUGIN_DISTRIBUTABLES_EXCLUDE)
  add_custom_command(TARGET ${RACK_PLUGIN_DISTRIBUTABLES_TARGET} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_rack_plugin_dir}/${_item}"
      COMMENT "Excluding ${_item} from ${PLUGIN_NAME}/")
endforeach ()

# `cmake --install` drops the whole plugin dir into CMAKE_INSTALL_PREFIX, which
# defaults to Rack's user plugin folder (see CMakeLists.txt).
install(DIRECTORY "${_rack_plugin_dir}" DESTINATION ".")

set(STABLE_PLUGIN_BUILD_TARGET build_plugin)
add_custom_target(${STABLE_PLUGIN_BUILD_TARGET})
add_dependencies(${STABLE_PLUGIN_BUILD_TARGET} ${RACK_PLUGIN_DISTRIBUTABLES_TARGET})

# Quick development install: library and manifest only, no resources.
add_custom_target(${STABLE_PLUGIN_BUILD_TARGET}_quick_install
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_INSTALL_PREFIX}/${PLUGIN_NAME}"
    COMMAND "${CMAKE_COMMAND}" -E copy $<TARGET_FILE:${RACK_PLUGIN_LIB}>
            "${CMAKE_INSTALL_PREFIX}/${PLUGIN_NAME}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/plugin.json"
            "${CMAKE_INSTALL_PREFIX}/${PLUGIN_NAME}"
    COMMENT "Installing ${PLUGIN_NAME} into ${CMAKE_INSTALL_PREFIX}")
add_dependencies(${STABLE_PLUGIN_BUILD_TARGET}_quick_install ${RACK_PLUGIN_LIB})

# ------------------------------------------------------- dist package --------

# Reproduces `make dist`: a stripped copy of the plugin dir, archived as a
# zstd-compressed tar named <Slug>-<Version>-<arch>.vcvplugin.
if (NOT DEFINED PLUGIN_VERSION)
  message(WARNING "PLUGIN_VERSION not set; skipping the plugin_dist target.")
else ()
  set(_rack_dist_dir "${PROJECT_BINARY_DIR}/dist")
  set(_rack_package "${PLUGIN_NAME}-${PLUGIN_VERSION}-${RACK_ARCH_NAME}.vcvplugin")

  add_custom_target(plugin_dist
      COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_rack_dist_dir}"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
              "${_rack_plugin_dir}" "${_rack_dist_dir}/${PLUGIN_NAME}"
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
      COMMENT "Packaging ${_rack_package}")
  add_dependencies(plugin_dist ${RACK_PLUGIN_DISTRIBUTABLES_TARGET})

  set(_rack_dist_lib "${_rack_dist_dir}/${PLUGIN_NAME}/$<TARGET_FILE_NAME:${RACK_PLUGIN_LIB}>")
  if (APPLE)
    # libRack.dylib already resolves through /tmp/Rack2 (see the plugin target).
    # Stripping invalidates the signature (expected, hence the silenced warning),
    # so re-sign.
    if (RACK_CODESIGN)
      set(_rack_dist_codesign COMMAND ${RACK_CODESIGN} -f -s - "${_rack_dist_lib}")
    else ()
      set(_rack_dist_codesign "")
    endif ()
    add_custom_command(TARGET plugin_dist POST_BUILD
        COMMAND "${CMAKE_STRIP}" -no_code_signature_warning -S "${_rack_dist_lib}"
        ${_rack_dist_codesign})
  else ()
    add_custom_command(TARGET plugin_dist POST_BUILD
        COMMAND "${CMAKE_STRIP}" -s "${_rack_dist_lib}")
  endif ()

  add_custom_command(TARGET plugin_dist POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E tar cf "${_rack_package}" --zstd -- "${PLUGIN_NAME}"
      WORKING_DIRECTORY "${_rack_dist_dir}")
endif ()
