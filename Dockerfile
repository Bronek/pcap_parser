FROM ubuntu:noble
ENV HOME=/root
WORKDIR ${HOME}

RUN set -ex ;\
    DEBIAN_FRONTEND=noninteractive ;\
    CODENAME=$( . /etc/os-release && echo $VERSION_CODENAME ) ;\
    apt-get update ;\
    apt-get install -y --no-install-recommends \
      lsb-release libc6-dev less vim xxd curl git grep sed gdb zsh make cmake ninja-build \
      python3 python3-pip python3-venv libpcap-dev catch2 gcc-14 g++-14 ;\
    apt-get clean

RUN set -ex ;\
    update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-14 999 ;\
    update-alternatives --install \
      /usr/bin/gcc gcc /usr/bin/gcc-14 100 \
      --slave /usr/bin/g++ g++ /usr/bin/g++-14 \
      --slave /usr/bin/gfortran gfortran /usr/bin/gfortran-14 \
      --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-14 \
      --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-14 \
      --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-14 \
      --slave /usr/bin/gcov gcov /usr/bin/gcov-14 \
      --slave /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-14 \
      --slave /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-14 \
      --slave /usr/bin/lto-dump lto-dump /usr/bin/lto-dump-14 ;\
    update-alternatives --auto cc ;\
    update-alternatives --auto gcc

ENV EDITOR=vim
ENV VISUAL=vim
ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++
ENV CMAKE_GENERATOR=Ninja
ENV CMAKE_BUILD_TYPE=Debug

COPY . ${HOME}
RUN set -ex ;\
    mkdir .build ;\
    cd .build ;\
    cmake .. ;\
    cmake --build . ;\
    ctest

# Intentionally leave build files in place - they take little space

