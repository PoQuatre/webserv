/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Ssl.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade </var/spool/mail/uanglade>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 06:12:01 by uanglade          #+#    #+#             */
/*   Updated: 2026/06/04 13:20:32 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <cstddef>
#include <string>
#include <vector>

#include "cli.hpp"
#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

// this code follows the RFC 8446

namespace ssl {

namespace versions {

#define TLS_VERSIONS                                                           \
    X(TLS10, "TLS/1.0", 3, 1)                                                  \
    X(TLS11, "TLS/1.1", 3, 2)                                                  \
    X(TLS12, "TLS/1.2", 3, 3)                                                  \
    X(TLS13, "TLS/1.3", 3, 4)

enum type {
#define X(name, _, __, ___) name,
    TLS_VERSIONS
#undef X
};

UNUSED
static const char *strings[] = {
#define X(_, str, __, ___) str,
    TLS_VERSIONS
#undef X
};

union type_int {
    uint16_t raw;
    struct {
        uint8_t major;
        uint8_t minor;
    };
};

UNUSED
static const type_int versions[] = {
#define X(_, __, x, y) { .major = (x), .minor = (y) },
    TLS_VERSIONS
#undef X
};

UNUSED
static const std::size_t COUNT = sizeof(strings) / sizeof(*strings);

}

// Base of all tls mesages fragments the data and specifies what the data is

namespace record_protocol {

enum ContentType {
    CONTENT_TYPE_invalid = 0,
    CONTENT_TYPE_change_cipher_spec = 20,
    CONTENT_TYPE_alert = 21,
    CONTENT_TYPE_handshake = 22,
    CONTENT_TYPE_application_data = 23,
};

struct TLSPlainText {
    ContentType type;
    versions::type_int legacy_record_version;
};

}

namespace handshake_protocol {

struct ClientHello {
    uint8_t content_type;
    uint8_t protocol_version_major;
    uint8_t protocol_version_minor;
    uint16_t content_length;
};
}

class SslContext {
public:
    SslContext() {};

private:
};

class Ssl {
public:
    enum SslState {
        STATE_HANDSHAKE = 0,
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
    bool _client_hello_received;
    handshake_protocol::ClientHello _client_hello;
};
}
