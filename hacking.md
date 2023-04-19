# Linux

The linux build is implemented as a nixos flake with two target derivations (and a dev shell): ovrly, and docker.
The ovrly derivation is default and builds a normal binary and delivers it and dependencies to `./result/bin`.

The docker derivation builds a docker image, `./result` being a symlink to a targz of the image.
Load this into your local docker image store with `docker load < result`.

To iterate, enter a development shell: `nix develop`.
Run `cmakeConfigurePhase` to prep cmake to do the build (delete `./build` first if it exists), this only needs to be rerun if the cmake config changes (file added, etc).
This will leave you in `./build`.

Run `buildPhase` after any code change to rebuild, and find the compiled binary in `./build/Release` (or `Debug` as the case may be).
This maintains the object cache between builds so they are relatively rapid in most cases.

# Windows

## Containerized Build

NB: The windows build and DirectX support is probably(read: definitely) broken after all the linux/OpenGL compat changes.
May move to cross-compiling windows binaries on linux like chromium does.

The prebuilt container is available on dockerhub in the `joshperry/ovrly` repo. The latest can be grabbed with a simple `docker pull joshperry/ovrly`.

    docker build -t joshperry/ovrly -m 2GB .
    docker run --rm -v h:\dev\ovrly:c:\workspace joshperry/ovrly .\.ci\build.ps1

## Building ZMQ Lib

    cmake -G"Visual Studio 16 2019" -A x64 -DWITH_PERF_TOOL=OFF -DCMAKE_CXX_STANDARD=17 -DBUILD_SHARED=ON -DBUILD_STATIC=OFF -DZMQ_BUILD_TESTS=OFF ..
