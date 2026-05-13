/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:44:31 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/13 02:56:47 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>
#include "webserv.hpp"

std::vector<Server> parse_config(const std::string &path){
	(void)path;

	std::vector<Server> servers;
	Config config;

	config.server_name = "test";
	config.ip32.sin_family = AF_INET;
	config.ip32.sin_addr.s_addr = htonl(127 << 24 | 1);
	config.ip32.sin_port = htons(8080);
	config.root_path = "/test";

	servers.push_back(Server(config));

	return servers;
}
