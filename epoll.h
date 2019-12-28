#ifndef EPOLL_H
#define EPOLL_H

#include <map>
#include <functional>

struct epoll {
    epoll();

    ~epoll();

    void add_event(int socket, std::function<void(uint32_t)> *ptr);

    void delete_event(int socket);

    void execute();

private:

    int fd, signal_fd;
    bool stop;
};


#endif //EPOLL_H
