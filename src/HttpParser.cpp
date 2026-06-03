/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpParser.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 20:54:18 by mle-flem          #+#    #+#             */
/*   Updated: 2026/06/03 21:26:52 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpParser.hpp"

#include <cstdlib>
#include <cstring>

#include "logger.hpp"

#define MAX_BUF_SIZE 8388608 // 8mb

HttpParser::HttpParser()
    : _state(READING_REQUEST_LINE)
    , _content_length(0)
    , _eof(false)
    , _request()
    , _chunked(false)
    , _error_code(http::status::OK)
{
}

bool HttpParser::feed(const char *data, std::size_t len)
{
    if (_buf.size() >= MAX_BUF_SIZE) {
        L_WARN("Request exceeded max size");
        set_err(http::status::BAD_REQUEST);
        return false;
    }
    _buf.append(data, len);
    return run();
}

void HttpParser::set_eof()
{
    _eof = true;
    if (!_buf.empty())
        run();
}

void HttpParser::reset()
{
    _state = READING_REQUEST_LINE;
    _content_length = 0;
    _eof = false;
    _chunked = false;
    _error_code = http::status::OK;
    _request = http::request();
    run();
}

bool HttpParser::run()
{
    bool progressed = true;
    while (progressed && _state != COMPLETE && _state != ERROR) {
        progressed = false;
        switch (_state) {
        default:
            break;
        }
    }

    if (_state == ERROR)
        _request.keep_alive = false;
    return _state != ERROR;
}

void HttpParser::set_err(http::status::type code)
{
    _state = ERROR;
    _error_code = code;
}
