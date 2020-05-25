## Containerized Build

    docker build -t ovrlybuild -m 2GB .
    docker run --rm -v h:\dev\ovrly:c:\workspace ovrlybuild .\.ci\build.ps1

## Building ZMQ Lib

    cmake -G"Visual Studio 16 2019" -A x64 -DWITH_PERF_TOOL=OFF -DCMAKE_CXX_STANDARD=17 -DBUILD_SHARED=ON -DBUILD_STATIC=OFF -DZMQ_BUILD_TESTS=OFF ..


