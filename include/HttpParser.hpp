/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpParser.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 20:54:11 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/27 21:22:17 by mle-flem         ###   ########.fr       */
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

    http::status::type error_code() const { return _error_code; }

    const http::request &request() const { return _request; }

    void reset();

private:
    std::string _buf;
    State _state;
    std::size_t _content_length;
    bool _eof;
    http::request _request;
    bool _chunked;
    http::status::type _error_code;

    bool run();
    void set_err(http::status::type code);

    bool try_parse_request_line();
    bool try_parse_method(std::size_t start, std::size_t end);
    bool try_parse_uri(std::size_t start, std::size_t end);
    bool try_parse_query(std::size_t start, std::size_t end);
    bool try_parse_version(std::size_t start, std::size_t end);

    bool try_parse_headers();
    bool try_parse_header_field(std::size_t &pos, std::size_t end_pos);

    bool try_parse_body();
    bool try_parse_chunk();
};
