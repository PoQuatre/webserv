/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/17 19:52:07 by mle-flem          #+#    #+#             */
/*   Updated: 2026/06/03 05:46:47 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include "logger.hpp"

#define RECV_CHUNK 4096

Connection::Connection()
    : _fd(-1)
    , _server(NULL)
    , _send_state(IDLE)
    , _ssl(NULL)
    , _is_ssl(false)
    , _is_handshake_done(false)
{
}

Connection::Connection(int32_t fd, const Server &server)
    : _fd(fd)
    , _server(&server)
    , _send_state(IDLE)
    , _ssl(NULL)
    , _is_ssl(false)
    , _is_handshake_done(false)
{
}

namespace {
bool is_request_ssl(int fd)
{

    char tmp[4];
    ssize_t n = recv(fd, tmp, 4, MSG_PEEK);
    if (n != 4)
        return false;
    if (tmp[0] == 0x16 && tmp[1] == 0x03 && tmp[2] == 0x01)
        return true;
    return false;
}
}

bool Connection::on_readable()
{
    if (!_is_ssl && is_request_ssl(_fd)) {
        _ssl = SSL_new(_server->get_ssl_ctx());
        SSL_set_fd(_ssl, _fd);
        SSL_set_accept_state(_ssl);
        _is_ssl = true;
        _is_handshake_done = false;
    }

    bool was_complete = _parser.is_complete();
    bool ok = do_recv();
    if (!ok && _parser.is_error())
        return false;

    if (!was_complete && _parser.is_complete())
        L_DEBUG("Client {} requested: {}", _fd, _parser.request());

    return !_parser.is_eof() && !_parser.is_error();
}

bool Connection::on_writable() { return do_send(); }

void Connection::enqueue_response(const std::string &data)
{
    _send_buf += data;
    _send_state = SENDING;
}

void Connection::reset()
{
    _send_buf.clear();
    _send_state = IDLE;
    _parser.reset();
}

bool Connection::do_recv()
{
    if (_is_ssl && _ssl && !_is_handshake_done && !SSL_is_init_finished(_ssl)) {
        L_TRACE("Trying to do handsake ");
        int ret = SSL_accept(_ssl);
        if (ret == 1) {
            _is_handshake_done = true;
            L_TRACE("Handsake done");
        } else if (ret == 0) {
            int err = SSL_get_error(_ssl, ret);
            if (err == SSL_ERROR_WANT_READ) {
                L_TRACE("Ssl want read");
            }
            if (err == SSL_ERROR_WANT_WRITE) {
                L_TRACE("Ssl want write");
            }
        } else if (ret < 0) {
            L_ERROR("fatal ssl handshake error");
        }
    }
    char tmp[RECV_CHUNK];
    ssize_t n = -1;

    if (_is_ssl) {
        if (_is_handshake_done) {
            L_TRACE("Reading with SSL");
            n = SSL_read(_ssl, tmp, RECV_CHUNK);
        }
    } else {
        n = recv(_fd, tmp, sizeof(tmp), 0);
    }
    if (n > 0) {
        L_TRACE("Received {} bytes from client {}", n, _fd);
        return _parser.feed(tmp, static_cast<std::size_t>(n));
    }
    if (n == 0) {
        L_TRACE("Client {} closed cleanly", _fd);
        _parser.set_eof();
        return false;
    }

    // n == -1: could be EAGAIN (no data) or a real error.
    // Since epoll told us the fd was readable, treat -1 as "nothing available
    // right now" and let epoll re-notify
    return true;
}

bool Connection::do_send()
{
    if (_send_buf.empty()) {
        _send_state = IDLE;
        return true;
    }
    ssize_t n = -1;
    if (_is_ssl) {
        n = SSL_write(
            _ssl, _send_buf.data(), static_cast<int>(_send_buf.size()));
    } else {
        n = send(_fd, _send_buf.c_str(), _send_buf.size(), 0);
    }

    if (n > 0) {
        L_TRACE("Sent {} bytes to client {}", n, _fd);
        _send_buf.erase(0, static_cast<std::size_t>(n));
    }
    // n == 0: shouldn't happen but is fine
    // n == -1: kernel buffer full, or EAGAIN - epoll will re-fire EPOLLOUT
    return true; // never an error, we rely on EPOLLERR/EPOLLHUP for that
}
