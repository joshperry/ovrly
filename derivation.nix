{ stdenv
, cmake
, pkg-config
, ninja
, glib
, nss
, nspr
, atk
, cups
, libdrm
, dbus
, mesa
, expat
, libxkbcommon
, pango
, cairo
, alsa-lib
, xorg
, glfw
, zeromq
, cppzmq
, nlohmann_json
, fmt
, spdlog
 } :

stdenv.mkDerivation {
  name = "ovrly";
  src = ./.;
  nativeBuildInputs = [
    cmake
    pkg-config
  ];

  buildInputs = [
    glib
    nss
    nspr
    atk
    cups
    libdrm
    dbus
    mesa
    expat
    xorg.libxcb
    libxkbcommon
    pango
    cairo
    alsa-lib
    xorg.libXcomposite
    xorg.libXdamage
    xorg.libXext
    xorg.libXfixes
    xorg.libX11
    xorg.libXrandr
    xorg.libXinerama
    xorg.libXcursor
    xorg.libXi
    glfw
    zeromq
    cppzmq
    nlohmann_json
    fmt
    spdlog
  ];

  cmakeFlags = [
    "-DCEF_ROOT=${./.}/thirdparty/cef_binary_111.2.6+g491d238+chromium-111.0.5563.65_linux64"
    "-DPROJECT_ARCH=x86_64"
    "-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON"
    "-DCMAKE_CXX_STANDARD=17"
  ];
}
