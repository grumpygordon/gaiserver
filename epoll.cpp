#include "epoll.h"

#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <stdexcept>
#include <unistd.h>
#include <csignal>
#include <iostream>

epoll::epoll() : fd(epoll_create(1)), stop(0) {
    if (fd == -1) {
        throw std::runtime_error("Could not create epoll");
    }
}

epoll::~epoll() {
    if (fd != -1) {
        close(fd);
    }
}

bool epoll::control(int socket, int op, uint32_t evs, std::function<void(uint32_t)> *ptr) {
    epoll_event ev{};
    ev.events = evs;
    ev.data.ptr = ptr;
    int status = epoll_ctl(fd, op, socket, &ev);
    if (status != 0) {
        std::cout << "Could not add event with socket " << socket << '\n';
        return false;
    }
    return true;
}

bool epoll::add_event(int socket, std::function<void(uint32_t)> *ptr) {
    return control(socket, EPOLL_CTL_ADD, EPOLLIN | EPOLLRDHUP, ptr);
}

bool epoll::mod_event(int socket, bool out, std::function<void(uint32_t)> *ptr) {
    return control(socket, EPOLL_CTL_MOD, EPOLLIN | EPOLLRDHUP | (out ? EPOLLOUT : 0), ptr);
}

void epoll::delete_event(int socket) {
    epoll_ctl(fd, EPOLL_CTL_DEL, socket, nullptr);
}

void epoll::execute() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1) {
        throw std::runtime_error("Could not set SIGINT and SIGTERM as blocked");
    }
    signal_fd = signalfd(-1, &mask, 0);
    if (signal_fd == -1) {
        throw std::runtime_error("Failed to create signalfd");
    }
	std::function<void(uint32_t)> fn = [this](uint32_t events) {
		stop = 1;
		std::cerr << "\nStop signal\n";
	};
	add_event(signal_fd, &fn);
    while (!stop) {
        const size_t K = 32;
        const size_t TIMEOUT = 60 * 1000;
        epoll_event events[K];
        int n = epoll_wait(fd, events, K, TIMEOUT);
        if (n < 0) {
            std::cout << "Wait failed, error " + std::to_string(n) << std::endl;
        }
        //std::cerr << "looping " << n << "\n";
        for (int i = 0; i < n; i++) {
			auto *ptr = reinterpret_cast<std::function<void(uint32_t)> *>(events[i].data.ptr);
			(*ptr)(events[i].events);
        }
    }
}
