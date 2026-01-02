# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

option(BUILD_FACTORY "Build the factory app" OFF)
message(STATUS "Build Factory Flag: ${BUILD_FACTORY}")

set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if(BUILD_FACTORY)
project(EbbFlowControl-factory)
message(STATUS "Build Factory")
else()
project(EbbFlowControl)
message(STATUS "Build Normal Application")
endif()
