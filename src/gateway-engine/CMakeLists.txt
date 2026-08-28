cmake_minimum_required(VERSION 3.16)
project(gateway-engine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(MODBUS REQUIRED libmodbus)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
find_package(nlohmann_json REQUIRED)

# Threads and Systemd (for sd_notify watchdog)
find_package(Threads REQUIRED)
find_library(SYSTEMD_LIB systemd)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${MODBUS_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
)

set(SOURCES
    src/main.cpp
    src/config_manager.cpp
    src/modbus_collector.cpp
    src/can_collector.cpp
    src/mqtt_client.cpp
    src/storage_manager.cpp
    src/downlink_handler.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    ${MODBUS_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    nlohmann_json::nlohmann_json
)

if(SYSTEMD_LIB)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SYSTEMD_LIB})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_SYSTEMD=1)
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY config/ DESTINATION /etc/gateway)
