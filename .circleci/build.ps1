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

$Env:Path += ";$Env:ProgramFiles\CMake\bin"

# Get mathfu
Push-Location $Env:HOMEPATH
git clone --recursive https://github.com/joshperry/mathfu.git

# Export path via env var
$Env:MATHFU_ROOT="$Env:HOMEPATH\mathfu"

# Get cef
$CEF_VERSION="cef_binary_81.3.10+gb223419+chromium-81.0.4044.138_windows64"
$CEF_VERSION_ENC=[uri]::EscapeDataString($CEF_VERSION)
Invoke-WebRequest -URI "http://opensource.spotify.com/cefbuilds/$CEF_VERSION_ENC.tar.bz2" -OutFile "$Env:HOMEPATH\$CEF_VERSION.tar.bz2"
bunzip2 -d "$CEF_VERSION.tar.bz2"
tar xf "$CEF_VERSION.tar"

# Export path via env var
$Env:CEF_ROOT="$Env:HOMEPATH\$CEF_VERSION"

Pop-Location

######
# Build
######

# Set up msvc environment
cmd.exe /c "call `"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat`" && set > %temp%\vcvars.txt"
Get-Content "$env:temp\vcvars.txt" | Foreach-Object {
  if ($_ -match "^(.*?)=(.*)$") {
    Set-Content "env:\$($matches[1])" $matches[2]
  }
}

# Generate visual studio sln
.\gen_vs2019.bat

# Execute the build
Push-Location build
cmake --build . --config Release
Pop-Location
