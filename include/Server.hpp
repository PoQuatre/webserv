/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:16:25 by nlaporte          #+#    #+#             */
/*   Updated: 2026/06/17 14:04:45 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <netinet/in.h>
#include <regex.h>

#include <iostream>
#include <string>
#include <vector>

#include "config-parser-def.hpp"
#include "http.hpp"

struct Config {
    std::map<uint32_t, std::string> error_pages;
    std::string root;
    std::size_t client_max_body_size;
    bool autoindex;
    bool allowed_methods[http::methods::COUNT];
    config_webserv conf;
};

std::ostream &operator<<(std::ostream &os, const Config &config);

struct Location {
    std::string path;
    Config config;
    location::type type;
    regex_t regexp;
};

std::ostream &operator<<(
    std::ostream &os, const std::vector<Location> &location);

class Server {
public:
    Server(const std::vector<Location> &locations,
        const std::string &server_name, const std::string &listen_addr,
        const Config &default_config);
    Server(const Server &other);
    ~Server();

    bool init(int32_t epollfd);
    void shutdown(int32_t epollfd);
    int32_t get_sockfd() const;

    const Location *find_location(const std::string &uri) const;
    const Config &default_config() const { return _default_config; }
    const std::string &server_name() const { return _server_name; }

private:
    Server();

    const std::vector<Location> _locations;
    const std::string _server_name;
    const Config _default_config;
    struct sockaddr_in _sockaddr;
    struct sockaddr_in6 _sockaddr6;
    int32_t _sockfd;
    bool _is_ipv6;
};
