# Generate visual studio sln
.\gen_vs2019.bat cibuild

# Execute the build
Push-Location cibuild
cmake --build . --config Release
Pop-Location
