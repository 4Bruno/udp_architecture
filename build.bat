@echo off

IF NOT EXIST "build\" (
    mkdir build
)
IF NOT EXIST "build\debug\" (
    mkdir build\debug
)

set build_path=build\debug
set CompilationFlags=/Zi %Optimization% /EHa- /Zo
set platform_cpp_files=..\..\src\win32_network_udp.cpp ..\..\src\win32_time.cpp ..\..\src\MurmurHash3.cpp

cls

pushd %build_path%

cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\src\udp_client.cpp %platform_cpp_files%
cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\src\udp_server.cpp %platform_cpp_files%

popd
