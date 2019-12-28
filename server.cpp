#include "server.h"

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <netinet/in.h>
#include <stdexcept>
#include <netdb.h>
#include <arpa/inet.h>
#include <thread>
#include <cstring>
#include <iostream>
#include <csignal>
#include "zconf.h"

server::server(uint16_t port, epoll &e) : epfd(e),
                                               port(port),
                                               server_socket(),
                                               add_new_socket([this](uint32_t) {
												   std::unique_ptr<client> ptr(new client(server_socket.get_fd(), epfd, this));
												   map.emplace(ptr.get(), std::move(ptr));
                                               }) {
    if (server_socket.get_fd() == -1) {
        throw std::runtime_error("Could not create a socket, the server with port " + std::to_string(port) + " wasn't created");
    }
    sockaddr_in server_address{};
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = 0;
    server_address.sin_family = AF_INET;
    for (;;) {
        int res = bind(server_socket.get_fd(), reinterpret_cast<sockaddr const *>(&server_address), sizeof(server_address));
        if (res != 0) {
            port++;
            server_address.sin_port = htons(port);
        } else {
            std::cout << "OK, the server has port " << port << std::endl;
            break;
        }
    }
    int res = listen(server_socket.get_fd(), SOMAXCONN);
    if (res != 0) {
        throw std::runtime_error("Could not listen, the server with port " + std::to_string(port) +
                                 " wasn't created, the error code is " + std::to_string(res));
    }
    epfd.add_event(server_socket.get_fd(), &add_new_socket);
}

server::~server() {
    if (server_socket.get_fd() != -1) {
        epfd.delete_event(server_socket.get_fd());
    }
}

server::client::client(int socket, epoll &epfd, server *parent) :
        epfd(epfd),
        fd(socket),
        ts(),
        is_waiting(false),
        timer_fd(TIMEOUT, ts),
        is_queued(false),
        kill_client([this, parent](uint32_t events) {
			std::cerr << "KILLED\n";
			parent->map.erase(this);
		}),
        fun([this, parent](uint32_t events) {
            char buf[1024];
            int rc = recv(fd.get_fd(), buf, sizeof(buf), 0);
			if (rc == 0) {
				std::cerr << "KILLED FROM FUN\n";
				parent->map.erase(this);
				return;
			}
			if (rc < 0) {
				std::cout << "Could not get information by socket " << fd.get_fd();
				return;
			}
            std::string request(buf, rc - 2);
            std::unique_lock<std::mutex> lg(m);
            client_requests.push(request);
            if (!is_queued) {
                is_queued = true;
                delete_timer();
                std::thread th([this] {
                    pid.emplace(getpid());
                    for (;;) {
                        std::unique_lock<std::mutex> lg(m);
                        if (client_requests.empty()) {
                            is_queued = false;
                            pid.reset();
                            set_timer();
                            break;
                        } else {
                            std::string request = client_requests.front();
                            client_requests.pop();
                            lg.unlock();
                            std::vector<std::string> ans = handle(request);
                            for (std::string &str : ans) {
                                int status = send(fd.get_fd(), str.c_str(),
                                                  str.length(), 0);
                                if (status < 0) {
                                    std::cerr << "Could not send information to socket " << fd.get_fd() << std::endl;
                                }
                            }
                        }
                    }
                });
                th.detach();
            }
        }) {
    epfd.add_event(fd.get_fd(), &fun);
    set_timer();
}

server::client::~client() {
    if (fd.get_fd() != -1) {
        this->epfd.delete_event(fd.get_fd());
    }
    delete_timer();
    std::unique_lock<std::mutex> lg(m);
    if (pid.has_value()) {
        kill(pid.value(), SIGTERM);
    }
}

bool server::client::operator==(const struct server::client &rhs) {
    return fd == rhs.fd;
}

void server::client::set_timer() {
    int status = timerfd_settime(timer_fd.get_fd(), 0, &ts, nullptr);
    if (status != 0) {
        std::cout << "Could not set timer" << std::endl;
    }
    is_waiting = true;
    epfd.add_event(timer_fd.get_fd(), &kill_client);
}

void server::client::delete_timer() {
    if (is_waiting) {
        epfd.delete_event(timer_fd.get_fd());
    }
    is_waiting = false;
}

std::vector<std::string> server::handle(const std::string &request) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    //hints.ai_flags = AI_PASSIVE;
    addrinfo *server_info = 0;
    int status = getaddrinfo(request.c_str(), "80", &hints, &server_info);
    if (status != 0) {
		freeaddrinfo(server_info);
		if (status == -2)
            return {"Incorrect address of website " + request + '\n'};
        return {"Couldn't get info for " + request + ", error is " + std::to_string(status)};
    }
    std::vector<std::string> ans;
    ans.emplace_back("The IP addresses for " + request + '\n');
    for (auto p = server_info; p != nullptr; p = p->ai_next) {
		std::string address(inet_ntoa(reinterpret_cast<sockaddr_in *>(p->ai_addr)->sin_addr));
        address.push_back('\n');
        ans.emplace_back(address);
    }
    freeaddrinfo(server_info);
    return ans;
}
