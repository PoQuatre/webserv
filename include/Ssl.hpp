/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Ssl.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade </var/spool/mail/uanglade>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 06:12:01 by uanglade          #+#    #+#             */
/*   Updated: 2026/06/04 22:53:39 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <cstddef>
#include <string>
#include <vector>

#include "ssl_types.hpp"

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

// this code follows the RFC 8446

namespace ssl {

class SslContext {
public:
    SslContext() { };

private:
};

class Ssl {
public:
    enum SslState {
        STATE_NONE = 0,
        STATE_HANDSHAKE,
        STATE_READY,
    };

    enum SslError {
        ERROR_OK,
        ERROR_NEED_WRITE,
        ERROR_NEED_READ,
        ERROR_FATAL,
    };

    // enum HandshakeState {
    //     HANDSHAKE_DONE,
    //     HANDSHAKE_NEED_CLIENT_HELLO,
    // };

    Ssl(int32_t fd);
    SslError connect(const SslContext *ssl_ctx);
    SslError accept();
    SslError disconnect();
    int32_t read(void *buf, int32_t size);
    int32_t write(const void *buf, int32_t num);

private:
    std::vector<uint8_t> _buf;
    int32_t _fd;
    const SslContext *_ctx;
    SslState _state;

    record_protocol::TLSPlainText _current_message;
    bool _client_hello_received;
    handshake_protocol::ClientHello _client_hello;
};
}
