/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 06:06:28 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/18 06:06:48 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cgi.hpp"

#include <cctype>
#include <sstream>
#include <vector>

namespace {

struct Header {
    std::string name;
    std::string lower_name;
    std::string value;
};

struct HeaderEnd {
    std::size_t pos;
    std::size_t len;
};

std::string trim(const std::string &s)
{
    std::size_t first = 0;
    std::size_t last = s.size();

    while (first < last && (s[first] == ' ' || s[first] == '\t'))
        ++first;
    while (last > first && (s[last - 1] == ' ' || s[last - 1] == '\t'))
        --last;
    return s.substr(first, last - first);
}

std::string lowercase(const std::string &s)
{
    std::string out = s;

    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(
            std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

bool starts_with(const std::string &s, const char *prefix)
{
    std::size_t i = 0;

    while (prefix[i]) {
        if (i >= s.size() || s[i] != prefix[i])
            return false;
        ++i;
    }
    return true;
}

bool find_header_end(const std::string &output, HeaderEnd &end)
{
    std::size_t crlf = output.find("\r\n\r\n");
    std::size_t lf = output.find("\n\n");

    if (crlf == std::string::npos && lf == std::string::npos)
        return false;
    if (lf == std::string::npos || (crlf != std::string::npos && crlf < lf)) {
        end.pos = crlf;
        end.len = 4;
    } else {
        end.pos = lf;
        end.len = 2;
    }
    return true;
}

bool is_token_char(char c)
{
    static const std::string extra = "!#$%&'*+-.^_`|~";
    unsigned char uc = static_cast<unsigned char>(c);

    return std::isalnum(uc) || extra.find(c) != std::string::npos;
}

bool parse_header_line(const std::string &line, Header &header)
{
    std::size_t colon = line.find(':');

    if (colon == std::string::npos || colon == 0)
        return false;
    header.name = line.substr(0, colon);
    for (std::size_t i = 0; i < header.name.size(); ++i) {
        if (!is_token_char(header.name[i]))
            return false;
    }
    header.lower_name = lowercase(header.name);
    header.value = trim(line.substr(colon + 1));
    return true;
}

Header make_header(const std::string &name, const std::string &value)
{
    Header header;

    header.name = name;
    header.lower_name = lowercase(name);
    header.value = value;
    return header;
}

bool parse_headers(const std::string &block, std::vector<Header> &headers)
{
    std::size_t start = 0;

    while (start <= block.size()) {
        std::size_t end = block.find('\n', start);
        std::string line = block.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        Header header;

        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty() || !parse_header_line(line, header))
            return false;
        headers.push_back(header);
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return true;
}

std::string known_reason(int code)
{
    for (std::size_t i = 0; i < http::status::COUNT; ++i) {
        if (http::status::codes[i] == code)
            return http::status::reasons[i];
    }
    return "";
}

bool parse_status_value(
    const std::string &value, int &code, std::string &reason)
{
    std::string text = trim(value);

    if (text.size() < 3 || !std::isdigit(static_cast<unsigned char>(text[0]))
        || !std::isdigit(static_cast<unsigned char>(text[1]))
        || !std::isdigit(static_cast<unsigned char>(text[2])))
        return false;
    if (text.size() > 3 && text[3] != ' ' && text[3] != '\t')
        return false;
    code = ((text[0] - '0') * 100) + ((text[1] - '0') * 10) + (text[2] - '0');
    if (code < 100 || code > 599)
        return false;
    reason = text.size() > 3 ? trim(text.substr(3)) : known_reason(code);
    return true;
}

std::string version_string(const http::request &req)
{
    if (req.version < http::versions::COUNT)
        return http::versions::strings[req.version];
    return "HTTP/1.1";
}

std::string make_response(const http::request &req, int code,
    const std::string &reason, const std::vector<Header> &headers,
    const std::string &body)
{
    std::ostringstream ss;

    ss << version_string(req) << " " << code << " " << reason << "\r\n";
    for (std::size_t i = 0; i < headers.size(); ++i) {
        if (headers[i].lower_name == "status"
            || headers[i].lower_name == "content-length")
            continue;
        ss << headers[i].name << ": " << headers[i].value << "\r\n";
    }
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Connection: " << (req.keep_alive ? "keep-alive" : "close") << "\r\n";
    ss << "\r\n";
    if (req.method != http::methods::HEAD)
        ss << body;
    return ss.str();
}

std::string make_body_response(
    const http::request &req, const std::string &body)
{
    std::vector<Header> headers;

    headers.push_back(make_header("Content-Type", "text/html"));
    return make_response(req, 200, "OK", headers, body);
}

std::string make_bad_gateway(const http::request &req)
{
    std::vector<Header> headers;

    headers.push_back(make_header("Content-Type", "text/html"));
    return make_response(req, 502, "Bad Gateway", headers,
        "<html><body><h1>502 Bad Gateway</h1></body></html>\n");
}

bool parse_nph_status_line(const std::string &line)
{
    std::size_t first_space = line.find(' ');
    int code;
    std::string reason;

    if (first_space != 8)
        return false;
    if (line.compare(0, 8, "HTTP/1.0") != 0
        && line.compare(0, 8, "HTTP/1.1") != 0)
        return false;
    return parse_status_value(line.substr(first_space + 1), code, reason);
}

std::string translate_nph(const std::string &output, const http::request &req)
{
    HeaderEnd header_end;
    std::size_t line_end;
    std::string first_line;
    std::vector<Header> headers;

    if (!find_header_end(output, header_end))
        return make_bad_gateway(req);
    line_end = output.find('\n');
    if (line_end == std::string::npos || line_end > header_end.pos)
        return make_bad_gateway(req);
    first_line = output.substr(0, line_end);
    if (!first_line.empty() && first_line[first_line.size() - 1] == '\r')
        first_line.erase(first_line.size() - 1);
    if (!parse_nph_status_line(first_line))
        return make_bad_gateway(req);
    if (line_end + 1 < header_end.pos
        && !parse_headers(
            output.substr(line_end + 1, header_end.pos - line_end - 1),
            headers))
        return make_bad_gateway(req);
    if (req.method == http::methods::HEAD)
        return output.substr(0, header_end.pos + header_end.len);
    return output;
}

std::string translate_parsed(const std::string &output,
    const http::request &req, const HeaderEnd &header_end)
{
    std::string block = output.substr(0, header_end.pos);
    std::string body = output.substr(header_end.pos + header_end.len);
    std::vector<Header> headers;
    int code = 200;
    std::string reason = "OK";
    bool has_status = false;
    bool has_location = false;

    if (block.empty())
        return make_body_response(req, output);
    if (!parse_headers(block, headers))
        return make_bad_gateway(req);
    for (std::size_t i = 0; i < headers.size(); ++i) {
        if (headers[i].lower_name == "status") {
            has_status = true;
            if (!parse_status_value(headers[i].value, code, reason))
                return make_bad_gateway(req);
        } else if (headers[i].lower_name == "location") {
            has_location = true;
        }
    }
    if (has_location && !has_status) {
        code = http::status::codes[http::status::MOVED_TEMPORARILY];
        reason = http::status::reasons[http::status::MOVED_TEMPORARILY];
    }
    return make_response(req, code, reason, headers, body);
}

}

std::string cgi::translate_output(
    const std::string &output, const http::request &req)
{
    HeaderEnd header_end;

    if (starts_with(output, "HTTP/"))
        return translate_nph(output, req);
    if (!find_header_end(output, header_end))
        return make_body_response(req, output);
    return translate_parsed(output, req, header_end);
}
