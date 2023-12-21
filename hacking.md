# Linux

The linux build is implemented as a nixos flake with two target derivations
(and a dev shell): ovrly, and docker. The ovrly derivation is default and
builds a normal binary and delivers it and dependencies to `./result/bin`.

The docker derivation builds a docker image, `./result` being a symlink to a
targz of the image. Load this into your local docker image store with
`docker load < result`.

To iterate, enter a development shell: `nix develop --impure`. Run
`cmakeConfigurePhase` to prep cmake to do the build (delete `./build` first if
it exists), this only needs to be rerun if the cmake config changes (file
added, etc). This will leave you in `./build`.

Run `buildPhase` after any code change to rebuild, and find the compiled binary
in `./build/Release` (or `Debug` as the case may be). This maintains the object
cache between builds so they are relatively rapid in most cases.

Currently running this command after any code changes:
`buildPhase && installPhase && LD_LIBRARY_PATH=../outputs/out/bin/ steam-run ../outputs/out/bin/ovrly`

I did get it wrapped to run without `steam-run`, but I'm trying to keep it
running without patching rpath so that it could prospectively run outside of
nix. Though I do need to figure out a better way to get it to look for libs
dropped next to the executable without `LD_LIBRARY_PATH`.

# Windows

NB: The windows build and DirectX support is probably(read: definitely) broken after all the linux/OpenGL compat changes.
May move to cross-compiling windows binaries on linux like chromium does.

## Containerized Build

The prebuilt container is available on dockerhub in the `joshperry/ovrly` repo. The latest can be grabbed with a simple `docker pull joshperry/ovrly`.

    docker build -t joshperry/ovrly -m 2GB .
    docker run --rm -v h:\dev\ovrly:c:\workspace joshperry/ovrly .\.ci\build.ps1

## Building ZMQ Lib

    cmake -G"Visual Studio 16 2019" -A x64 -DWITH_PERF_TOOL=OFF -DCMAKE_CXX_STANDARD=17 -DBUILD_SHARED=ON -DBUILD_STATIC=OFF -DZMQ_BUILD_TESTS=OFF ..

# Graphics

The rendering system is least-common-denominator style, maybe in a slightly bad way.

I started with an overkill library(https://github.com/daktronics/cef-mixer/)
that at the time I didn't have the knowledge yet to know is solving a
completely different problem than ovrly needs. So it kind of a DirectX-centric
interface with advanced features (multi-context), so integrating it with OpenGL
had a little impedance mismatch.

All this to say, this is somewhere I'd like to clean up the interface to be a
bit more tailored.
