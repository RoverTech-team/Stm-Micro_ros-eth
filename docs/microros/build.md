---
title: Building the Library
parent: micro-ROS
nav_order: 2
---

# Building the micro-ROS Static Library

The pre-built `libmicroros.a` must be generated on a **Linux machine** with ROS 2 Humble installed, then copied to your project. Follow `microrosWs/step.txt` exactly.

## Step 1 — Clone and navigate

```bash
mkdir -p ~/microros_build && cd ~/microros_build
git clone https://github.com/micro-ROS/micro_ros_stm32cubemx_utils.git
cd micro_ros_stm32cubemx_utils/microros_static_library
source /opt/ros/humble/setup.bash
sudo apt install -y python3-colcon-common-extensions gcc-arm-none-eabi g++-arm-none-eabi
```

## Step 2 — Create `toolchain.cmake`

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_CROSSCOMPILING 1)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_C_COMPILER_WORKS 1 CACHE INTERNAL "")
set(CMAKE_CXX_COMPILER_WORKS 1 CACHE INTERNAL "")
set(FLAGS "-O2 -ffunction-sections -fdata-sections -fno-exceptions \
  -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=softfp \
  -nostdlib --param max-inline-insns-single=500 \
  -DCLOCK_MONOTONIC=0 \
  -D'RCUTILS_LOG_MIN_SEVERITY=RCUTILS_LOG_MIN_SEVERITY_NONE'" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_INIT   "-std=c11 ${FLAGS} -D'__attribute__(x)='" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_INIT "-std=c++11 ${FLAGS} -fno-rtti -D'__attribute__(x)='" CACHE STRING "" FORCE)
set(__BIG_ENDIAN__ 0)
```

## Step 3 — Create `colcon.meta`

```json
{
  "names": {
    "rcl":              { "cmake-args": ["-DBUILD_TESTING=OFF", "-DRCL_COMMAND_LINE_ENABLED=OFF", "-DRCL_LOGGING_ENABLED=OFF"] },
    "rcutils":          { "cmake-args": ["-DENABLE_TESTING=OFF", "-DRCUTILS_NO_FILESYSTEM=ON", "-DRCUTILS_NO_THREAD_SUPPORT=ON", "-DRCUTILS_NO_64_ATOMIC=ON", "-DRCUTILS_AVOID_DYNAMIC_ALLOCATION=ON"] },
    "rosidl_typesupport":{ "cmake-args": ["-DROSIDL_TYPESUPPORT_SINGLE_TYPESUPPORT=ON"] },
    "tracetools":       { "cmake-args": ["-DTRACETOOLS_DISABLED=ON", "-DTRACETOOLS_STATUS_CHECKING_TOOL=OFF"] },
    "microxrcedds_client":{ "cmake-args": ["-DUCLIENT_PIC=OFF", "-DUCLIENT_PROFILE_UDP=ON", "-DUCLIENT_PROFILE_TCP=ON", "-DUCLIENT_PROFILE_DISCOVERY=OFF", "-DUCLIENT_PROFILE_SERIAL=OFF", "-DUCLIENT_PROFILE_STREAM_FRAMING=ON", "-DUCLIENT_PROFILE_CUSTOM_TRANSPORT=ON"] },
    "rmw_microxrcedds": { "cmake-args": ["-DRMW_UXRCE_MAX_NODES=1", "-DRMW_UXRCE_MAX_PUBLISHERS=5", "-DRMW_UXRCE_MAX_SUBSCRIPTIONS=5", "-DRMW_UXRCE_MAX_SERVICES=1", "-DRMW_UXRCE_MAX_CLIENTS=1", "-DRMW_UXRCE_MAX_HISTORY=4", "-DRMW_UXRCE_TRANSPORT=custom"] }
  }
}
```

## Step 4 — Build

```bash
colcon build --merge-install \
  --cmake-toolchain-file toolchain.cmake \
  --cmake-args -DCMAKE_BUILD_TYPE=Release
```

## Step 5 — Package and transfer

```bash
mkdir -p ~/microros_output/libmicroros
find install -name "*.a" -exec cp {} ~/microros_output/libmicroros/ \;
cp -r install/include ~/microros_output/libmicroros/microros_include
cd ~ && tar -czvf microros_library.tar.gz microros_output
# scp microros_library.tar.gz user@mac:/path/to/project
```

## Step 6 — Place in project

Extract and copy into:
```
microrosWs/Micro_ros_eth/microroseth/micro_ros_stm32cubemx_utils/microros_static_library/
```
