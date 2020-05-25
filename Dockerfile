# escape=`

# Use the latest Windows Server Core image with .NET Framework 4.8.
FROM mcr.microsoft.com/dotnet/framework/sdk:4.8-windowsservercore-ltsc2019

# Restore the default Windows shell for correct batch processing.
SHELL ["cmd", "/S", "/C"]

# Use temp dir for environment setup
WORKDIR C:\TEMP

# Download the Build Tools bootstrapper.
ADD https://aka.ms/vs/16/release/vs_buildtools.exe vs_buildtools.exe

# Install Build Tools with msvc excluding workloads and components with known issues.
RUN vs_buildtools.exe --quiet --wait --norestart --nocache `
    --installPath C:\BuildTools `
    --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended `
    --add Microsoft.VisualStudio.Component.VC.ATL `
    --remove Microsoft.VisualStudio.Component.Windows10SDK.10240 `
    --remove Microsoft.VisualStudio.Component.Windows10SDK.10586 `
    --remove Microsoft.VisualStudio.Component.Windows10SDK.14393 `
    --remove Microsoft.VisualStudio.Component.Windows81SDK `
 || IF "%ERRORLEVEL%"=="3010" EXIT 0

#ENV chocolateyVersion 0.10.3
ENV ChocolateyUseWindowsCompression false

# Set your PowerShell execution policy
RUN powershell Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Force

# Install Chocolatey
RUN powershell -NoProfile -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"

# Install Chocolatey packages
RUN choco install git.install bzip2 -y && choco install cmake --version 3.17.2 --installargs 'ADD_CMAKE_TO_PATH=System' -y

# Copy dependency setup script into container
COPY .ci\depsetup.ps1 depsetup.ps1

# Downloaded dependency versions
ENV MATHFU_VERSION master
ENV CEF_VERSION cef_binary_81.3.10+gb223419+chromium-81.0.4044.138_windows64

# Install dependencies
RUN powershell.exe -NoLogo -ExecutionPolicy Bypass .\depsetup.ps1

# Change workdir to build workspace
WORKDIR C:\workspace

# Define the entry point for the docker container.
# This entry point starts the developer command prompt and launches the PowerShell shell.
ENTRYPOINT ["C:\\BuildTools\\Common7\\Tools\\VsDevCmd.bat", "&&", "powershell.exe", "-NoLogo", "-ExecutionPolicy", "Bypass"]
