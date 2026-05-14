/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:48:53 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/14 19:57:49 by uanglade         ###   ########.fr       */
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
    logger::info("Creating server with: \n\troot_location: {}\n\troot_path: "
                 "{}\n\tserver_name: {}",
        _root_location, _root_path, _server_name);
    if (listen_addr[0] == '[') {
        _is_ipv6 = true;
        _sockaddr6.sin6_family = AF_INET6;

        std::string addr = listen_addr.substr(1, listen_addr.find(']'));
        inet_pton(AF_INET6, addr.c_str(), &_sockaddr6.sin6_addr);

        _sockaddr6.sin6_port = htons(static_cast<uint16_t>(
            // listen_addr should be valid at this point in time
            // NOLINTNEXTLINE(cert-err34-c,bugprone-unchecked-string-to-number-conversion)
            std::atoi(&listen_addr[listen_addr.find("]:") + 2])));
    } else {
        _is_ipv6 = false;
        _sockaddr.sin_family = AF_INET;

        std::string addr = listen_addr.substr(0, listen_addr.find(':'));
        inet_pton(AF_INET, addr.c_str(), &_sockaddr.sin_addr);

        _sockaddr.sin_port = htons(static_cast<uint16_t>(
            // listen_addr should be valid at this point in time
            // NOLINTNEXTLINE(cert-err34-c,bugprone-unchecked-string-to-number-conversion)
            std::atoi(&listen_addr[listen_addr.find(':') + 1])));
    }
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
    int32_t domain = _is_ipv6 ? AF_INET6 : AF_INET;
    _sockfd = socket(domain, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (_sockfd == -1) {
        perror("socket");
        return false;
    }

    int32_t opt = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))
        == -1) {
        perror("setsockopt");
        close(_sockfd);
        _sockfd = -1;
        return false;
    }

    sockaddr *addr = _is_ipv6 ? reinterpret_cast<sockaddr *>(&_sockaddr6)
                              : reinterpret_cast<sockaddr *>(&_sockaddr);
    socklen_t addrlen = _is_ipv6 ? sizeof(_sockaddr6) : sizeof(_sockaddr);
    if (bind(_sockfd, addr, addrlen) == -1) {
        perror("bind");
        close(_sockfd);
        _sockfd = -1;
        return false;
    }

    if (listen(_sockfd, SOMAXCONN) == -1) {
        perror("listen");
        close(_sockfd);
        _sockfd = -1;
        return false;
    }

    epoll_event ev = {};
    ev.events = EPOLLIN;
    ev.data.fd = _sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, _sockfd, &ev) == -1) {
        perror("epoll_ctl: _sockfd");
        close(_sockfd);
        _sockfd = -1;
        return false;
    }

    return true;
}

std::ostream &operator<<(std::ostream &os, const Location &location)
{
    os << "Location {\n";
    os << "\tpath: " << location.path << ",\n";
    os << "\tconfig: " << location.config << ",\n";
    os << "\texact: " << std::boolalpha << location.exact << ",\n";

    os << "\tchildren: [";

    if (!location.children.empty()) {
        os << '\n';

        for (size_t i = 0; i < location.children.size(); ++i) {
            os << "\t\t" << location.children[i];

            if (i + 1 < location.children.size())
                os << ",";

            os << '\n';
        }

        os << "\t";
    }

    os << "]\n";
    os << "}";

    return os;
}

std::ostream &operator<<(std::ostream &os, const Config &config)
{
    os << "Config {\n";
    os << "\troot: " << config.root << ",\n";

    os << "\tallowed methods: [";
    for (uint8_t i = 0; i < 9; i++) {
        std::string methods_strings[8];
        if (config.allowed_methods[i] != http::methods::COUNT) {
            os << methods_strings[config.allowed_methods[i]];
        }
    }

    return os;
}
