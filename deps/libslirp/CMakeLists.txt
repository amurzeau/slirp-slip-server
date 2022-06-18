cmake_minimum_required(VERSION 3.1)

project(slirp VERSION 4.7.0 LANGUAGES C)

set(SLIRP_MAJOR_VERSION ${PROJECT_MAJOR_VERSION})
set(SLIRP_MINOR_VERSION ${PROJECT_MINOR_VERSION})
set(SLIRP_MICRO_VERSION ${PROJECT_PATCH_VERSION})

set(SLIRP_VERSION_STRING "\"${PROJECT_VERSION}\"")

configure_file(
	libslirp/src/libslirp-version.h.in
	${CMAKE_CURRENT_BINARY_DIR}/libslirp/libslirp-version.h
    @ONLY)

file(GLOB SOURCE_FILES
	libslirp/src/*.c
	libslirp/src/*.h
	glib-shim/*.c
	glib-shim/*.h
    ${CMAKE_CURRENT_BINARY_DIR}/libslirp/libslirp-version.h
)

add_library(${PROJECT_NAME} ${SOURCE_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE G_LOG_DOMAIN="Slirp" _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
target_compile_definitions(${PROJECT_NAME} PUBLIC G_BYTE_ORDER=1234)

if(BUILD_SHARED_LIBS)
	target_compile_definitions(${PROJECT_NAME} PRIVATE
		BUILDING_LIBSLIRP
	)
else()
	target_compile_definitions(${PROJECT_NAME} PUBLIC
		BUILDING_LIBSLIRP_STATIC
	)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
	libslirp/src
	${CMAKE_CURRENT_BINARY_DIR}/libslirp
	glib-shim)

if(WIN32)
	target_link_libraries(${PROJECT_NAME} PUBLIC ws2_32 iphlpapi)
	target_compile_definitions(${PROJECT_NAME} PUBLIC
		G_OS_WIN32
	)
else(UNIX)
	target_compile_definitions(${PROJECT_NAME} PUBLIC
		G_OS_UNIX
	)
endif()

add_executable(pingtest libslirp/test/pingtest.c)
target_link_libraries(pingtest PRIVATE ${PROJECT_NAME})

add_executable(ncsitest libslirp/test/ncsitest.c)
target_link_libraries(ncsitest PRIVATE ${PROJECT_NAME})
