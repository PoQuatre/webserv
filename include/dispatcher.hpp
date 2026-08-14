/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dispatcher.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/29 00:00:00 by mle-flem          #+#    #+#             */
/*   Updated: 2026/08/13 22:54:52 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <string>

#include "Server.hpp"
#include "http.hpp"

namespace dispatcher {

const Config &config_for(const http::request &req, const Server &server);

std::string handle(const http::request &req, const Server &server);

bool open_static_file_response(const http::request &req, const Config &cfg,
    std::string &headers, int32_t &filefd);

std::string error_response(http::status::type status);
std::string error_response(
    const http::request &req, const Config &cfg, http::status::type status);

}
