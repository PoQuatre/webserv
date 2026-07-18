/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dispatcher.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/29 00:00:00 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/18 21:58:46 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>

#include "Server.hpp"
#include "http.hpp"

namespace dispatcher {

const Config &config_for(const http::request &req, const Server &server);
std::string filesystem_path(const http::request &req, const Config &cfg);

std::string handle(const http::request &req, const Server &server);

std::string error_response(http::status::type status);
std::string error_response(
    const http::request &req, const Config &cfg, http::status::type status);

}
