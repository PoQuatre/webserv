/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:48:53 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/14 21:37:45 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "logger.hpp"

Server::Server(const std::string &root_path, const Location &root_location,
    const std::string &server_name, const std::string &listen_addr)
    : _root_location(root_location)
    , _root_path(root_path)
    , _server_name(server_name)
    , _sockaddr()
    , _sockaddr6()
    , _is_ipv6(false)
{
    std::string addr;
    uint32_t port;

    if (listen_addr[0] == '[') {
        _is_ipv6 = true;
        _sockaddr6.sin6_family = AF_INET6;

        addr = listen_addr.substr(1, listen_addr.find(']'));
        inet_pton(AF_INET6, addr.c_str(), &_sockaddr6.sin6_addr);

        // listen_addr should be valid at this point in time
        // NOLINTNEXTLINE(cert-err34-c,bugprone-unchecked-string-to-number-conversion)
        port = std::atoi(&listen_addr[listen_addr.find("]:") + 2]);
        _sockaddr6.sin6_port = htons(static_cast<uint16_t>(port));
    } else {
        _is_ipv6 = false;
        _sockaddr.sin_family = AF_INET;

        addr = listen_addr.substr(0, listen_addr.find(':'));
        inet_pton(AF_INET, addr.c_str(), &_sockaddr.sin_addr);

        // listen_addr should be valid at this point in time
        // NOLINTNEXTLINE(cert-err34-c,bugprone-unchecked-string-to-number-conversion)
        port = std::atoi(&listen_addr[listen_addr.find(':') + 1]);
        _sockaddr.sin_port = htons(static_cast<uint16_t>(port));
    }
    logger::info("Creating server with: \n\troot_location: {}\n\troot_path: "
                 "{}\n\tserver_name: {}\n\tis ipv6: {}\n\taddr: {}\n\tport: {}",
        _root_location, _root_path, _server_name, _is_ipv6, addr, port);
}

Server::Server(const Server &other)
    : _root_location(other._root_location)
    , _root_path(other._root_path)
    , _server_name(other._root_path)
    , _sockaddr(other._sockaddr)
    , _sockaddr6(other._sockaddr6)
    , _sockfd(-1)
    , _is_ipv6(other._is_ipv6)
{
}

Server::~Server() { }

int32_t Server::get_sockfd() const { return _sockfd; }

bool Server::init(int32_t epollfd)
{
    logger::info("Initializing server, name : {}", _server_name);

    int32_t domain = _is_ipv6 ? AF_INET6 : AF_INET;
    _sockfd = socket(domain, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (_sockfd == -1) {
        logger::error("Failed to create socket");
        perror("socket");
        return false;
    }

    logger::info("Created socket: {}", _sockfd);

    int32_t opt = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))
        == -1) {
        logger::error("Failed to set socket options");
        perror("setsockopt");
        close(_sockfd);
        _sockfd = -1;
        return false;
    }

    sockaddr *addr = _is_ipv6 ? reinterpret_cast<sockaddr *>(&_sockaddr6)
                              : reinterpret_cast<sockaddr *>(&_sockaddr);
    socklen_t addrlen = _is_ipv6 ? sizeof(_sockaddr6) : sizeof(_sockaddr);
    if (bind(_sockfd, addr, addrlen) == -1) {
        logger::error("Failed to bind socket");
        perror("bind");
        close(_sockfd);
        _sockfd = -1;
        return false;
    }

    logger::info("Binded socket to address: {}", addr);

    if (listen(_sockfd, SOMAXCONN) == -1) {
        logger::error("Failed to listen on socket");
        perror("listen");
        close(_sockfd);
        _sockfd = -1;
        return false;
    }

    epoll_event ev = {};
    ev.events = EPOLLIN;
    ev.data.fd = _sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, _sockfd, &ev) == -1) {
        logger::error("Failed to control epoll instance");
        perror("epoll_ctl: _sockfd");
        close(_sockfd);
        _sockfd = -1;
        return false;
    }

    logger::info("Successfully created instance");

    return true;
}

std::ostream &operator<<(std::ostream &os, const Location &location)
{
    os << "{";
    os << "path: " << location.path << ", ";
    os << "config: " << location.config << ", ";
    os << "exact: " << std::boolalpha << location.exact << ", ";

    os << "children: [";

    if (!location.children.empty()) {
        for (size_t i = 0; i < location.children.size(); ++i) {
            os << " " << location.children[i];

            if (i + 1 < location.children.size())
                os << ",";
        }
    }

    os << "]";
    os << "}";

    return os;
}

std::ostream &operator<<(std::ostream &os, const Config &config)
{
    os << "{";
    os << "root: " << config.root << ", ";

    os << "allowed methods: [";
    for (uint8_t i = 0; i < 9; i++) {
        if (config.allowed_methods[i]) {
            os << http::methods::strings[i];
            os << ", ";
        }
    }
    os << "], ";
    os << "autoindex: " << std::boolalpha << config.autoindex << ", ";
    os << "client max body size: " << config.client_max_body_size << ", ";
    os << "error pages: [";
    for (uint32_t i = 0; i < 512; i++) {
        if (config.error_pages[i]) {
            os << i << ": " << config.error_pages[i] << ", ";
        }
    }
    os << "]";
    os << "}";
    return os;
}
