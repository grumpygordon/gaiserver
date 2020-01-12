#ifndef GAISERVER_H
#define GAISERVER_H

#include "socket.h"
#include "epoll.h"

#include <cstdint>
#include <queue>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <condition_variable>
#include <thread>
#include <atomic>

struct server {
    explicit server(uint16_t port, epoll &e);

    ~server();

private:
    struct client {
        client(int socket, epoll &epfd, server *parent);

        ~client();

        bool operator==(client const &rhs);

        void set_timer();

        void delete_timer();

        epoll &epfd;
        mysocket fd;

        const int TIMEOUT = 50;
        itimerspec ts;
        mysocket timer_fd;

        std::atomic_bool writable, updated, fin;
        bool has_timer;
        std::mutex m;
        std::thread t;
        std::condition_variable cv;
        std::queue<std::string> client_requests, answers;
        std::function<void(uint32_t)> fun, kill_client;
		static const int BF = 1024;
        char buf[BF];
        int bpos;
    };

    epoll &epfd;
    uint16_t port;
    mysocket server_socket;
    
    std::unordered_map<client*, std::unique_ptr<client> > map;

    std::function<void(uint32_t)> add_new_socket;

    static std::vector<std::string> handle(std::string const &request);
};


#endif //GAISERVER_H
