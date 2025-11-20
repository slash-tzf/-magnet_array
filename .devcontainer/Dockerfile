FROM ubuntu:22.04

# 避免交互式提示
ENV DEBIAN_FRONTEND=noninteractive

# 安装工具链
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    usbutils \
    openocd \
    gdb-multiarch \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    && apt-get clean -y && rm -rf /var/lib/apt/lists/*

# 解决 gdb-multiarch 名称不统一的问题 (让 VS Code 更好找)
RUN ln -s /usr/bin/gdb-multiarch /usr/bin/arm-none-eabi-gdb