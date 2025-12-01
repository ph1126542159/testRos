cmake_minimum_required(VERSION 3.22)

set(USE_TOOLCHAIN_FILE ON)

if(USE_TOOLCHAIN_FILE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/toolchain.cmake)
endif()


project(testRos CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_PARALLEL_LEVEL 16)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)




find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)


add_executable(${PROJECT_NAME} main.cpp)

ament_target_dependencies(${PROJECT_NAME}
    rclcpp
    sensor_msgs
    std_msgs
    std_srvs
)
