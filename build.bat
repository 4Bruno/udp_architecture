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

cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\test_programs\test_network_udp.cpp ..\..\src\win32_network_udp.cpp
cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\test_programs\test_server_udp.cpp ..\..\src\win32_network_udp.cpp
rem cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\test_programs\test_win32_high_resolution_timer.cpp

popd
