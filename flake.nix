{
  description = "A flake for building ovrly";

  inputs.nixpkgs.url = github:nixos/nixpkgs;

  inputs.cef.url = https://cef-builds.spotifycdn.com/cef_binary_111.2.7+gebf5d6a+chromium-111.0.5563.148_linux64_minimal.tar.bz2;
  inputs.cef.flake = false;

  inputs.mathfu.url = git+https://github.com/google/mathfu.git?submodules=1;
  inputs.mathfu.flake = false;

  outputs = { self, nixpkgs, cef, mathfu }: {
    packages.x86_64-linux.default =
      with import nixpkgs { system = "x86_64-linux"; };
      stdenv.mkDerivation {
        name = "ovrly";
        src = self;

        nativeBuildInputs = [
          cmake
          ninja
        ];

        buildInputs = [
# cef/chromium deps
          alsa-lib
          atk
          cairo
          cups
          dbus
          expat
          glib
          libdrm
          libxkbcommon
          mesa
          nspr
          nss
          pango
          xorg.libX11
          xorg.libxcb
          xorg.libXcomposite
          xorg.libXcursor
          xorg.libXdamage
          xorg.libXext
          xorg.libXfixes
          xorg.libXi
          xorg.libXinerama
          xorg.libXrandr
# ovrly deps
          cppzmq
          fmt
          glfw
          nlohmann_json
          spdlog
          zeromq
        ];

        cmakeFlags = [
          "-DCEF_ROOT=${cef}"
          "-DMATHFU_DIR=${mathfu}"
          "-DPROJECT_ARCH=x86_64"
          "-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON"
          "-DCMAKE_CXX_STANDARD=17"
        ];
      };
  };
}
