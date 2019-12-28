#include "socket.h"

#include <sys/socket.h>
#include <unistd.h>

mysocket::mysocket() : fd(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) {}

mysocket::mysocket(int server_socket) : fd(accept4(server_socket, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK)) {}

mysocket::mysocket(int timeout, itimerspec &ts) : fd(timerfd_create(CLOCK_MONOTONIC, 0)) {
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;
    ts.it_value.tv_sec = timeout;
    ts.it_value.tv_nsec = 0;
}

mysocket::~mysocket() {
    if (fd != -1) {
        close(fd);
    }
}

bool mysocket::operator==(const struct mysocket &rhs) {
    return rhs.get_fd() == fd;
}

int mysocket::get_fd() const {
    return fd;
}
