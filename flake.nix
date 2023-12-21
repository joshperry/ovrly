{
  description = "A flake for building ovrly";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-23.11";

  inputs.cef.url = "https://cef-builds.spotifycdn.com/cef_binary_120.1.9%2Bg3914241%2Bchromium-120.0.6099.109_linux64.tar.bz2";
  inputs.cef.flake = false;

  inputs.mathfu.url = "git+https://github.com/google/mathfu.git?submodules=1";
  inputs.mathfu.flake = false;

  inputs.openvr.url = "github:ValveSoftware/openvr";
  inputs.openvr.flake = false;

  inputs.flake-utils.url = "github:numtide/flake-utils";

  inputs.nixgl.url = "github:guibou/nixGL";
  inputs.nixgl.inputs.nixpkgs.follows = "nixpkgs";

  outputs = { self, nixpkgs, cef, mathfu, openvr, flake-utils, nixgl }:
    flake-utils.lib.eachSystem [ "x86_64-linux" ] (system:
    let
      flakeoverlays = final: prev: {
        spdlog = prev.spdlog.overrideAttrs (old: {
          # OpenVR libs use an older libfmt, this fixes that conflict
          propagatedBuildInputs = [ final.fmt_9 ];
        });
      };
      pkgs = import nixpkgs { inherit system; overlays=[ nixgl.overlay flakeoverlays ]; };
      deps = with pkgs; [
  # cef/chromium deps
        alsa-lib atk cairo cups dbus expat glib libdrm libva libxkbcommon mesa nspr nss pango
        xorg.libX11 xorg.libxcb xorg.libXcomposite xorg.libXcursor xorg.libXdamage
        xorg.libXext xorg.libXfixes xorg.libXi xorg.libXinerama xorg.libXrandr
  # ovrly deps
        cppzmq fmt_9 glfw nlohmann_json spdlog zeromq
      ];

      ovrlyBuild = pkgs.stdenv.mkDerivation {
          name = "ovrly";
          src = self;

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
          ];

          buildInputs = deps;

          cmakeFlags = [
            "-DCEF_ROOT=${cef}"
            "-DMATHFU_DIR=${mathfu}"
            "-DOPENVR_DIR=${openvr}"
            "-DPROJECT_ARCH=x86_64"
            "-DCMAKE_CXX_STANDARD=20"
          ];
        };
      dockerImage = pkgs.dockerTools.buildImage {
        name = "ovrly";
        config =
          let
            FONTCONFIG_FILE = pkgs.makeFontsConf {
              fontDirectories = [ pkgs.freefont_ttf ];
            };
          in
          {
            Cmd = [ "${ovrlyBuild}/bin/ovrly" ];
            Env = [
              "FONTCONFIG_FILE=${FONTCONFIG_FILE}"
            ];
          };
      };
    in {
      packages = {
        ovrly = ovrlyBuild;
        docker = dockerImage;
      };
      defaultPackage = ovrlyBuild;

      devShell = pkgs.mkShell {
        nativeBuildInputs = [
          pkgs.cmake
        ];
        buildInputs = deps;

        # Don't want hardening in our dev debug builds
        hardeningDisable = ["all"];

        # Exports as env-vars so we can find these paths in-shell
        CEF_ROOT=cef;
        MATHFU_DIR=mathfu;
        OPENVR_DIR=openvr;

        cmakeFlags = [
          "-DCMAKE_BUILD_TYPE=Debug"
          "-DCEF_ROOT=${cef}"
          "-DMATHFU_DIR=${mathfu}"
          "-DOPENVR_DIR=${openvr}"
          "-DPROJECT_ARCH=x86_64"
          "-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON"
          "-DCMAKE_CXX_STANDARD=20"
        ];
      };
    }
  );
}
