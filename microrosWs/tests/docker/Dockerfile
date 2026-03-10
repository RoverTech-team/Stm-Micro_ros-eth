FROM ubuntu:22.04 AS base

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    python3 \
    python3-pip \
    python3-venv \
    && rm -rf /var/lib/apt/lists/*

FROM base AS arm-toolchain

RUN apt-get update && apt-get install -y \
    gcc-arm-none-eabi \
    g++-arm-none-eabi \
    libnewlib-arm-none-eabi \
    arm-none-eabi-toolchain \
    && rm -rf /var/lib/apt/lists/*

ENV ARM_TOOLCHAIN_PATH=/usr/bin
ENV CROSS_COMPILE=arm-none-eabi-

WORKDIR /firmware
COPY . .

RUN mkdir -p /firmware/build && \
    cd /firmware/Micro_ros_eth/microroseth/CM7 && \
    find . -name "*.c" -o -name "*.h" | head -20

FROM base AS test-env

RUN apt-get update && apt-get install -y \
    gcc \
    g++ \
    cppcheck \
    clang-tidy \
    clang-format \
    clang \
    libclang-dev \
    libfuzzer-14-dev \
    gcov \
    lcov \
    doxygen \
    graphviz \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install \
    pytest \
    pytest-cov \
    pytest-asyncio \
    pexpect \
    pyyaml \
    coloredlogs \
    atheris \
    pythonfuzz \
    hypofuzz

WORKDIR /unity
RUN git clone --depth 1 https://github.com/ThrowTheSwitch/Unity.git . && \
    cp src/unity.h /usr/local/include/ && \
    cp src/unity.c /usr/local/src/ && \
    gcc -c src/unity.c -o /usr/local/lib/unity.o && \
    ar rcs /usr/local/lib/libunity.a /usr/local/lib/unity.o

WORKDIR /project
COPY . .

RUN mkdir -p /project/build /project/coverage

ENV UNITY_ROOT=/unity

FROM base AS renode-env

RUN apt-get update && apt-get install -y \
    wget \
    gnupg \
    software-properties-common \
    mono-complete \
    libicu70 \
    libgtk-3-0 \
    xvfb \
    x11-utils \
    && rm -rf /var/lib/apt/lists/*

RUN wget -qO- https://raw.githubusercontent.com/renode/renode/master/tools/renode_install.sh | bash /dev/stdin 1.14.0

RUN pip3 install \
    robotframework \
    robotframework-seriallibrary \
    robotframework-sshlibrary \
    pexpect \
    pyyaml \
    pytest \
    pytest-asyncio \
    psutil

ENV RENODE_PATH=/opt/renode
ENV PATH="${RENODE_PATH}:${PATH}"
ENV DISPLAY=:99

RUN mkdir -p /tmp/.X11-unix && chmod 1777 /tmp/.X11-unix

WORKDIR /simulation
COPY . .

FROM test-env AS unit-tester

WORKDIR /project/tests

CMD ["pytest", "-v", "--cov=../src", "--cov-report=html:../coverage"]

FROM base AS ros2-base

RUN apt-get update && apt-get install -y \
    software-properties-common \
    curl \
    gnupg \
    lsb-release \
    && curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | tee /etc/apt/sources.list.d/ros2.list > /dev/null \
    && apt-get update \
    && apt-get install -y \
    ros-humble-ros-base \
    ros-humble-micro-ros-msgs \
    python3-colcon-common-extensions \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install \
    micro-ros-agent

ENV ROS_DISTRO=humble
ENV AMENT_PREFIX_PATH=/opt/ros/humble
ENV COLCON_PREFIX_PATH=/opt/ros/humble
ENV ROS_PACKAGE_PATH=/opt/ros/humble/share
ENV ROS_PYTHON_VERSION=3
ENV PYTHONPATH=/opt/ros/humble/lib/python3.10/site-packages:$PYTHONPATH
ENV PATH=/opt/ros/humble/bin:$PATH
ENV LD_LIBRARY_PATH=/opt/ros/humble/lib:$LD_LIBRARY_PATH

WORKDIR /micro_ros_ws

RUN mkdir -p src && \
    cd src && \
    git clone --depth 1 https://github.com/micro-ROS/micro_ros_setup.git || true

WORKDIR /project
COPY . .

FROM ros2-base AS micro-ros-integration

RUN apt-get update && apt-get install -y \
    qemu-system-arm \
    qemu-user-static \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /project/tests

CMD ["python3", "-m", "pytest", "integration/", "-v"]

FROM test-env AS final

RUN apt-get update && apt-get install -y \
    netcat \
    net-tools \
    iputils-ping \
    && rm -rf /var/lib/apt/lists/*

COPY --from=arm-toolchain /usr/bin/arm-none-eabi-* /usr/bin/
COPY --from=ros2-base /opt/ros/humble /opt/ros/humble

ENV ROS_DISTRO=humble
ENV PATH=/opt/ros/humble/bin:$PATH
ENV LD_LIBRARY_PATH=/opt/ros/humble/lib:$LD_LIBRARY_PATH

WORKDIR /project

ENTRYPOINT ["/bin/bash"]
