/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Ssl.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade </var/spool/mail/uanglade>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 06:30:19 by uanglade          #+#    #+#             */
/*   Updated: 2026/06/04 17:06:55 by uanglade         ###   ########.fr       */
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

Ssl::Ssl(int32_t fd)
    : _fd(fd)
    , _client_hello_received(false)
{
}

Ssl::SslError Ssl::connect(const SslContext *ssl_ctx)
{
    _ctx = ssl_ctx;
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
        if (!_client_hello_received && _buf.size() > 5) {
            _client_hello.content_type = _buf[0];
            _client_hello.protocol_version = _buf[1] << 8 | _buf[2];
            _client_hello.content_length = _buf[3] << 8 | _buf[4];
            L_TRACE(
                "Content type: {}, Protocol version: {}, Content length: {}",
                (int)_client_hello.content_type, _client_hello.protocol_version,
                _client_hello.content_length);
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
