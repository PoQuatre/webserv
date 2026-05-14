/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:16:25 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/14 08:40:54 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <netinet/in.h>

#include <string>
#include <vector>

#include "http.hpp"

struct Config {
    const std::string *error_pages[512];
    const std::string root;
    std::size_t client_max_body_size;
    bool autoindex;
    bool allowed_methods[http::methods::COUNT];
};

struct Location {
    const std::string path;
    const Config config;
    std::vector<Location> children;
    bool exact;
};

class Server {
public:
    Server(const std::string &root_path, const Location &root_location,
        const std::string &server_name, const std::string &listen_addr);
    Server(const Server &other);
    ~Server();

    bool init(int32_t epollfd);
    int32_t get_sockfd() const;

private:
    Server();

    const Location _root_location;
    const std::string _root_path;
    const std::string _server_name;
    struct sockaddr_in _sockaddr;
    struct sockaddr_in6 _sockaddr6;
    int32_t _sockfd;
    bool _is_ipv6;
};
