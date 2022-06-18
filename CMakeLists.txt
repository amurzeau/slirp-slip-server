cmake_minimum_required(VERSION 3.1)

project(slirp-server VERSION 0.1.0)

option(BUILD_SHARED_LIBS "Build shared libs" OFF)

add_subdirectory(deps EXCLUDE_FROM_ALL)

add_subdirectory(src)
