/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/17 07:17:09 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/18 05:01:16 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <string>

#include "http.hpp"

class Connection {
public:
    enum ParseState {
        READING_REQUEST_LINE,
        READING_HEADERS,
        READING_BODY,
        PARSE_COMPLETE,
        PARSE_ERROR,
    };

    enum SendState {
        IDLE,
        SENDING,
    };

    Connection();
    explicit Connection(int32_t fd);

    bool on_readable();
    bool on_writable();

    void enqueue_response(const std::string &data);

    bool is_parse_complete() const { return _parse_state == PARSE_COMPLETE; }
    bool is_parse_error() const { return _parse_state == PARSE_ERROR; }
    bool is_sending() const { return _send_state == SENDING; }

    const http::request &request() const { return _request; }
    int32_t fd() const { return _fd; }

    void reset();

private:
    int32_t _fd;
    std::string _recv_buf;
    std::string _send_buf;

    ParseState _parse_state;
    SendState _send_state;

    std::size_t _content_length;
    std::size_t _header_end;
    bool _eof;

    http::request _request;

    bool try_parse_request_line();
    bool try_parse_method(std::size_t start, std::size_t end);
    bool try_parse_version(std::size_t start, std::size_t end);

    bool try_parse_headers();

    bool try_parse_body();

    bool do_recv();
    bool do_send();
};
