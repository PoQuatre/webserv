/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ssl_types.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade <uanglade@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 22:51:59 by uanglade          #+#    #+#             */
/*   Updated: 2026/06/05 00:27:38 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <cstddef>
#include <ostream>

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
    void *data;
};

std::ostream &operator<<(std::ostream &os, const TLSPlainText &msg);

}

namespace handshake_protocol {

namespace extensions {

namespace ExtensionType {

#define EXTENSION_TYPES                                                        \
    X(EXTENSION_TYPE_server_name, 0)                                           \
    X(EXTENSION_TYPE_max_fragment_length, 1)                                   \
    X(EXTENSION_TYPE_status_request, 5)                                        \
    X(EXTENSION_TYPE_supported_groups, 10)                                     \
    X(EXTENSION_TYPE_signature_algorithms, 13)                                 \
    X(EXTENSION_TYPE_use_srtp, 14)                                             \
    X(EXTENSION_TYPE_heartbeat, 15)                                            \
    X(EXTENSION_TYPE_application_layer_protocol_negotiation, 16)               \
    X(EXTENSION_TYPE_signed_certificate_timestamp, 18)                         \
    X(EXTENSION_TYPE_client_certificate_type, 19)                              \
    X(EXTENSION_TYPE_server_certificate_type, 20)                              \
    X(EXTENSION_TYPE_padding, 21)                                              \
    X(EXTENSION_TYPE_pre_shared_key, 41)                                       \
    X(EXTENSION_TYPE_early_data, 42)                                           \
    X(EXTENSION_TYPE_supported_versions, 43)                                   \
    X(EXTENSION_TYPE_cookie, 44)                                               \
    X(EXTENSION_TYPE_psk_key_exchange_modes, 45)                               \
    X(EXTENSION_TYPE_certificate_authorities, 47)                              \
    X(EXTENSION_TYPE_oid_filters, 48)                                          \
    X(EXTENSION_TYPE_post_handshake_auth, 49)                                  \
    X(EXTENSION_TYPE_signature_algorithms_cert, 50)                            \
    X(EXTENSION_TYPE_key_share, 51)

enum type {
#define X(name, value) name = (value),
    EXTENSION_TYPES
#undef X
};

inline const char *to_string(type cs)
{
    switch (cs) {
#define X(name, _)                                                             \
    case name:                                                                 \
        return #name;
        EXTENSION_TYPES
#undef X
    }
    return "UNKNOWN";
}

}

struct Extenion {
    ExtensionType::type type;
    void *extenion_data;
};

std::ostream &operator<<(std::ostream &os, const Extenion &ext);

// TODO: more extensions

}

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

inline const char *to_string(type cs)
{
    switch (cs) {
#define X(name, _)                                                             \
    case name:                                                                 \
        return #name;
        HANDSHAKE_TYPES
#undef X
    }
    return "UNKNOWN";
}

#undef HANDSHAKE_TYPES

}

struct ClientHello {
    HandshakeType::type type;
    uint8_t length[3];
    versions::type_int version;
    uint8_t random[32];
    uint8_t session_id_length;
    const uint8_t *session_id;
    uint16_t cipher_suites_length;
    const cipher_suites::type *cipher_suites;
    uint8_t legacy_compression_methods_length;
    const uint8_t *legacy_compression_methods;
    uint16_t extensions_length;
    const extensions::Extenion *extensions;
};

inline std::ostream &operator<<(std::ostream &os, const ClientHello &msg)
{
    os << "type: "
       << ssl::handshake_protocol::HandshakeType::to_string(msg.type)
       << ", length: " << msg.length
       << ", version: " << ssl::versions::strings[msg.version.minor - 1]
       << "\n";

    os << "random: ";
    for (int i = 0; i < 32; i++) {
        os << std::hex << (int)msg.random[i];
    }
    os << "\n"
       << "session id length: " << std::dec << (int)msg.session_id_length
       << ", session id: ";
    for (int i = 0; i < msg.session_id_length; i++) {
        os << std::hex << (int)msg.session_id[i];
    }
    os << "\ncipher suites length: " << std::dec
       << (int)msg.cipher_suites_length << " cipher suites: \n";
    // FIXME: c casse connerie de cast

    // for (int i = 0; i < msg.cipher_suites_length / 2; i++) {
    // os << std::hex << (int)msg.cipher_suites[0]
    // << "\n"; // ssl::cipher_suites::to_string(msg.cipher_suites[i])
    // << "\n";
    // }
    os << "extensions count: " << std::dec << (int)msg.extensions_length;

    return os;
}

}
}
