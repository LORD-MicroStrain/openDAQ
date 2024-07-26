cmake_minimum_required(VERSION 3.20)
project(mscl_device_server VERSION ${OPENDAQ_EXAMPLES_VERSION})

find_package(openDAQ)
add_compile_definitions(MODULE_PATH="${OPENDAQ_MODULES_DIR}")

add_executable(mscl_device_server mscl_device_server.cpp)
target_link_libraries(device_server PRIVATE daq::opendaq MSCL_Static)