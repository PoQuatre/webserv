/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:16:25 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/28 02:41:32 by nlaporte         ###   ########.fr       */
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
    Server(const std::string &root_path,
        const std::vector<Location> &root_location,
        const std::string &server_name, const std::string &listen_addr);
    Server(const Server &other);
    ~Server();

    bool init(int32_t epollfd);
    void shutdown(int32_t epollfd);
    int32_t get_sockfd() const;

private:
    Server();

    const std::vector<Location> _root_location;
    const std::string _root_path;
    const std::string _server_name;
    struct sockaddr_in _sockaddr;
    struct sockaddr_in6 _sockaddr6;
    int32_t _sockfd;
    bool _is_ipv6;
};
