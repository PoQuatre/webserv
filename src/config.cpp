/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:44:31 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/14 19:01:17 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <netinet/in.h>
#include <sys/socket.h>

#include <vector>

#include "logger.hpp"
#include "webserv.hpp"

namespace {

std::string dirpart(const std::string &path)
{
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return "."; // no separator → current dir
    if (pos == 0)
        return "/"; // root
    return path.substr(0, pos);
}

}

std::vector<Server> parse_config(const std::string &path)
{
    logger::info("Parsing configs");

    std::vector<Server> servers;
    Location location = {};

    const std::string root = dirpart(path);

    servers.push_back(Server(root, location, "example.com", "0.0.0.0:8080"));

    return servers;
}
