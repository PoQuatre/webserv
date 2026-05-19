/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/17 19:52:07 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/19 06:29:28 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>

#include "logger.hpp"

#define RECV_CHUNK 4096
#define MAX_BUF_SIZE 8388608 // 8mb

Connection::Connection()
    : _fd(-1)
    , _parse_state(READING_REQUEST_LINE)
    , _send_state(IDLE)
    , _content_length(0)
    , _header_end(0)
    , _request()
{
}

Connection::Connection(int32_t fd)
    : _fd(fd)
    , _parse_state(READING_REQUEST_LINE)
    , _send_state(IDLE)
    , _content_length(0)
    , _header_end(0)
    , _request()
{
}

bool Connection::on_readable()
{
    if (!do_recv())
        return false; // peer closed or unrecoverable

    bool progressed = true;
    while (progressed && _parse_state != PARSE_COMPLETE
        && _parse_state != PARSE_ERROR) {
        progressed = false;
        switch (_parse_state) {
        case READING_REQUEST_LINE:
            progressed = try_parse_request_line();
            break;
        case READING_HEADERS:
            progressed = try_parse_headers();
            break;
        case READING_BODY:
            progressed = try_parse_body();
            break;
        default:
            break;
        }
    }

    if (progressed && _parse_state == PARSE_COMPLETE)
        L_DEBUG("Client {} requested: {}", _fd, _request);

    return _parse_state != PARSE_ERROR;
}

bool Connection::on_writable() { return do_send(); }

void Connection::enqueue_response(const std::string &data)
{
    _send_buf += data;
    _send_state = SENDING;
}

void Connection::reset()
{
    _recv_buf.clear();
    _send_buf.clear();
    _parse_state = READING_REQUEST_LINE;
    _send_state = IDLE;
    _content_length = 0;
    _header_end = 0;
    _request = http::request();
}

bool Connection::try_parse_request_line()
{
    std::size_t crlf_len = 1;
    std::size_t crlf = _recv_buf.find('\n');
    if (crlf == std::string::npos) {
        L_TRACE("Client {}'s request line is not ready yet", _fd);
        return false;
    }
    if (crlf > 0 && _recv_buf[crlf - 1] == '\r') {
        --crlf;
        ++crlf_len;
    }

    std::size_t notsp = _recv_buf.find_first_not_of(" \f\r\t\v", 0);
    if (notsp >= crlf) {
        L_WARN("Client {}'s request line is empty", _fd);
        _parse_state = PARSE_ERROR;
        return false;
    }
    std::size_t sp = _recv_buf.find_first_of(" \f\r\t\v", notsp);
    if (sp >= crlf) {
        L_WARN("Client {}'s request line only contains a method, maybe?", _fd);
        _parse_state = PARSE_ERROR;
        return false;
    }
    if (!try_parse_method(notsp, sp))
        return false;

    notsp = _recv_buf.find_first_not_of(" \f\r\t\v", sp);
    if (notsp >= crlf) {
        L_WARN("Client {}'s request line doesn't contain an URI", _fd);
        _parse_state = PARSE_ERROR;
        return false;
    }
    sp = _recv_buf.find_first_of(" \f\r\t\v", notsp);
    if (_request.method == http::methods::GET) {
        sp = std::min(sp, crlf);
    } else if (sp >= crlf) {
        L_WARN("Client {}'s request line is missing an HTTP version", _fd);
        _parse_state = PARSE_ERROR;
        return false;
    }
    _request.uri = _recv_buf.substr(notsp, sp);

    notsp = _recv_buf.find_first_not_of(" \f\r\t\v", sp);
    if (notsp >= crlf && _request.method == http::methods::GET) {
        _request.version = http::versions::HTTP09;
        _recv_buf.erase(0, crlf + crlf_len);
        _parse_state = READING_HEADERS;
        return true;
    }
    if (notsp >= crlf) {
        L_WARN("Client {}'s request line is missing an HTTP version", _fd);
        _parse_state = PARSE_ERROR;
        return false;
    }
    sp = _recv_buf.find_first_of(" \f\r\t\v", notsp);
    sp = std::min(sp, crlf);
    if (!try_parse_version(notsp, sp))
        return false;

    _recv_buf.erase(0, crlf + crlf_len);
    _parse_state = READING_HEADERS;
    return true;
}

bool Connection::try_parse_method(std::size_t start, std::size_t end)
{
    const std::string::iterator m_it
        = _recv_buf.begin() + static_cast<ssize_t>(start);
    const std::string::iterator m_ite
        = _recv_buf.begin() + static_cast<ssize_t>(end);

    for (std::string::iterator it = m_it; it != m_ite; ++it)
        *it = static_cast<char>(std::toupper(*it));

    for (std::size_t i = 0; i < http::methods::COUNT; ++i) {
        if (std::equal(m_it, m_ite, http::methods::strings[i])) {
            _request.method = static_cast<http::methods::type>(i);
            return true;
        }
    }

    L_WARN("Couldn't parse client {}'s request method: '{}'", _fd,
        _recv_buf.substr(start, end - start));
    _parse_state = PARSE_ERROR;
    return false;
}

bool Connection::try_parse_version(std::size_t start, std::size_t end)
{
    const std::string::iterator v_it
        = _recv_buf.begin() + static_cast<ssize_t>(start);
    const std::string::iterator v_ite
        = _recv_buf.begin() + static_cast<ssize_t>(end);

    for (std::string::iterator it = v_it; it != v_ite; ++it)
        *it = static_cast<char>(std::toupper(*it));

    for (std::size_t i = 0; i < http::versions::COUNT; ++i) {
        if (std::equal(v_it, v_ite, http::versions::strings[i])) {
            _request.version = static_cast<http::versions::type>(i);
            return true;
        }
    }

    L_WARN("Couldn't parse client {}'s HTTP version: '{}'", _fd,
        _recv_buf.substr(start, end - start));
    _parse_state = PARSE_ERROR;
    return false;
}

namespace {

struct find_result {
    std::size_t pos;
    std::size_t len;
};

find_result find_header_end(const std::string &buf)
{
    std::size_t len = buf.size();
    for (std::size_t i = 1; i < len; ++i) {
        if (buf[i] != '\n')
            continue;

        if (buf[i - 1] == '\n') {
            // could be \r\n\n or \n\n
            if (i >= 2 && buf[i - 2] == '\r') {
                find_result r = { i - 2, 3 };
                return r;
            }
            // must be \n\n
            find_result r = { i - 1, 2 };
            return r;
        }

        if (buf[i - 1] == '\r' && i >= 2 && buf[i - 2] == '\n') {
            // could be \r\n\r\n or \n\r\n
            if (i >= 3 && buf[i - 3] == '\r') {
                find_result r = { i - 3, 4 };
                return r;
            }
            // must be \r\n\r\n
            find_result r = { i - 2, 3 };
            return r;
        }
    }

    return (find_result) { std::string::npos, 0 };
}

}

bool Connection::try_parse_headers()
{
    find_result end = find_header_end(_recv_buf);
    if (end.pos == std::string::npos) {
        L_TRACE("Client {}'s headers are not ready yet", _fd);
        return false;
    }

    std::size_t pos = _recv_buf.find_first_not_of(" \f\r\t\v");
    while (pos < end.pos) {
        std::size_t crlf_len = 1;
        std::size_t crlf = _recv_buf.find('\n', pos);
        if (crlf == std::string::npos)
            return false;
        if (crlf > 0 && _recv_buf[crlf - 1] == '\r') {
            --crlf;
            ++crlf_len;
        }

        std::size_t colon = _recv_buf.find(':');
        if (colon > crlf) {
            L_WARN("Client {} is missing a colon in it's headers", _fd);
            _parse_state = PARSE_ERROR;
            return false;
        }

        std::string key = _recv_buf.substr(pos, colon - pos);
        std::string val = _recv_buf.substr(colon + 1, crlf - colon);

        std::string::iterator ite = key.end();
        for (std::string::iterator it = key.begin(); it != ite; ++it)
            *it = static_cast<char>(std::tolower(*it));

        _request.headers[key] = val;

        pos = crlf + crlf_len;
    }

    L_TRACE("Got {} headers from client {}", _request.headers.size(), _fd);

    _recv_buf.erase(0, end.pos + end.len);
    if (_request.headers.count("content-length")) {
        _content_length = static_cast<std::size_t>(
            // NOLINTNEXTLINE(cert-err34-c,bugprone-unchecked-string-to-number-conversion)
            std::atoi(_request.headers["content-length"].c_str()));
        _parse_state = (_content_length > 0) ? READING_BODY : PARSE_COMPLETE;
    } else {
        // FIXME: for HTTP/1.1 handle Transfer-Encoding: chunked
        _content_length = 0;
        _parse_state = PARSE_COMPLETE;
    }

    if (_parse_state == PARSE_COMPLETE)
        L_TRACE("No body to parse for client {}", _fd);

    return true;
}

bool Connection::try_parse_body()
{
    if (_recv_buf.size() < _content_length) {
        L_TRACE("Client {}'s body is not ready yet", _fd);
        return false;
    }

    L_TRACE("Finished parsing client {}'s body", _fd);
    _request.body = _recv_buf.substr(0, _content_length);
    _recv_buf.erase(0, _content_length);
    _parse_state = PARSE_COMPLETE;
    return true;
}

bool Connection::do_recv()
{
    if (_recv_buf.size() >= MAX_BUF_SIZE) {
        L_WARN("Client {} exceeded it's max request size", _fd);
        _parse_state = PARSE_ERROR;
        return false;
    }

    char tmp[RECV_CHUNK];
    ssize_t n = recv(_fd, tmp, sizeof(tmp), 0);

    if (n > 0) {
        L_TRACE("Received {} bytes from client {}", n, _fd);
        _recv_buf.append(tmp, static_cast<std::size_t>(n));
        return true;
    }
    if (n == 0) {
        L_TRACE("Client {} closed cleanly", _fd);
        return false;
    }

    // n == -1: could be EAGAIN (no data) or a real error.
    // Since epoll told use the fd was readable, treat -1 as "nothing available
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
    }

    // n == 0: shouldn't happen but is fine
    // n == -1: kernel buffer ful, or EAGAIN - epoll will re-fire EPOLLOUT
    return true; // never an error, we rely on EPOLLERR/EPOLLHUP for that
}
