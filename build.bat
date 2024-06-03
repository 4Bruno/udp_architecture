@echo off
setlocal

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

set "arg=%~1"
echo %arg% | findstr /i "c" >nul
if %errorlevel% == 0 (
    echo --------------------------------------------------
    echo Building client
    echo --------------------------------------------------
    cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\src\udp_client.cpp ..\..\src\win32_virtual_terminal.cpp ..\..\src\win32_multithread.cpp  %platform_cpp_files%
)
echo %arg% | findstr /i "s" >nul
if %errorlevel% == 0 (
    echo --------------------------------------------------
    echo Building server
    echo --------------------------------------------------
    cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\src\udp_server.cpp ..\..\src\win32_virtual_terminal.cpp %platform_cpp_files%
)
echo %arg% | findstr /i "t" >nul
if %errorlevel% == 0 (
    echo --------------------------------------------------
    echo Building test
    echo --------------------------------------------------
    cl /nologo /Od %CompilationFlags% /I ..\..\src  ..\..\src\test_arch.cpp  %platform_cpp_files%
)

popd
