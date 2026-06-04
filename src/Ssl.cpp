/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Ssl.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade </var/spool/mail/uanglade>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 06:30:19 by uanglade          #+#    #+#             */
/*   Updated: 2026/06/04 22:42:32 by uanglade         ###   ########.fr       */
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

handshake_protocol::ClientHello convert_to_client_hello(
    const std::vector<uint8_t> &buf)
{
    handshake_protocol::ClientHello ret;

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
            if (_current_message.type
                == record_protocol::ContentType::CONTENT_TYPE_handshake) { }
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
