/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ssl_types.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade <uanglade@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 22:51:59 by uanglade          #+#    #+#             */
/*   Updated: 2026/06/04 22:54:11 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <cstddef>

#include "cipher_suites.hpp"

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

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

#undef TLS_VERSIONS

}

// Base of all tls mesages fragments the data and specifies what the data is

namespace record_protocol {

namespace ContentType {

#define CONTENT_TYPES                                                          \
    X(CONTENT_TYPE_invalid, 0)                                                 \
    X(CONTENT_TYPE_change_cipher_spec, 20)                                     \
    X(CONTENT_TYPE_alert, 21)                                                  \
    X(CONTENT_TYPE_handshake, 22)                                              \
    X(CONTENT_TYPE_application_data, 23)

enum type {
#define X(name, value) name = (value),
    CONTENT_TYPES
#undef X
};

UNUSED
static const char *strings[] = {
#define X(str, _) #str,
    CONTENT_TYPES
#undef X

};

UNUSED
static const std::size_t COUNT = sizeof(strings) / sizeof(*strings);

#undef CONTENT_TYPES
}

struct TLSPlainText {
    ContentType::type type;
    uint16_t content_length;
    versions::type_int legacy_record_version;
};

}

namespace handshake_protocol {

namespace HandshakeType {

#define HANDSHAKE_TYPES                                                        \
    X(HANDSHAKE_TYPE_client_hello, 1)                                          \
    X(HANDSHAKE_TYPE_server_hello, 2)                                          \
    X(HANDSHAKE_TYPE_new_session_ticket, 4)                                    \
    X(HANDSHAKE_TYPE_end_of_early_data, 5)                                     \
    X(HANDSHAKE_TYPE_encrypted_extensions, 8)                                  \
    X(HANDSHAKE_TYPE_certificate, 11)                                          \
    X(HANDSHAKE_TYPE_certificate_request, 13)                                  \
    X(HANDSHAKE_TYPE_certificate_verify, 15)                                   \
    X(HANDSHAKE_TYPE_finished, 20)                                             \
    X(HANDSHAKE_TYPE_key_update, 24)                                           \
    X(HANDSHAKE_TYPE_message_hash, 254)

enum type {
#define X(name, value) name = (value),
    HANDSHAKE_TYPES
#undef X
};

UNUSED
static const char *strings[] = {
#define X(str, _) #str,
    HANDSHAKE_TYPES
#undef X

};

UNUSED
static const std::size_t COUNT = sizeof(strings) / sizeof(*strings);

#undef HANDSHAKE_TYPES
}

struct ClientHello {
    HandshakeType::type type;
    uint8_t length[3];
    versions::type_int version;
    uint8_t random[32];
    uint8_t session_id_length;
    uint8_t *session_id;
    uint16_t cipher_suites_length;
    cipher_suites::type *cipher_suites;
};

}

}
