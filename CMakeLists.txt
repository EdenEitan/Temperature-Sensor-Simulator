cmake_minimum_required(VERSION 3.16)

project(TemperatureSensorSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(sensor_core STATIC
    src/Sensor.cpp
    src/Protocol.cpp
)
target_include_directories(sensor_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(device_sim src/main.cpp)
target_link_libraries(device_sim PRIVATE sensor_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_sensor tests/cpp/test_sensor.cpp)
    target_link_libraries(test_sensor PRIVATE sensor_core)
    target_include_directories(test_sensor PRIVATE tests/cpp)
    add_test(NAME sensor_unit COMMAND test_sensor)

    add_executable(test_protocol tests/cpp/test_protocol.cpp)
    target_link_libraries(test_protocol PRIVATE sensor_core)
    target_include_directories(test_protocol PRIVATE tests/cpp)
    add_test(NAME protocol_unit COMMAND test_protocol)
endif()
