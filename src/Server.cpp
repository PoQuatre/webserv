/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:48:53 by nlaporte          #+#    #+#             */
/*   Updated: 2026/06/03 06:45:43 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>

#include "logger.hpp"

namespace {

int location_priority(location::type t)
{
    switch (t) {
    case location::STRICT:
        return 0;
    case location::CLASSIC:
    case location::PRIO:
        return 1;
    case location::CASE_SENSITIVE:
    case location::CASE_INSENSITIVE:
        return 2;
    default:
        return 3;
    }
}

struct LocationPriorityLess {
    bool operator()(const Location &a, const Location &b) const
    {
        int pa = location_priority(a.type);
        int pb = location_priority(b.type);
        if (pa != pb)
            return pa < pb;

        // Regex locations (group 2) preserve declaration order via stable_sort
        if (a.type == location::CASE_SENSITIVE
            || a.type == location::CASE_INSENSITIVE)
            return false;

        // Prefix/exact locations: longer paths take precedence (longest match)
        return a.path.size() < b.path.size();
    }
};

std::vector<Location> sort_locations(std::vector<Location> locs)
{
    std::stable_sort(locs.begin(), locs.end(), LocationPriorityLess());
    return locs;
}

}

Server::Server(const std::vector<Location> &locations,
    const std::string &server_name, const std::string &listen_addr,
    const Config &default_config)
    : _locations(sort_locations(locations))
    , _server_name(server_name)
    , _default_config(default_config)
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
    // TODO: private key and certificate parsing
    L_TRACE("Creating server with: \n\tlocations: \n{}\tserver_name: {}\n"
            "\tis ipv6: {}\n\taddr: {}\n\tport: {}",
        _locations, _server_name, _is_ipv6, addr, port);
}

Server::Server(const Server &other)
    : _locations(other._locations)
    , _server_name(other._server_name)
    , _default_config(other._default_config)
    , _sockaddr(other._sockaddr)
    , _sockaddr6(other._sockaddr6)
    , _sockfd(-1)
    , _is_ipv6(other._is_ipv6)
    , _ssl_ctx(other._ssl_ctx)
{
}

Server::~Server() {}

int32_t Server::get_sockfd() const { return _sockfd; }

bool Server::init(int32_t epollfd)
{
    L_DEBUG("Initializing server, name : {}", _server_name);

    int32_t domain = _is_ipv6 ? AF_INET6 : AF_INET;
    _sockfd = socket(domain, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (_sockfd == -1) {
        L_ERROR("Failed to create socket: {}", std::strerror(errno));
        return false;
    }

    L_DEBUG("Created socket: {}", _sockfd);

    int32_t opt = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))
        == -1) {
        L_ERROR("Failed to set socket options: {}", std::strerror(errno));
        close(_sockfd);
        _sockfd = -1;
        return false;
    }

    sockaddr *addr = _is_ipv6 ? reinterpret_cast<sockaddr *>(&_sockaddr6)
                              : reinterpret_cast<sockaddr *>(&_sockaddr);
    socklen_t addrlen = _is_ipv6 ? sizeof(_sockaddr6) : sizeof(_sockaddr);
    if (bind(_sockfd, addr, addrlen) == -1) {
        L_ERROR("Failed to bind socket: {}", strerror(errno));
        close(_sockfd);
        _sockfd = -1;
        return false;
    }

    L_INFO("Listening on address: {}", addr);

    if (listen(_sockfd, SOMAXCONN) == -1) {
        L_ERROR("Failed to listen on socket: {}", strerror(errno));
        close(_sockfd);
        _sockfd = -1;
        return false;
    }

    epoll_event ev = {};
    ev.events = EPOLLIN;
    ev.data.fd = _sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, _sockfd, &ev) == -1) {
        L_ERROR("Failed to add socket {} to epoll instance: {}", _sockfd,
            strerror(errno));
        close(_sockfd);
        _sockfd = -1;
        return false;
    }

    return true;
}

void Server::shutdown(int32_t epollfd)
{
    L_DEBUG("Stopping server {}", _server_name);
    epoll_ctl(epollfd, EPOLL_CTL_DEL, _sockfd, NULL);
    close(_sockfd);
}

std::ostream &operator<<(
    std::ostream &os, const std::vector<Location> &locations)
{
    Location location;
    for (std::vector<Location>::const_iterator cit = locations.begin();
        cit != locations.end(); cit++) {
        location = *cit;
        os << "{";
        os << "type: " << location::strings[location.type] << ", ";
        os << "path: " << location.path << ", ";
        os << "config: " << location.config << "}\n";
    }
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
    for (std::map<uint32_t, std::string>::const_iterator cit
        = config.error_pages.begin();
        cit != config.error_pages.end(); cit++) {
        os << cit->first << ": " << cit->second << ", ";
    }
    os << "]";
    os << "}";
    return os;
}

const Location *Server::find_location(const std::string &uri) const
{
    const Location *longest_match = NULL;

    std::vector<Location>::const_iterator cit = _locations.begin();
    std::vector<Location>::const_iterator cite = _locations.end();

    for (; cit != cite; ++cit) {
        switch (cit->type) {
        case location::STRICT:
            if (uri == cit->path)
                return cit.base();
            break;

        case location::PRIO:
        case location::CLASSIC:
            if (uri.compare(0, cit->path.size(), cit->path) == 0)
                longest_match = cit.base();
            break;

        default:
            goto regex_phase;
        }
    }

regex_phase:
    if (longest_match && longest_match->type == location::PRIO)
        return longest_match;

    for (; cit != cite; ++cit) {
        regmatch_t pmatch[1];
        if (!regexec(&cit->regexp, uri.c_str(), 1, pmatch, REG_STARTEND))
            return cit.base();
    }

    return longest_match;
}
