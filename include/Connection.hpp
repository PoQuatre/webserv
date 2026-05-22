/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/17 07:17:09 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/21 00:00:00 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <string>

#include "HttpParser.hpp"
#include "http.hpp"

class Connection {
public:
    enum SendState {
        IDLE,
        SENDING,
    };

    Connection();
    explicit Connection(int32_t fd);

    bool on_readable();
    bool on_writable();

    void enqueue_response(const std::string &data);

    bool is_parse_complete() const { return _parser.is_complete(); }
    bool is_parse_error() const { return _parser.is_error(); }
    bool is_sending() const { return _send_state == SENDING; }

    const http::request &request() const { return _parser.request(); }
    int32_t fd() const { return _fd; }

    void reset();

private:
    int32_t _fd;
    std::string _send_buf;
    SendState _send_state;
    HttpParser _parser;

    bool do_recv();
    bool do_send();
};
