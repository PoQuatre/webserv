/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/17 07:17:09 by mle-flem          #+#    #+#             */
/*   Updated: 2026/06/03 05:23:36 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <openssl/ssl.h>
#include <stdint.h>

#include <string>

#include "HttpParser.hpp"
#include "Server.hpp"
#include "http.hpp"

class Connection {
public:
    enum SendState {
        IDLE,
        SENDING,
    };

    Connection();
    Connection(int32_t fd, const Server &server);

    bool on_readable();
    bool on_writable();

    void enqueue_response(const std::string &data);

    bool is_parse_complete() const { return _parser.is_complete(); }
    bool is_parse_error() const { return _parser.is_error(); }
    http::status::type parse_error_code() const { return _parser.error_code(); }
    bool is_sending() const { return _send_state == SENDING; }
    bool keep_alive() const { return _parser.request().keep_alive; }

    const http::request &request() const { return _parser.request(); }
    const Server &server() const { return *_server; }
    int32_t fd() const { return _fd; }
    SSL *ssl() const { return _ssl; }

    void reset();

private:
    int32_t _fd;
    const Server *_server;
    std::string _send_buf;
    SendState _send_state;
    HttpParser _parser;
    SSL *_ssl;
    bool _is_ssl;
    bool _is_handshake_done;

    bool do_recv();
    bool do_send();
};
