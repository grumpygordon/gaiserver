#ifndef SOCKET_H
#define SOCKET_H


#include <cstdint>
#include <sys/timerfd.h>

struct mysocket {
    mysocket();

    explicit mysocket(int server_socket);

    mysocket(int timeout, itimerspec &ts);

    mysocket(mysocket const &rhs) = delete;

    ~mysocket();

    bool operator==(mysocket const &rhs);

    mysocket &operator=(mysocket const &rhs) = delete;

    int get_fd() const;

private:
    int fd;
};


#endif //SOCKET_H
