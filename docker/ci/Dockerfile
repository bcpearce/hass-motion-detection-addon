FROM ubuntu:24.04 AS build_gcc

RUN apt-get update

# Dependencies for building recent GCC
RUN apt-get install -y \
    curl \
    build-essential \
    git \
    make \
    gawk \
    flex \
    bison \
    libgmp-dev \
    libmpfr-dev \
    libmpc-dev \
    python3 \
    binutils \
    perl \
    libisl-dev \
    libzstd-dev \
    tar \
    gzip \
    bzip2

RUN export CONFIG_SHELL=/bin/bash
RUN mkdir /gcc-15 && mkdir /gcc-15/source && mkdir /gcc-15/build

ARG GCC_REF=releases/gcc-15.1.0

RUN git clone --single-branch --branch ${GCC_REF} https://gcc.gnu.org/git/gcc.git /gcc-15/source
WORKDIR /gcc-15/build
RUN /gcc-15/source/configure --prefix=/opt/gcc-15 --disable-multilib --enable-languages=c,c++
RUN make -j$(nproc)
RUN make install
RUN rm -rf /gcc-15/build

FROM ubuntu:24.04 AS prepare_build_tools

COPY --from=build_gcc /opt /opt

RUN update-alternatives --install /usr/bin/gcc gcc /opt/gcc-15/bin/gcc 100
RUN update-alternatives --install /usr/bin/g++ g++ /opt/gcc-15/bin/g++ 100

# Dependencies for CMake Keyring
RUN apt-get update
RUN apt-get -y install \
    ca-certificates \
    gpg \
    wget

RUN test -f /usr/share/doc/kitware-archive-keyring/copyright || wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
RUN echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main' | tee /etc/apt/sources.list.d/kitware.list >/dev/null
RUN apt-get update
RUN apt-get install -y \
    autoconf \
    autoconf-archive \
    automake \
    bison \
    build-essential \
    cmake \
    curl \
    git \
    libgles2-mesa-dev \
    libltdl-dev \
    libx11-dev \
    libxext-dev \
    libxft-dev \
    libxi-dev \
    libxrandr-dev \
    libxrender-dev \
    libxtst-dev \
    nasm \
    ninja-build \
    pkg-config \
    python3 \
    python3-jinja2 \
    zip
# Required for GitHub Actions
RUN apt-get install -y \
    nodejs \
    npm

# Set up VCPKG
WORKDIR /
RUN git clone https://github.com/microsoft/vcpkg.git

ENV VCPKG_ROOT="/vcpkg"
ENV PATH="$VCPKG_ROOT:/usr/bin/ninja:$PATH"

ARG VCPKG_COMMIT=8f54ef5453e7e76ff01e15988bf243e7247c5eb5

RUN cd vcpkg && git checkout ${VCPKG_COMMIT} && ./bootstrap-vcpkg.sh

WORKDIR /project
COPY build.sh /project/build.sh

RUN ["/bin/bash"]