set(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty)
set(WINDOWS_PATH_SUFFIXES win64 Win64 x64)

## OpenVR lib paths
set(OPENVR_BINARY_DIR ${THIRDPARTY_DIR}/openvr/bin/win64)
set(OPENVR_BINARY_FILES openvr_api.dll)
find_library(OPENVR_LIBRARIES
  NAMES
    openvr_api
  PATHS
    ${THIRDPARTY_DIR}/openvr/lib
  PATH_SUFFIXES
    ${WINDOWS_PATH_SUFFIXES}
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
set(OPENVR_INCLUDE_DIR ${THIRDPARTY_DIR}/openvr/headers)

## zeromq lib paths
set(ZMQ_BINARY_DIR ${THIRDPARTY_DIR}/zmq/bin/win64)
set(ZMQ_BINARY_FILES libzmq-v142-mt-4_3_2.dll)
find_library(ZMQ_LIBRARIES
  NAMES
    libzmq-v142-mt-4_3_2
  PATHS
    ${THIRDPARTY_DIR}/zmq/lib
  PATH_SUFFIXES
    ${WINDOWS_PATH_SUFFIXES}
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
set(ZMQ_INCLUDE_DIR ${THIRDPARTY_DIR}/zmq/include)
message(${ZMQ_LIBRARIES})

## zmq cpp binding header-only library
set(CPPZMQ_INCLUDE_DIR ${THIRDPARTY_DIR}/cppzmq)

## MathFU Library with test and benchmark builds disabled
set(mathfu_build_benchmarks OFF CACHE BOOL "")
set(mathfu_build_tests OFF CACHE BOOL "")
add_subdirectory($ENV{MATHFU_ROOT} mathfu)

## Generated GLAD OpenGL loader
set(GLAD_INCLUDE_DIR ${THIRDPARTY_DIR}/glad/include)

## Spdlog header-only logging library
set(SPDLOG_INCLUDE_DIR ${THIRDPARTY_DIR}/spdlog/include)

## header-only json library
set(JSON_INCLUDE_DIR ${THIRDPARTY_DIR}/nlohmann)

## Project Source files
set(ALL_SRCS
	app.rc
	appovrly.cc
	appovrly.h
  d3d.cc
  d3d.h
  events.h
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
  ranges.hpp
	resource.h
  serralize.hpp
	uiovrly.cc
	uiovrly.h
	uiovrly_win.cc
  vrovrly.cc
  vrovrly.h
  webovrly.cc
  webovrly.h
)

if(MSVC)
  #Fix nonconformant __cplusplus define
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
endif()

# Logical target used to link the libcef library.
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

# Determine the target output directory.
SET_CEF_TARGET_OUT_DIR()

#include the subdirectory containing our libs
include_directories(${_CEF_ROOT} ${OPENVR_INCLUDE_DIR} ${GLAD_INCLUDE_DIR} ${SPDLOG_INCLUDE_DIR} ${JSON_INCLUDE_DIR} ${ZMQ_INCLUDE_DIR} ${CPPZMQ_INCLUDE_DIR})

#indicate the entry point for the executable
add_executable (ovrly WIN32 ${ALL_SRCS})
SET_EXECUTABLE_TARGET_PROPERTIES(ovrly)

# Configure flags for referencing mathfu
mathfu_configure_flags(ovrly)

## Set C++17 standards mode
set_property(TARGET ovrly PROPERTY CXX_STANDARD 17)
set_property(TARGET ovrly PROPERTY CXX_STANDARD_REQUIRED ON)

source_group("src" FILES ${ALL_SRCS})

add_dependencies(ovrly libcef_dll_wrapper)

# Indicate which libraries to include during the link process.
target_link_libraries (ovrly d3d11.lib Shlwapi.lib libcef_lib libcef_dll_wrapper ${OPENVR_LIBRARIES} ${ZMQ_LIBRARIES})

COPY_FILES(ovrly "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(ovrly "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(ovrly "${OPENVR_BINARY_FILES}" "${OPENVR_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(ovrly "${ZMQ_BINARY_FILES}" "${ZMQ_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")

# copy runtime resources to output directory
file(GLOB_RECURSE ovrly_resources RELATIVE ${CMAKE_SOURCE_DIR}/resource ${CMAKE_SOURCE_DIR}/resource/*)
foreach(_resfile ${ovrly_resources})
	add_custom_command(
			TARGET ovrly POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
					${CMAKE_SOURCE_DIR}/resource/${_resfile}
					${TARGET_OUTPUT_DIRECTORY}/resource/${_resfile})
endforeach()
