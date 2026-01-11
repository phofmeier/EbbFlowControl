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

# Custom command to generate QR code for WiFi credentials
set(QR_CODE_FILE "${CMAKE_SOURCE_DIR}/${CONFIG_WIFI_SOFT_AP_SSID}_wifi_qr.png")
add_custom_command(
    OUTPUT ${QR_CODE_FILE}
    COMMAND ${CMAKE_COMMAND} -E echo "Generating WiFi QR code..."
    COMMAND ./scripts/generate_wifi_qr_code.sh "${CONFIG_WIFI_SOFT_AP_SSID}" "${CONFIG_WIFI_SOFT_AP_PASSWORD}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${SDKCONFIG}
    COMMENT "Generating QR code for SoftAP WiFi credentials"
)

# Custom target to generate QR code
add_custom_target(generate_wifi_qr
    DEPENDS ${QR_CODE_FILE}
)
