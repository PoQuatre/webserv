/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/17 19:52:07 by mle-flem          #+#    #+#             */
/*   Updated: 2026/08/13 22:56:03 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "logger.hpp"

#define RECV_CHUNK 4096
#define FILE_CHUNK 16384

namespace {

std::string peer_address(int32_t fd)
{
    sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    char buffer[NI_MAXHOST];

    if (getpeername(fd, reinterpret_cast<sockaddr *>(&addr), &len) != 0)
        return "";
    if (getnameinfo(reinterpret_cast<sockaddr *>(&addr), len, buffer,
            sizeof(buffer), NULL, 0, NI_NUMERICHOST)
        != 0)
        return "";
    return buffer;
}

}

Connection::Connection()
    : _fd(-1)
    , _default_server(NULL)
    , _server(NULL)
    , _file_fd(-1)
    , _send_state(IDLE)
{
}

Connection::Connection(int32_t fd, const Server &server)
    : _fd(fd)
    , _default_server(&server)
    , _server(&server)
    , _file_fd(-1)
    , _send_state(IDLE)
{
    _parser.set_remote_addr(peer_address(fd));
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

void Connection::enqueue_file(int32_t fd)
{
    close_file();
    _file_fd = fd;
    _send_state = SENDING;
}

void Connection::wait_for_cgi() { _send_state = WAITING_CGI; }

void Connection::reset()
{
    close_file();
    _send_buf.clear();
    _send_state = IDLE;
    _server = _default_server;
    _parser.reset();
}

void Connection::close_file()
{
    if (_file_fd != -1) {
        close(_file_fd);
        _file_fd = -1;
    }
}

bool Connection::fill_file_buffer()
{
    char tmp[FILE_CHUNK];
    ssize_t n;

    if (!_send_buf.empty() || _file_fd == -1)
        return true;
    n = read(_file_fd, tmp, sizeof(tmp));
    if (n > 0) {
        _send_buf.assign(tmp, static_cast<std::size_t>(n));
        return true;
    }
    if (n == 0) {
        close_file();
        _send_state = IDLE;
        return true;
    }
    L_WARN(
        "Failed to read static file for client {}: {}", _fd, strerror(errno));
    close_file();
    _send_state = IDLE;
    return false;
}

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
    if (!fill_file_buffer())
        return false;
    if (_send_buf.empty()) {
        _send_state = IDLE;
        return true;
    }

    ssize_t n = send(_fd, _send_buf.c_str(), _send_buf.size(), 0);
    if (n > 0) {
        L_TRACE("Sent {} bytes to client {}", n, _fd);
        _send_buf.erase(0, static_cast<std::size_t>(n));
    }

    // n == 0: shouldn't happen but is fine
    // n == -1: kernel buffer full, or EAGAIN - epoll will re-fire EPOLLOUT
    return true; // never an error, we rely on EPOLLERR/EPOLLHUP for that
}
