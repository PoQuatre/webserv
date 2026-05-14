/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   http.hpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 04:03:45 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/14 20:17:46 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <cstddef>

namespace http {

namespace methods {

#define HTTP_METHODS                                                           \
    X(GET)                                                                     \
    X(POST)                                                                    \
    X(DELETE)                                                                  \
    X(PUT)                                                                     \
    X(PATCH)                                                                   \
    X(HEAD)                                                                    \
    X(OPTIONS)                                                                 \
    X(CONNECT)                                                                 \
    X(TRACE)

enum type {
#define X(name) name,
    HTTP_METHODS
#undef X
};

static const char *strings[] = {
#define X(name) #name,
    HTTP_METHODS
#undef X
};

static const std::size_t COUNT = sizeof(strings) / sizeof(*strings);

}

}

#undef HTTP_METHODS
