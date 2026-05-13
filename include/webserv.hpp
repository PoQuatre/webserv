/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 18:55:47 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/13 03:06:34 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <netinet/in.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "Server.hpp"

struct Config {
    std::string server_name;
    sockaddr_in ip32;
    sockaddr_in6 ip64;
    std::string root_path;
};

std::vector<Server> parse_config(const std::string &path);
