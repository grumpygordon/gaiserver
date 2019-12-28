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
#include <cassert>

server::server(uint16_t port, epoll &e) : epfd(e),
                                               port(port),
                                               server_socket(),
                                               add_new_socket([this](uint32_t) {
												   std::unique_ptr<client> ptr(new client(server_socket.get_fd(), epfd, this));
												   map.emplace(ptr.get(), std::move(ptr));
                                               }) {
    if (server_socket.get_fd() == -1) {
        throw std::runtime_error("Failed to create socket for server at port " + std::to_string(port));
    }
    sockaddr_in server_address{};
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = 0;
    server_address.sin_family = AF_INET;
    while (true) {
        int res = bind(server_socket.get_fd(), reinterpret_cast<sockaddr const *>(&server_address), sizeof(server_address));
        if (res != 0) {
            port++;
            server_address.sin_port = htons(port);
        } else {
            std::cout << "OK, port " << port << std::endl;
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
        bpos(0),
        fin{false},
        client_requests{},
        answers{},
        cv{},
        t([this]() {
			while (true) {
				std::unique_lock<std::mutex> lg(m);
				cv.wait(lg, [this]() {
					std::cerr << "zashel\n";
					return fin || !client_requests.empty();
					return !client_requests.empty();
				});
				std::cerr << "YA TUT CV\n";
				if (fin)
					break;
				if (!is_queued) {
					is_queued = true;
					delete_timer();
				}
				while (!client_requests.empty()) {
					std::string request = client_requests.front();
					std::cerr << "LOL\n";
					client_requests.pop();
					std::vector<std::string> ans = handle(request);
					for (auto &i : ans)
						answers.push(i);
				}
				if (answers.empty() && client_requests.empty()) {
					is_queued = false;
					set_timer();
				}
				std::cerr << "CV DONE\n";
			}
		}),
        kill_client([this, parent](uint32_t events) {
			std::cerr << "KILLED\n";
			parent->map.erase(this);
		}),
        fun([this, parent](uint32_t events) {
			if ((events & EPOLLRDHUP) || (events & EPOLLERR) || (events & EPOLLHUP)) {
				std::cerr << "KILLED FROM FUN\n";
				parent->map.erase(this);
				return;
			}
            int rc = recv(fd.get_fd(), buf + bpos, BF - bpos, 0);
			if (rc < 0) {
				std::cerr << "Could not get information by socket " << fd.get_fd() << '\n';
			} else {
				std::cerr << "AFTER READ\n";
				int to = bpos + rc;
				bpos = std::max(0, bpos - 1);
				while (bpos + 1 < to) {
					if (buf[bpos] == '\r' && buf[bpos + 1] == '\n') {					
						std::string request(buf, buf + bpos);
						bpos += 2;
						for (int i = bpos; i < to; i++)
							buf[i - bpos] = buf[i];
						to = to - bpos;
						bpos = 0;
						{
							std::unique_lock<std::mutex> lg(m);
							std::cerr << "PUSH\n";
							client_requests.push(request);
							cv.notify_all();
						}
					} else {
						bpos++;
					}
				}
				bpos = to;
			}
			{
				std::unique_lock<std::mutex> lg(m);
					std::cerr << "YA TUT FUN\n";
				while (!answers.empty()) {
					std::cerr << "HAS SOME\n";
					std::string &str = answers.front();
					int status = send(fd.get_fd(), str.c_str(),
									  str.length(), 0);
					if (status < 0)
						std::cerr << "Failed to send to " << fd.get_fd() << '\n';
					else if (status < str.length())
						answers.front() = answers.front().substr(status);
					else
						answers.pop();
				}
			}
        }) {
	std::cerr << "SNACAHALA TYT\n";
	epfd.add_event(fd.get_fd(), &fun);
    set_timer();
}

server::client::~client() {
	{
		std::unique_lock<std::mutex> lg(m);
		fin = 1;
	}
	std::cerr << "DESTR\n";
	cv.notify_all();
	t.join();
    if (fd.get_fd() != -1)
        this->epfd.delete_event(fd.get_fd());
    delete_timer();
}

bool server::client::operator==(const struct server::client &rhs) {
    return fd == rhs.fd;
}

void server::client::set_timer() {
    int status = timerfd_settime(timer_fd.get_fd(), 0, &ts, nullptr);
    if (status != 0)
        std::cout << "Failed to set timer" << '\n';
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
    addrinfo *server_info = 0;
    int status = getaddrinfo(request.c_str(), "http", &hints, &server_info);
    if (status != 0) {
		freeaddrinfo(server_info);
		if (status == -2)
            return {"Incorrect address of website " + request + '\n'};
        return {"Couldn't get info for " + request + ", error is " + std::to_string(status)};
    }
    std::vector<std::string> ans;
    ans.emplace_back("IPs for " + request + '\n');
    for (auto p = server_info; p != nullptr; p = p->ai_next) {
		std::string address(inet_ntoa(reinterpret_cast<sockaddr_in *>(p->ai_addr)->sin_addr));
        address.push_back('\n');
        ans.emplace_back(address);
    }
    freeaddrinfo(server_info);
    return ans;
}
