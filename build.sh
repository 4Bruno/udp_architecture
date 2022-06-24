if [[ ! -d build ]]; then
 mkdir build
 mkdir build/debug
 mkdir build/release
fi

# ggdb debugging info for gdb debugger
# -std=gnull enable C99 inline semantics
# serious_c_flags="-m64 -pedantic -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes"
serious_c_flags="-m64 -pedantic -Wshadow -Wpointer-arith -Wcast-qual"
# -std=gnu11
gcc $serious_c_flags -Wall -DDAEMON=0 -DDEBUG=0 -DDISABLE_STDIO=0  -ggdb test_programs/test_network_udp.cpp src/linux_network_udp.cpp -o build/debug/test_network_udp.exe
