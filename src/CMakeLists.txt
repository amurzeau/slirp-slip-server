# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.11)

file(GLOB SOURCES_FILES *.h *.c *.cpp)

set(LOG_LEVEL_TRACE 0)
set(LOG_LEVEL_DEBUG 1)
set(LOG_LEVEL_INFO 2)
set(LOG_LEVEL_WARN 3)
set(LOG_LEVEL_ERROR 4)
set(LOG_LEVEL_CRITICAL 5)
set(LOG_LEVEL_OFF 6)

set(CMAKE_CXX_STANDARD 20)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES_FILES})
target_link_libraries(${PROJECT_NAME} slirp uv_a spdlog)
target_compile_definitions(${PROJECT_NAME} PRIVATE
	SLIRP_SERVER_VERSION="${PROJECT_VERSION}"
	SPDLOG_ACTIVE_LEVEL=${LOG_LEVEL_DEBUG}
)

install(TARGETS ${PROJECT_NAME} DESTINATION ./)
