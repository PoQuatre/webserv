/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dispatcher.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/29 00:00:00 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/20 16:16:38 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>

#include "Server.hpp"
#include "http.hpp"

namespace dispatcher {

struct Outcome {
    enum Type {
        RESPONSE_NOW,
        CGI_REQUIRED,
    };

    Outcome();

    Type type;
    std::string response;
    const Config *config;
    std::string filesystem_path;
};

const Config &config_for(const http::request &req, const Server &server);
std::string filesystem_path(const http::request &req, const Config &cfg);

Outcome handle(const http::request &req, const Server &server);

std::string error_response(http::status::type status);
std::string error_response(
    const http::request &req, const Config &cfg, http::status::type status);

}
