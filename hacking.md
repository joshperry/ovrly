## Containerized Build

The prebuilt container is available on dockerhub in the `joshperry/ovrly` repo. The latest can be grabbed with a simple `docker pull joshperry/ovrly`.

    docker build -t joshperry/ovrly -m 2GB .
    docker run --rm -v h:\dev\ovrly:c:\workspace joshperry/ovrly .\.ci\build.ps1

## Building ZMQ Lib

    cmake -G"Visual Studio 16 2019" -A x64 -DWITH_PERF_TOOL=OFF -DCMAKE_CXX_STANDARD=17 -DBUILD_SHARED=ON -DBUILD_STATIC=OFF -DZMQ_BUILD_TESTS=OFF ..


