/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 18:55:47 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/15 13:51:53 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <netinet/in.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "Server.hpp"

bool parse_config(
    std::vector<Server> &servers, const std::string &path, bool recovery);
