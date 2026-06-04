/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Ssl.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade </var/spool/mail/uanglade>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 06:30:19 by uanglade          #+#    #+#             */
/*   Updated: 2026/06/05 00:27:36 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Ssl.hpp"

#include <sys/socket.h>
#include <sys/types.h>

#include <cstring>

#include "logger.hpp"

#ifndef RECV_CHUNK
#define RECV_CHUNK 4096
#endif

namespace ssl {

namespace {

handshake_protocol::ClientHello convert_to_client_hello(const uint8_t *buf)
{
    handshake_protocol::ClientHello ret;

    ret.type = static_cast<handshake_protocol::HandshakeType::type>(*buf++);
    ret.length[0] = *buf++;
    ret.length[1] = *buf++;
    ret.length[2] = *buf++;
    ret.version.major = *buf++;
    ret.version.minor = *buf++;
    for (int i = 0; i < 32; ++i) {
        ret.random[i] = *buf++;
    }
    ret.session_id_length = *buf++;
    ret.session_id = buf;
    buf += ret.session_id_length;
    ret.cipher_suites_length = buf[0] << 8 | buf[1];
    buf += 2;
    // FIXME: c casse connerie de cast
    ret.cipher_suites = (cipher_suites::type *)buf;
    buf += ret.cipher_suites_length;
    ret.legacy_compression_methods_length = *buf++;
    ret.legacy_compression_methods = buf;
    buf += ret.legacy_compression_methods_length;
    ret.extensions_length = buf[0] << 8 | buf[1];
    ret.extensions = (handshake_protocol::extensions::Extenion *)buf;

    return ret;
}

}

Ssl::Ssl(int32_t fd)
    : _fd(fd)
    , _state(STATE_NONE)
    , _client_hello_received(false)
{
}

Ssl::SslError Ssl::connect(const SslContext *ssl_ctx)
{
    _ctx = ssl_ctx;
    _state = STATE_HANDSHAKE;
    return ERROR_OK;
}

Ssl::SslError Ssl::accept()
{

    uint8_t tmp[RECV_CHUNK];
    ssize_t n = -1;

    n = recv(_fd, tmp, sizeof(tmp), 0);
    if (n > 0) {
        _buf.resize(_buf.size() + n);
        uint8_t *end = _buf.end().base() - n;
        std::memcpy(end, tmp, n);
        L_TRACE("Received {} bytes from client {}", n, _fd);
        if (_buf.size() > 5) {
            _current_message.type
                = static_cast<record_protocol::ContentType::type>(_buf[0]);
            _current_message.legacy_record_version.major = _buf[1];
            _current_message.legacy_record_version.minor = _buf[2];
            _current_message.content_length = _buf[3] << 8 | _buf[4];
            L_TRACE("Content type: {}, Protocol version: major: {} minor: {}, "
                    "Content length: {}",
                record_protocol::ContentType::strings[_current_message.type == 0
                        ? 0
                        : _current_message.type - 19],
                (int)_current_message.legacy_record_version.major,
                (int)_current_message.legacy_record_version.minor,
                _current_message.content_length);
        }
        if (_buf.size() >= _current_message.content_length) {
            _current_message.data = &_buf[5];
            if (_current_message.type
                == record_protocol::ContentType::CONTENT_TYPE_handshake) {
                handshake_protocol::ClientHello client_hello
                    = convert_to_client_hello(&_buf[5]);
                L_TRACE("client hello : {}", client_hello);
                (void)client_hello;
            }
        }
    }
    if (n == 0) {
        L_TRACE("Ssl client closed cleanly");
        for (uint64_t i = 0; i < _buf.size(); i += 10) {
            for (uint64_t j = i; j < i + 10 && j < _buf.size(); j++) {
                std::cout << "0x" << std::hex << (int)_buf[j] << "|";
            }
            std::cout << "\n";
        }
        // L_TRACE("Final buf: {}", _buf);
        return ERROR_FATAL;
    }

    return ERROR_NEED_READ;
}

Ssl::SslError Ssl::disconnect() { return ERROR_OK; }

int32_t Ssl::read(void *buf, int size)
{
    (void)buf;
    (void)size;
    return 0;
}
int32_t Ssl::write(const void *buf, int num)
{
    (void)buf;
    (void)num;
    return 1;
}

}
