/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpParser.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 20:54:11 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/22 07:28:51 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stddef.h>

#include <string>

#include "http.hpp"

class HttpParser {
public:
    enum State {
        READING_REQUEST_LINE,
        READING_HEADERS,
        READING_BODY,
        COMPLETE,
        ERROR,
    };

    HttpParser();

    bool feed(const char *data, std::size_t len);
    void set_eof();

    bool is_complete() const { return _state == COMPLETE; }
    bool is_error() const { return _state == ERROR; }
    bool is_eof() const { return _eof; }

    const http::request &request() const { return _request; }

    void reset();

private:
    std::string _buf;
    State _state;
    std::size_t _content_length;
    bool _eof;
    http::request _request;
    bool _chunked;

    bool run();

    bool try_parse_request_line();
    bool try_parse_method(std::size_t start, std::size_t end);
    bool try_parse_version(std::size_t start, std::size_t end);

    bool try_parse_headers();

    bool try_parse_body();
    bool try_parse_chunk();
};
