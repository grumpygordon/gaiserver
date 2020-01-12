#ifndef EPOLL_H
#define EPOLL_H

#include <map>
#include <functional>

struct epoll {
    epoll();

    ~epoll();

    bool add_event(int socket, std::function<void(uint32_t)> *ptr);

    bool control(int socket, int op, uint32_t evs, std::function<void(uint32_t)> *ptr);

    bool mod_event(int socket, bool out, std::function<void(uint32_t)> *ptr);

    void delete_event(int socket);

    void execute();

private:

    int fd, signal_fd;
    bool stop;
};


#endif //EPOLL_H
