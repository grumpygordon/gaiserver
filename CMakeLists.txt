cmake_minimum_required(VERSION 3.10)
project(addrinfo_server)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11 -w -pedantic -pthread -g -fsanitize=address,undefined")
endif()

add_executable(
        addrinfo_server
        main.cpp
        server.cpp
        server.h
        socket.cpp
        socket.h
        epoll.cpp
        epoll.h
	zconf.h
)
