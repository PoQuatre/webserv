/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:16:25 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/13 05:17:48 by nlaporte         ###   ########.fr       */
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

struct Location
{
	const std::string path;
	const Config config;
	std::vector<Location> children;
	bool exact;
};

class Server {
public:
    Server(const std::string &root_path, const Config &config,
        const std::string &server_name, const std::string &listen_addr);
    ~Server();

private:
    const Config &_root_config;
    const std::string &_root_path;
    const std::string &_server_name;
    struct sockaddr_in _sockaddr;
    struct sockaddr_in6 _sockaddr6;
    bool _is_ipv6;
};
