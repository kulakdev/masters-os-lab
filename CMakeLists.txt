cmake_minimum_required(VERSION 3.21)
project(Sockets C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_C_FLAGS_RELEASE "-O3 -Wall -Wextra")

include(CheckSymbolExists)
check_symbol_exists(epoll_create1 "sys/epoll.h" HAVE_EPOLL)
check_symbol_exists(kqueue "sys/event.h" HAVE_KQUEUE)

include_directories(shared)

add_subdirectory(lab1)
# add_subdirectory(lab2)
# add_subdirectory(lab3)