set(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty)
set(WINDOWS_PATH_SUFFIXES win64 Win64 x64)

## OpenVR API paths
find_library(OPENVR_LIBRARIES
  NAMES
    openvr_api
  PATHS
    ${THIRDPARTY_DIR}/openvr/bin
    ${THIRDPARTY_DIR}/openvr/lib
  PATH_SUFFIXES
    ${WINDOWS_PATH_SUFFIXES}
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
set(OPENVR_INCLUDE_DIR ${THIRDPARTY_DIR}/openvr/headers)

set(ALL_SRCS
	app.rc
	main.cc
	resource.h
	ovrly_app.h
	ovrly_app.cc
	ovrly_handler.h
	ovrly_handler.cc
	ovrly_handler_win.cc
	platform.h
)
	
# Logical target used to link the libcef library.
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

# Determine the target output directory.
SET_CEF_TARGET_OUT_DIR()
	
#include the subdirectory containing our libs
include_directories(${_CEF_ROOT} ${OPENVR_INCLUDE_DIR})

#indicate the entry point for the executable
add_executable (ovrly WIN32 ${ALL_SRCS})
SET_EXECUTABLE_TARGET_PROPERTIES(ovrly)

source_group("src" FILES ${ALL_SRCS})

add_dependencies(ovrly libcef_dll_wrapper)

# Indicate which libraries to include during the link process.
target_link_libraries (ovrly d3d11.lib Shlwapi.lib libcef_lib libcef_dll_wrapper ${OPENVR_LIBRARIES})

COPY_FILES(ovrly "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${TARGET_OUTPUT_DIRECTORY}")
COPY_FILES(ovrly "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${TARGET_OUTPUT_DIRECTORY}")

# copy runtime resources to output directory
file(GLOB_RECURSE ovrly_resources RELATIVE ${CMAKE_SOURCE_DIR}/resource ${CMAKE_SOURCE_DIR}/resource/*)
foreach(_resfile ${ovrly_resources})
	add_custom_command(
			TARGET ovrly POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
					${CMAKE_SOURCE_DIR}/resource/${_resfile}
					${TARGET_OUTPUT_DIRECTORY}/resource/${_resfile})
endforeach()
