/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:48:53 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/14 08:47:35 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

#include <arpa/inet.h>

#include <cstdlib>

Server::Server(const std::string &root_path, const Location &root_location,
    const std::string &server_name, const std::string &listen_addr)
    : _root_location(root_location)
    , _root_path(root_path)
    , _server_name(server_name)
    , _sockaddr()
    , _sockaddr6()
    , _is_ipv6(false)
{
    if (listen_addr[0] == '[') {
        _is_ipv6 = true;
        _sockaddr6.sin6_family = AF_INET6;

        std::string addr = listen_addr.substr(1, listen_addr.find(']'));
        inet_pton(AF_INET6, addr.c_str(), &_sockaddr6.sin6_addr);

        _sockaddr6.sin6_port = htons(static_cast<uint16_t>(
            // NOLINTNEXTLINE(cert-err34-c) listen_addr should be valid
            std::atoi(&listen_addr[listen_addr.find("]:") + 2])));
    } else {
        _is_ipv6 = false;
        _sockaddr.sin_family = AF_INET;

        std::string addr = listen_addr.substr(0, listen_addr.find(':'));
        inet_pton(AF_INET, addr.c_str(), &_sockaddr.sin_addr);

        _sockaddr.sin_port = htons(static_cast<uint16_t>(
            // NOLINTNEXTLINE(cert-err34-c) listen_addr should be valid
            std::atoi(&listen_addr[listen_addr.find(':') + 1])));
    }
}

Server::Server(const Server &other)
    : _root_location(other._root_location)
    , _root_path(other._root_path)
    , _server_name(other._root_path)
    , _sockaddr(other._sockaddr)
    , _sockaddr6(other._sockaddr6)
    , _is_ipv6(other._is_ipv6)
{
}

Server::~Server() { }
