/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpParser.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 20:54:18 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/24 15:27:54 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpParser.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "logger.hpp"

#define MAX_BUF_SIZE 8388608 // 8mb

HttpParser::HttpParser()
    : _state(READING_REQUEST_LINE)
    , _content_length(0)
    , _eof(false)
    , _request()
    , _chunked(false)
{
}

bool HttpParser::feed(const char *data, std::size_t len)
{
    if (_buf.size() >= MAX_BUF_SIZE) {
        L_WARN("Request exceeded max size");
        _state = ERROR;
        return false;
    }
    _buf.append(data, len);
    return run();
}

void HttpParser::set_eof()
{
    _eof = true;
    if (!_buf.empty())
        run();
}

void HttpParser::reset()
{
    _buf.clear();
    _state = READING_REQUEST_LINE;
    _content_length = 0;
    _eof = false;
    _chunked = false;
    _request = http::request();
}

bool HttpParser::run()
{
    bool progressed = true;
    while (progressed && _state != COMPLETE && _state != ERROR) {
        progressed = false;
        switch (_state) {
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
    return _state != ERROR;
}

bool HttpParser::try_parse_request_line()
{
    std::size_t crlf_len = 1;
    std::size_t crlf = _buf.find('\n');
    if (crlf == std::string::npos) {
        if (!_eof) {
            L_TRACE("Request line not ready yet");
            return false;
        }
        crlf = _buf.size();
        crlf_len = 0;
    } else if (crlf > 0 && _buf[crlf - 1] == '\r') {
        --crlf;
        ++crlf_len;
    }

    std::size_t notsp = _buf.find_first_not_of(" \f\r\t\v", 0);
    if (notsp >= crlf) {
        L_WARN("Request line is empty");
        _state = ERROR;
        return false;
    }
    std::size_t sp = _buf.find_first_of(" \f\r\t\v", notsp);
    if (sp >= crlf) {
        L_WARN("Request line only contains a method");
        _state = ERROR;
        return false;
    }
    if (!try_parse_method(notsp, sp))
        return false;

    notsp = _buf.find_first_not_of(" \f\r\t\v", sp);
    if (notsp >= crlf) {
        L_WARN("Request line doesn't contain a URI");
        _state = ERROR;
        return false;
    }
    sp = _buf.find_first_of(" \f\r\t\v", notsp);
    if (_request.method == http::methods::GET) {
        sp = std::min(sp, crlf);
    } else if (sp >= crlf) {
        L_WARN("Request line is missing an HTTP version");
        _state = ERROR;
        return false;
    }
    _request.uri = _buf.substr(notsp, sp - notsp);

    notsp = _buf.find_first_not_of(" \f\r\t\v", sp);
    if (notsp >= crlf && _request.method == http::methods::GET) {
        _request.version = http::versions::HTTP09;
        _buf.erase(0, crlf + crlf_len);
        _state = COMPLETE;
        return true;
    }
    if (notsp >= crlf) {
        L_WARN("Request line is missing an HTTP version");
        _state = ERROR;
        return false;
    }
    sp = _buf.find_first_of(" \f\r\t\v", notsp);
    sp = std::min(sp, crlf);
    if (!try_parse_version(notsp, sp))
        return false;

    _buf.erase(0, crlf + crlf_len);
    _state = READING_HEADERS;
    return true;
}

bool HttpParser::try_parse_method(std::size_t start, std::size_t end)
{
    const std::string::iterator m_it
        = _buf.begin() + static_cast<ssize_t>(start);
    const std::string::iterator m_ite
        = _buf.begin() + static_cast<ssize_t>(end);

    for (std::string::iterator it = m_it; it != m_ite; ++it)
        *it = static_cast<char>(std::toupper(*it));

    for (std::size_t i = 0; i < http::methods::COUNT; ++i) {
        if ((end - start) == std::strlen(http::methods::strings[i])
            && std::equal(m_it, m_ite, http::methods::strings[i])) {
            _request.method = static_cast<http::methods::type>(i);
            return true;
        }
    }

    L_WARN(
        "Couldn't parse request method: '{}'", _buf.substr(start, end - start));
    _state = ERROR;
    return false;
}

bool HttpParser::try_parse_version(std::size_t start, std::size_t end)
{
    const std::string::iterator v_it
        = _buf.begin() + static_cast<ssize_t>(start);
    const std::string::iterator v_ite
        = _buf.begin() + static_cast<ssize_t>(end);

    for (std::string::iterator it = v_it; it != v_ite; ++it)
        *it = static_cast<char>(std::toupper(*it));

    for (std::size_t i = 0; i < http::versions::COUNT; ++i) {
        if ((end - start) == std::strlen(http::versions::strings[i])
            && std::equal(v_it, v_ite, http::versions::strings[i])) {
            _request.version = static_cast<http::versions::type>(i);
            return true;
        }
    }

    L_WARN(
        "Couldn't parse HTTP version: '{}'", _buf.substr(start, end - start));
    _state = ERROR;
    return false;
}

namespace {

std::string trim_lws(const std::string &buf, std::size_t start, std::size_t end)
{
    std::size_t s = buf.find_first_not_of(" \t", start);
    if (s == std::string::npos || s >= end)
        return "";
    std::size_t e = buf.find_last_not_of(" \t", end - 1);
    return buf.substr(s, e - s + 1);
}

struct find_result {
    std::size_t pos;
    std::size_t len;
};

find_result find_header_end(const std::string &buf)
{
    if (!buf.empty() && buf[0] == '\n') {
        find_result r = { 0, 1 };
        return r;
    }
    if (buf.size() >= 2 && buf[0] == '\r' && buf[1] == '\n') {
        find_result r = { 0, 2 };
        return r;
    }

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
            // must be \n\r\n
            find_result r = { i - 2, 3 };
            return r;
        }
    }

    find_result r = { std::string::npos, 0 };
    return r;
}

}

bool HttpParser::try_parse_headers()
{
    find_result end = find_header_end(_buf);
    if (end.pos == std::string::npos) {
        if (!_eof) {
            L_TRACE("Headers not ready yet");
            return false;
        }
        end.pos = _buf.size();
        end.len = 0;
    }

    std::size_t pos = _buf.find_first_not_of(" \f\r\t\v");
    while (pos < end.pos)
        if (!try_parse_header_field(pos, end.pos))
            return false;

    L_TRACE("Got {} headers", _request.headers.size());

    _buf.erase(0, end.pos + end.len);
    if (_request.version == http::versions::HTTP11
        && _request.headers.count("transfer-encoding")
        && _request.headers["transfer-encoding"] == "chunked") {
        _state = READING_BODY;
        _chunked = true;
    } else if (_request.headers.count("content-length")) {
        const std::string &cl_str = _request.headers["content-length"];
        if (cl_str.empty() || cl_str[0] == '-') {
            L_WARN("Invalid content-length: '{}'", cl_str);
            _state = ERROR;
            return false;
        }

        char *end_ptr;
        errno = 0;
        _content_length = static_cast<std::size_t>(
            std::strtoull(cl_str.c_str(), &end_ptr, 10));
        if (end_ptr == cl_str.c_str() || *end_ptr != '\0' || errno == ERANGE) {
            L_WARN("Invalid content-length: '{}'", cl_str);
            _state = ERROR;
            return false;
        }
        _state = (_content_length > 0) ? READING_BODY : COMPLETE;
    } else {
        _content_length = 0;
        _state = COMPLETE;
    }

    if (_state == COMPLETE)
        L_TRACE("No body to parse");

    return true;
}

bool HttpParser::try_parse_header_field(std::size_t &pos, std::size_t end_pos)
{
    std::size_t crlf_len = 1;
    std::size_t crlf = _buf.find('\n', pos);
    if (crlf == std::string::npos) {
        if (!_eof)
            return false;
        crlf = end_pos;
        crlf_len = 0;
    } else if (crlf > 0 && _buf[crlf - 1] == '\r') {
        --crlf;
        ++crlf_len;
    }

    std::size_t colon = _buf.find(':', pos);
    if (colon > crlf) {
        L_WARN("Header is missing a colon");
        _state = ERROR;
        return false;
    }

    std::string key = trim_lws(_buf, pos, colon);
    std::string val = trim_lws(_buf, colon + 1, crlf);

    pos = crlf + crlf_len;

    while (pos < end_pos && (_buf[pos] == ' ' || _buf[pos] == '\t')) {
        std::size_t cont_crlf_len = 1;
        std::size_t cont_crlf = _buf.find('\n', pos);
        if (cont_crlf == std::string::npos) {
            cont_crlf = end_pos;
            cont_crlf_len = 0;
        } else if (cont_crlf > 0 && _buf[cont_crlf - 1] == '\r') {
            --cont_crlf;
            ++cont_crlf_len;
        }
        std::string cont = trim_lws(_buf, pos, cont_crlf);
        if (!cont.empty()) {
            val += ' ';
            val += cont;
        }
        pos = cont_crlf + cont_crlf_len;
    }

    std::string::iterator ite = key.end();
    for (std::string::iterator it = key.begin(); it != ite; ++it)
        *it = static_cast<char>(std::tolower(*it));

    _request.headers[key] = val;
    return true;
}

bool HttpParser::try_parse_body()
{
    if (_chunked) {
        while (try_parse_chunk())
            ;
        return _state != READING_BODY;
    }

    if (_buf.size() < _content_length) {
        L_TRACE("Body not ready yet");
        return false;
    }

    L_TRACE("Finished parsing body");
    _request.body = _buf.substr(0, _content_length);
    _buf.erase(0, _content_length);
    _state = COMPLETE;
    return true;
}

bool HttpParser::try_parse_chunk()
{
    std::size_t crlf_len = 1;
    std::size_t crlf = _buf.find('\n');
    if (crlf == std::string::npos) {
        if (!_eof) {
            L_TRACE("No chunk size ready to be received");
            return false;
        }
        crlf = _buf.size();
        crlf_len = 0;
    } else if (crlf > 0 && _buf[crlf - 1] == '\r') {
        --crlf;
        ++crlf_len;
    }

    char *end;
    uint64_t length = std::strtol(_buf.c_str(), &end, 16);
    if (crlf != static_cast<std::size_t>(end - _buf.c_str())) {
        L_ERROR("Failed to parse chunk size");
        _state = ERROR;
        return false;
    }

    std::size_t start = crlf + crlf_len;

    crlf_len = 1;
    crlf = _buf.find('\n', start + length);
    if (crlf == std::string::npos) {
        if (!_eof) {
            L_TRACE("No chunk data ready to be received");
            return false;
        }
        crlf = _buf.size();
        crlf_len = 0;
    } else if (crlf > 0 && _buf[crlf - 1] == '\r') {
        --crlf;
        ++crlf_len;
    }

    if (length == 0) {
        L_TRACE("Request complete");
        _state = COMPLETE;
        _buf.erase(0, crlf + crlf_len);
        return false;
    }

    _request.body.append(_buf, start, length);
    _buf.erase(0, crlf + crlf_len);
    return true;
}
