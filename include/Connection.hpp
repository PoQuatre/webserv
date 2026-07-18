/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/17 07:17:09 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/18 06:59:22 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

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
        WAITING_CGI,
    };

    Connection();
    Connection(int32_t fd, const Server &server);

    bool on_readable();
    bool on_writable();

    void enqueue_response(const std::string &data);
    void wait_for_cgi();

    bool is_parse_complete() const { return _parser.is_complete(); }
    bool is_parse_error() const { return _parser.is_error(); }
    http::status::type parse_error_code() const { return _parser.error_code(); }
    bool is_sending() const { return _send_state == SENDING; }
    bool is_waiting_cgi() const { return _send_state == WAITING_CGI; }
    bool keep_alive() const { return _parser.request().keep_alive; }

    const http::request &request() const { return _parser.request(); }
    const Server &server() const { return *_server; }
    int32_t fd() const { return _fd; }

    void reset();

private:
    int32_t _fd;
    const Server *_server;
    std::string _send_buf;
    SendState _send_state;
    HttpParser _parser;

    bool do_recv();
    bool do_send();
};
