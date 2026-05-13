/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:44:31 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/13 04:25:59 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <netinet/in.h>
#include <sys/socket.h>

#include <vector>

#include "webserv.hpp"

std::vector<Server> parse_config(const std::string &path)
{
    (void)path;

    std::vector<Server> servers;
    Config config;

    servers.push_back(Server("./test", config, "", "80"));

    return servers;
}
