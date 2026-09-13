if(HAVE_EPOLL)
    set(IOMUX_SRC ../shared/iomux_epoll.c)
elseif(HAVE_KQUEUE)
    set(IOMUX_SRC ../shared/iomux_kqueue.c)
else()
    set(IOMUX_SRC ../shared/iomux_poll.c)
endif()

add_executable(socket_benchmark src/main.c src/common.c src/server.c src/client.c ${IOMUX_SRC})