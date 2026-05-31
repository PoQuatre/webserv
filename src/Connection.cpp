/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/17 19:52:07 by mle-flem          #+#    #+#             */
/*   Updated: 2026/06/01 06:58:47 by mle-flem         ###   ########.fr       */
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
{
}

Connection::Connection(int32_t fd, const Server &server)
    : _fd(fd)
    , _server(&server)
    , _send_state(IDLE)
{
}

bool Connection::on_readable()
{
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

void Connection::reset() { _parser.reset(); }

bool Connection::do_recv()
{
    char tmp[RECV_CHUNK];
    ssize_t n = recv(_fd, tmp, sizeof(tmp), 0);

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

    ssize_t n = send(_fd, _send_buf.c_str(), _send_buf.size(), 0);
    if (n > 0) {
        L_TRACE("Sent {} bytes to client {}", n, _fd);
        _send_buf.erase(0, static_cast<std::size_t>(n));
        if (_send_buf.empty())
            _send_state = IDLE;
    }

    // n == 0: shouldn't happen but is fine
    // n == -1: kernel buffer full, or EAGAIN - epoll will re-fire EPOLLOUT
    return true; // never an error, we rely on EPOLLERR/EPOLLHUP for that
}
