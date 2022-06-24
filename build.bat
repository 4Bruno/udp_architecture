@echo off

IF NOT EXIST "build\" (
    mkdir build
)
IF NOT EXIST "build\debug\" (
    mkdir build\debug
)

set build_path=build\debug
set CompilationFlags=/Zi %Optimization% /EHa- /Zo

pushd %build_path%

cl /Od %CompilationFlags% /I ..\..\src  ..\..\test_programs\test_network_udp.cpp ..\..\src\win32_network_udp.cpp

popd
