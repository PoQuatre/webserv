/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   http.hpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 04:03:45 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/18 07:34:02 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <cstddef>
#include <map>
#include <string>

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

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

UNUSED
static const char *strings[] = {
#define X(name) #name,
    HTTP_METHODS
#undef X
};

UNUSED
static const std::size_t COUNT = sizeof(strings) / sizeof(*strings);

}

namespace versions {

#define HTTP_VERSIONS                                                          \
    X(HTTP09, "HTTP/0.9")                                                      \
    X(HTTP10, "HTTP/1.0")                                                      \
    X(HTTP11, "HTTP/1.1")

enum type {
#define X(name, _) name,
    HTTP_VERSIONS
#undef X
};

UNUSED
static const char *strings[] = {
#define X(_, str) str,
    HTTP_VERSIONS
#undef X
};

UNUSED
static const std::size_t COUNT = sizeof(strings) / sizeof(*strings);

}

namespace status {

#define STATUS_CODES                                                           \
    X(OK, 200, "OK")                                                           \
    X(CREATED, 201, "Created")                                                 \
    X(ACCEPTED, 202, "Accepted")                                               \
    X(NO_CONTENT, 204, "No Content")                                           \
                                                                               \
    X(MULTIPLE_CHOICES, 300, "Multiple Choices")                               \
    X(MOVED_PERMANENTLY, 301, "Moved Permanently")                             \
    X(MOVED_TEMPORARILY, 302, "Moved Temporarily")                             \
    X(NOT_MODIFIED, 304, "Not Modified")                                       \
                                                                               \
    X(BAD_REQUEST, 400, "Bad Request")                                         \
    X(FORBIDDEN, 403, "Forbidden")                                             \
    X(NOT_FOUND, 404, "Not Found")                                             \
                                                                               \
    X(INTERNAL_SERVER_ERROR, 500, "Internal Server Error")                     \
    X(NOT_IMPLEMENTED, 501, "Not Implemented")                                 \
    X(BAD_GATEWAY, 502, "Bad Gateway")                                         \
    X(SERVICE_UNAVAILABLE, 503, "Service Unavailable")

enum type {
#define X(name, _, __) name,
    STATUS_CODES
#undef X
};

UNUSED
static const char *strings[] = {
#define X(name, _, __) #name,
    STATUS_CODES
#undef X
};

UNUSED
static const int32_t codes[] = {
#define X(_, code, __) code,
    STATUS_CODES
#undef X
};

UNUSED
static const char *reasons[] = {
#define X(_, __, reason) reason,
    STATUS_CODES
#undef X
};

UNUSED
static const std::size_t COUNT = sizeof(strings) / sizeof(*strings);

}

struct request {
    methods::type method;
    std::string uri;
    versions::type version;
    std::map<std::string, std::string> headers;
    std::string body;
};

}

#undef HTTP_METHODS
#undef HTTP_VERSIONS
#undef STATUS_CODES
