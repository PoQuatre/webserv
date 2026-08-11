/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   upload.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/11 03:22:46 by mle-flem          #+#    #+#             */
/*   Updated: 2026/08/11 03:22:47 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "upload.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <cctype>
#include <fstream>

namespace {

bool write_file(const std::string &path, const std::string &content)
{
    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);

    if (!f.is_open())
        return false;
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    return !f.fail();
}

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

bool upload_path_ready(const std::string &path)
{
    struct stat st;

    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)
        && access(path.c_str(), W_OK) == 0;
}

std::string join_path(const std::string &dir, const std::string &file)
{
    if (!dir.empty() && dir[dir.size() - 1] == '/')
        return dir + file;
    return dir + "/" + file;
}

bool safe_upload_name(const std::string &name)
{
    return !name.empty() && name != "." && name != ".."
        && name.find('/') == std::string::npos
        && name.find('\\') == std::string::npos
        && name.find('\0') == std::string::npos;
}

std::string uri_basename(const std::string &uri)
{
    std::size_t slash = uri.rfind('/');

    if (slash == std::string::npos)
        return uri;
    return uri.substr(slash + 1);
}

std::string unquote_value(const std::string &value)
{
    std::string out;

    if (value.size() < 2 || value[0] != '"' || value[value.size() - 1] != '"')
        return value;
    for (std::size_t i = 1; i + 1 < value.size(); ++i) {
        if (value[i] == '\\' && i + 2 < value.size())
            ++i;
        out += value[i];
    }
    return out;
}

bool content_type_is_multipart(const http::request &req, std::string &boundary)
{
    std::map<std::string, std::string>::const_iterator cit
        = req.headers.find("content-type");
    std::string content_type;
    std::size_t start;

    if (cit == req.headers.end())
        return false;
    content_type = cit->second;
    if (lowercase(trim(content_type.substr(0, content_type.find(';'))))
        != "multipart/form-data")
        return false;
    start = content_type.find(';');
    while (start != std::string::npos && start + 1 < content_type.size()) {
        std::size_t end = content_type.find(';', start + 1);
        std::string item = trim(content_type.substr(start + 1,
            end == std::string::npos ? std::string::npos : end - start - 1));
        std::size_t eq = item.find('=');

        if (eq != std::string::npos
            && lowercase(trim(item.substr(0, eq))) == "boundary") {
            boundary = unquote_value(trim(item.substr(eq + 1)));
            break;
        }
        start = end;
    }
    return true;
}

bool find_header_end(const std::string &body, std::size_t start,
    std::size_t &header_pos, std::size_t &header_len)
{
    std::size_t crlf = body.find("\r\n\r\n", start);
    std::size_t lf = body.find("\n\n", start);

    if (crlf == std::string::npos && lf == std::string::npos)
        return false;
    if (lf == std::string::npos || (crlf != std::string::npos && crlf < lf)) {
        header_pos = crlf;
        header_len = 4;
    } else {
        header_pos = lf;
        header_len = 2;
    }
    return true;
}

std::string multipart_header(
    const std::string &block, const std::string &wanted)
{
    std::size_t start = 0;

    while (start <= block.size()) {
        std::size_t end = block.find('\n', start);
        std::string line = block.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        std::size_t colon;

        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        colon = line.find(':');
        if (colon != std::string::npos
            && lowercase(trim(line.substr(0, colon))) == wanted)
            return trim(line.substr(colon + 1));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return "";
}

bool content_disposition_filename(
    const std::string &value, std::string &filename)
{
    std::size_t start = 0;

    while (start <= value.size()) {
        std::size_t end = value.find(';', start);
        std::string item = trim(value.substr(
            start, end == std::string::npos ? std::string::npos : end - start));
        std::size_t eq = item.find('=');

        if (eq != std::string::npos
            && lowercase(trim(item.substr(0, eq))) == "filename") {
            filename = unquote_value(trim(item.substr(eq + 1)));
            return true;
        }
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return false;
}

bool find_next_boundary(const std::string &body, const std::string &delimiter,
    std::size_t start, std::size_t &pos, std::size_t &sep_len)
{
    std::size_t crlf = body.find("\r\n" + delimiter, start);
    std::size_t lf = body.find("\n" + delimiter, start);

    if (crlf == std::string::npos && lf == std::string::npos)
        return false;
    if (lf == std::string::npos || (crlf != std::string::npos && crlf < lf)) {
        pos = crlf;
        sep_len = 2;
    } else {
        pos = lf;
        sep_len = 1;
    }
    return true;
}

bool save_raw_upload(const http::request &req, const Config &cfg)
{
    std::string filename = uri_basename(req.uri);

    return safe_upload_name(filename)
        && write_file(join_path(cfg.upload_path, filename), req.body);
}

bool save_multipart_upload(
    const http::request &req, const Config &cfg, const std::string &boundary)
{
    const std::string delimiter = "--" + boundary;
    std::size_t pos = 0;
    std::size_t saved = 0;

    while (true) {
        std::size_t header_pos;
        std::size_t header_len;
        std::size_t next;
        std::size_t sep_len;
        std::string headers;
        std::string filename;

        if (req.body.compare(pos, delimiter.size(), delimiter) != 0)
            return false;
        pos += delimiter.size();
        if (req.body.compare(pos, 2, "--") == 0)
            return saved > 0;
        if (req.body.compare(pos, 2, "\r\n") == 0)
            pos += 2;
        else if (req.body.compare(pos, 1, "\n") == 0)
            pos += 1;
        else
            return false;
        if (!find_header_end(req.body, pos, header_pos, header_len))
            return false;
        if (!find_next_boundary(
                req.body, delimiter, header_pos + header_len, next, sep_len))
            return false;
        headers = req.body.substr(pos, header_pos - pos);
        pos = next + sep_len;
        if (!content_disposition_filename(
                multipart_header(headers, "content-disposition"), filename))
            continue;
        if (!safe_upload_name(filename))
            return false;
        if (!write_file(join_path(cfg.upload_path, filename),
                req.body.substr(
                    header_pos + header_len, next - header_pos - header_len)))
            return false;
        ++saved;
    }
}

}

http::status::type upload::save(const http::request &req, const Config &cfg)
{
    std::string boundary;
    bool ok;

    if (!upload_path_ready(cfg.upload_path))
        return http::status::FORBIDDEN;
    if (cfg.client_max_body_size != 0
        && req.body.size() > cfg.client_max_body_size)
        return http::status::PAYLOAD_TOO_LARGE;
    if (content_type_is_multipart(req, boundary))
        ok = !boundary.empty() && save_multipart_upload(req, cfg, boundary);
    else
        ok = save_raw_upload(req, cfg);
    if (!ok)
        return http::status::BAD_REQUEST;
    return http::status::CREATED;
}
