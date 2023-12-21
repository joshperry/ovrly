set(WINDOWS_PATH_SUFFIXES win64 Win64 x64)
set(LINUX_PATH_SUFFIXES linux64)

find_library(ZMQ_LIB NAMES zmq)
find_library(FMT_LIB NAMES fmt)
find_library(SPDLOG_LIB NAMES spdlog)
find_library(GLFW_LIB NAMES glfw)

## OpenVR lib paths
find_library(OPENVR_LIBRARIES
  NAMES
    openvr_api
  PATHS
    ${OPENVR_DIR}/lib
  PATH_SUFFIXES
    ${WINDOWS_PATH_SUFFIXES} ${LINUX_PATH_SUFFIXES}
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
set(OPENVR_INCLUDE_DIR ${OPENVR_DIR}/headers)
message(${OPENVR_LIBRARIES})
get_filename_component(OPENVR_LIB_DIR ${OPENVR_LIBRARIES} DIRECTORY)
get_filename_component(OPENVR_LIB_FILES ${OPENVR_LIBRARIES} NAME)
message(${OPENVR_LIB_DIR})
message(${OPENVR_LIB_FILES})

## MathFU Library with test and benchmark builds disabled
set(mathfu_build_benchmarks OFF CACHE BOOL "")
set(mathfu_build_tests OFF CACHE BOOL "")
add_subdirectory(${MATHFU_DIR} mathfu)

## Shared Project Source files
set(SHARED_SRCS
	appovrly.cc
	appovrly.h
  events.h
  gfx.cc
  imgovrly.cc
  imgovrly.h
  jsovrly.cc
  jsovrly.h
  logging.cc
  logging.h
	main.cc
  mgrovrly.cc
  mgrovrly.h
  ovrly.cc
  ovrly.h
	platform.h
	resource.h
  serralize.hpp
	uiovrly.cc
	uiovrly.h
  vrovrly.cc
  vrovrly.h
  webovrly.cc
  webovrly.h
)

#include the subdirectory containing our libs
include_directories(${OPENVR_INCLUDE_DIR})

#indicate the entry point for the executable
if(OS_WINDOWS)
  #Fix nonconformant __cplusplus define
  if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
  endif()
  set(PLAT_SRCS
    app.rc
    gfx_d3d.cc
    uiovrly_win.cc
  )
  add_executable (ovrly WIN32 ${SHARED_SRCS} ${PLAT_SRCS})
endif()
if(OS_LINUX)
  list(APPEND CEF_CXX_COMPILER_FLAGS
    -fexceptions                 # Enable exceptions
    -std=c++20
    )
  set(PLAT_SRCS
    gfx_ogl.cc
    uiovrly_linux.cc
  )
  add_executable(ovrly ${SHARED_SRCS} ${PLAT_SRCS})
endif()

SET_EXECUTABLE_TARGET_PROPERTIES(ovrly)

# Configure flags for referencing mathfu
mathfu_configure_flags(ovrly)

source_group("src" FILES ${SHARED_SRCS})
source_group("platsrc" FILES ${PLAT_SRCS})

add_dependencies(ovrly libcef_dll_wrapper)

# Indicate which libraries to include during the link process.
target_link_libraries (ovrly PRIVATE ${OPENVR_LIBRARIES} ${ZMQ_LIB} ${FMT_LIB} ${SPDLOG_LIB} ${GLFW_LIB} libcef_lib libcef_dll_wrapper glib-2.0 nss3 nspr4 atk-1.0 cups drm Xcomposite Xdamage Xext Xfixes dbus-1 gbm expat xcb xkbcommon pango-1.0 cairo asound va)

# Copy OpenVR lib to CEF's output dir
COPY_FILES(ovrly "${OPENVR_LIB_FILES}" "${OPENVR_LIB_DIR}" "${CEF_TARGET_OUT_DIR}")

# Copy static bin/resource files to CEF's output dir
COPY_FILES(ovrly "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
COPY_FILES(ovrly "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")

# copy ovrly runtime resources to CEF's output directory
file(GLOB_RECURSE ovrly_resources RELATIVE ${CMAKE_SOURCE_DIR}/resource ${CMAKE_SOURCE_DIR}/resource/*)
foreach(_resfile ${ovrly_resources})
	add_custom_command(
			TARGET ovrly POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
					${CMAKE_SOURCE_DIR}/resource/${_resfile}
          ${CEF_TARGET_OUT_DIR}/resource/${_resfile})
endforeach()

# Install the ovrly app binary
install(TARGETS ovrly RUNTIME)
