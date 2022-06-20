# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.1)

find_package(Git QUIET)
if(GIT_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --tags --always
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(GIT_VERSION MATCHES "^v")
		string(REGEX REPLACE "^v?([0-9]+\\.[0-9]+(\\.[0-9]+)?).*\$" "\\1" GIT_VERSION_TRIMMED ${GIT_VERSION})
	else()
		set(GIT_VERSION "unknown-rev")
		set(GIT_VERSION_TRIMMED "0.0.0")
	endif()
else()
	set(GIT_VERSION "unknown-rev")
	set(GIT_VERSION_TRIMMED "0.0.0")
endif()

project(slirp-server VERSION "${GIT_VERSION_TRIMMED}")

# option() honors normal variables
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

option(BUILD_SHARED_LIBS "Build shared libs" OFF)
set(BUILD_TESTING OFF)

add_subdirectory(deps EXCLUDE_FROM_ALL)

add_subdirectory(src)


set(CPACK_GENERATOR ZIP)
set(CPACK_STRIP_FILES ON)
set(CPACK_PACKAGE_VERSION ${DAMC_GIT_VERSION})

include(CPack)
