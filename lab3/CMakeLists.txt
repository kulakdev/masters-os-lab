find_package(Threads REQUIRED)

add_executable(cache_benchmark
        src/main.c
        src/mem_pattern.c
        src/stride.c
        src/latency.c
        src/matmul.c
        src/false_sharing.c
        src/race.c)

target_link_libraries(cache_benchmark PRIVATE Threads::Threads)
