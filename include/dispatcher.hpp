/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dispatcher.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/29 00:00:00 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/27 19:28:13 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>

#include "Server.hpp"
#include "http.hpp"
#include "logger.hpp"

// FIXME: remove that
#define make_error_response_impl(req, st, cfg)                                 \
    (L_DEBUG("Responded with error code {}", http::status::codes[st]),         \
        make_error_response_impl_impl(req, st, cfg))

namespace dispatcher {

const Config &config_for(const http::request &req, const Server &server);

std::string handle(const http::request &req, const Server &server);

std::string error_response(http::status::type status);
std::string error_response(
    const http::request &req, const Config &cfg, http::status::type status);

}
