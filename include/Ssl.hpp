/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Ssl.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade </var/spool/mail/uanglade>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 06:12:01 by uanglade          #+#    #+#             */
/*   Updated: 2026/06/10 22:27:24 by uanglade         ###   ########.fr       */
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
    SslContext() {};

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
    SslState get_state() const { return _state; }

private:
    SslError check_client_hello(const handshake_protocol::ClientHello &msg);
    void generate_keys();
    void generate_server_hello();
    void derive_secret(const uint8_t *secret, const char *label,
        const std::vector<uint8_t> &transcript, uint8_t out[32]);

    std::vector<uint8_t> _read_buf;
    std::vector<uint8_t> _send_buf;
    int32_t _fd;
    const SslContext *_ctx;
    SslState _state;
    record_protocol::TLSPlainText _current_message;
    bool _client_hello_received;
    handshake_protocol::ClientHello _client_hello;
    cipher_suites::type _chosen_cipher;
    handshake_protocol::extensions::NamedGroup::type _chosen_group;
    handshake_protocol::extensions::KeyShareEntry _chose_key_share;
    handshake_protocol::extensions::SignatureScheme::type
        _chosen_signature_algo;
    uint8_t _server_private_key[32];
    uint8_t _server_public_key[32];
    uint8_t _client_public_key[32];
    uint8_t _shared_key[32];
    std::vector<record_protocol::TLSPlainText> _handshake_records;
    std::vector<std::vector<uint8_t> > _buf_records;
};
}
