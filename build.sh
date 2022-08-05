if [[ ! -d build ]]; then
 mkdir build
 mkdir build/debug
 mkdir build/release
fi

clear

# ggdb debugging info for gdb debugger
# -std=gnull enable C99 inline semantics
# serious_c_flags="-m64 -pedantic -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes"
serious_c_flags="-m64 -pedantic -Wshadow -Wpointer-arith -Wcast-qual"
# -std=gnu11
echo "Building test network server"
gcc $serious_c_flags -Wall -DDAEMON=0 -DDEBUG=0 -DDISABLE_STDIO=0  -ggdb  src/linux_time.cpp src/udp_server.cpp src/linux_network_udp.cpp src/MurmurHash3.cpp src/linux_virtual_terminal.cpp -o build/debug/udp_server.exe
