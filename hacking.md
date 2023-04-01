# Linux

    $ mkdir linuxbuild && cd linuxbuild
    $ cmake -G "Ninja" -DPROJECT_ARCH="x86_64" -DCMAKE_BUILD_TYPE=Debug -DOPENVR_LIBRARIES=/overly/thirdparty/openvr \
    -DCEF_ROOT=/overly/thirdparty/cef_binary_111.2.6+g491d238+chromium-111.0.5563.65_linux64 -DZMQ_LIBRARIES=/overly/thirdparty/zmq \
    -DCMAKE_CXX_STANDARD=17 ..
    $ cmake --build .

## ZMQ

    mkdir linuxbuild
    cd linuxbuild
    cmake -G "Ninja" -DPROJECT_ARCH="x86_64" ..
    cmake --build .

# Windows

## Containerized Build

The prebuilt container is available on dockerhub in the `joshperry/ovrly` repo. The latest can be grabbed with a simple `docker pull joshperry/ovrly`.

    docker build -t joshperry/ovrly -m 2GB -f Dockerfile.windows .
    docker run --rm -v h:\dev\ovrly:c:\workspace joshperry/ovrly .\.ci\build.ps1

## Building ZMQ Lib

    cmake -G"Visual Studio 16 2019" -A x64 -DWITH_PERF_TOOL=OFF -DCMAKE_CXX_STANDARD=17 -DBUILD_SHARED=ON -DBUILD_STATIC=OFF -DZMQ_BUILD_TESTS=OFF ..
