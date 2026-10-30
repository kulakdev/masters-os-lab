include(CheckSymbolExists)

set(CMAKE_REQUIRED_LIBRARIES rt)
check_symbol_exists(mq_open "mqueue.h" HAVE_MQUEUE)
unset(CMAKE_REQUIRED_LIBRARIES)

add_executable(ipc_benchmark
    src/main.c
    src/common.c
    src/channel.c
    src/ring.c
    src/bench_pipe.c
    src/bench_fifo.c
    src/bench_socket.c
    src/bench_file.c
    src/bench_mmap.c
    src/bench_shm.c
    src/bench_mqueue.c
)

if(HAVE_MQUEUE)
    target_compile_definitions(ipc_benchmark PRIVATE HAVE_MQUEUE)
    target_link_libraries(ipc_benchmark rt)
endif()
