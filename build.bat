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

rem cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\src\test_network_udp_raw.cpp ..\..\src\win32_network_udp.cpp
cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\src\test_network_udp_package_loss_handler.cpp %platform_cpp_files%
cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\src\test_server_udp_package_loss_handler.cpp %platform_cpp_files%
rem cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\src\test_win32_high_resolution_timer.cpp
rem cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\src\test_server_udp.cpp ..\..\src\win32_network_udp.cpp

popd
