/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:48:53 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/13 04:25:50 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

Server::Server(const std::string &root_path, const Config &config,
    const std::string &server_name, const std::string &listen_addr)
    : _root_config(config)
    , _root_path(root_path)
    , _server_name(server_name)
{
    (void)listen_addr;
    // TODO: parse listen_addr to sockaddr_in or sockaddr_in6
}

Server::~Server() { }
