# =============================================
# toolchain.cmake - 交叉编译专用
# =============================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 1. 交叉编译器
set(CMAKE_C_COMPILER /home/yifan/penghui/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /home/yifan/penghui/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++)

# 2. sysroot
set(CMAKE_SYSROOT /home/yifan/penghui/work/sysroot)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu")

# 3. 强制搜索 sysroot（关键！）
set(CMAKE_FIND_ROOT_PATH
    ${CMAKE_SYSROOT}
    /home/yifan/penghui/work/arm_ros/ros/humble
    /home/yifan/penghui/work/ros_ws_demo/sysroot_docker/usr_x5/lib/aarch64-linux-gnu/cmake/spdlog
    /home/yifan/penghui/work/camera_depth/node_moudles/poco/lib/cmake/Poco
    /home/yifan/penghui/work/camera_depth/node_moudles/poco/lib/cmake/OSP
    /home/yifan/penghui/work/camera_depth/node_moudles/poco/lib/cmake/PocoTrace
    /home/yifan/penghui/work/sysroot/usr/lib/cmake/fmt
)

# 4. 库搜索路径（链接器用）
link_directories(
    ${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu
    ${CMAKE_SYSROOT}/usr/lib
    /home/yifan/penghui/work/camera_depth/node_moudles/poco/lib
    /home/yifan/penghui/work/sysroot/usr/lib
)

# 5. CMake 包搜索路径（find_package 用）
list(APPEND CMAKE_PREFIX_PATH
    ${CMAKE_SYSROOT}
    /home/yifan/penghui/work/arm_ros/ros/humble
    /home/yifan/penghui/work/ros_ws_demo/sysroot_docker/usr_x5/lib/aarch64-linux-gnu/cmake/spdlog
    /home/yifan/penghui/work/camera_depth/node_moudles/poco/lib/cmake/Poco
    /home/yifan/penghui/work/camera_depth/node_moudles/poco/lib/cmake/OSP
    /home/yifan/penghui/work/camera_depth/node_moudles/poco/lib/cmake/PocoTrace
    /home/yifan/penghui/work/camera_depth/node_moudles/poco/lib/cmake/ServiceHelper
    /home/yifan/penghui/work/sysroot/usr/lib/cmake/fmt
)

# 6. 库搜索路径（CMake 内部用）
list(APPEND CMAKE_LIBRARY_PATH
    ${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu
    ${CMAKE_SYSROOT}/usr/lib
    /home/yifan/penghui/work/camera_depth/node_moudles/poco/lib
    /home/yifan/penghui/work/sysroot/usr/lib
)

# 7. 搜索规则（必须加！）
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
