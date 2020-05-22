$ProgressPreference = "SilentlyContinue"
$ErrorActionPreference="Stop"

######
# Install external dependencies
######

# Install cmake
$CMAKE_VERSION="3.17.2"
Invoke-WebRequest -URI "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-win64-x64.zip" -OutFile "$Env:HOMEPATH\cmake-$CMAKE_VERSION-win64-x64.zip"
Expand-Archive "$Env:HOMEPATH\cmake-$CMAKE_VERSION-win64-x64.zip" -DestinationPath "$Env:ProgramFiles"
Rename-Item "$Env:ProgramFiles\cmake-$CMAKE_VERSION-win64-x64" -NewName CMake

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    New-Alias -Name cmake -Value "$Env:ProgramFiles\CMake\bin\cmake.exe"
}

# Get mathfu
Push-Location $Env:HOMEPATH
git clone --recursive https://github.com/google/mathfu.git

$MATHFU_ROOT="$Env:HOMEPATH\mathfu"

# Get cef
$CEF_VERSION="cef_binary_81.3.10+gb223419+chromium-81.0.4044.138_windows64"
$CEF_VERSION_ENC=[uri]::EscapeDataString($CEF_VERSION)
Invoke-WebRequest -URI "http://opensource.spotify.com/cefbuilds/$CEF_VERSION_ENC.tar.bz2" -OutFile "$Env:HOMEPATH\$CEF_VERSION.tar.bz2"
bunzip2 -d "$CEF_VERSION.tar.bz2"
tar xvf "$CEF_VERSION.tar"

$CEF_ROOT="$Env:HOMEPATH\$CEF_VERSION"

Pop-Location

######
# Build
######

# Generate visual studio sln
gen_vs2019.bat

# Execute the build
Push-Location build
cmake --build . --config Release
Pop-Location
