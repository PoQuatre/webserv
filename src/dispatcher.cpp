/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dispatcher.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/29 00:00:00 by mle-flem          #+#    #+#             */
/*   Updated: 2026/08/11 03:12:30 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "dispatcher.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "logger.hpp"

namespace {

struct MimeType {
    const char *ext;
    const char *type;
};

const MimeType MIME_TYPES[] = {
    { ".html", "text/html" },
    { ".htm", "text/html" },
    { ".css", "text/css" },
    { ".js", "application/javascript" },
    { ".json", "application/json" },
    { ".png", "image/png" },
    { ".jpg", "image/jpeg" },
    { ".jpeg", "image/jpeg" },
    { ".gif", "image/gif" },
    { ".svg", "image/svg+xml" },
    { ".ico", "image/x-icon" },
    { ".txt", "text/plain" },
    { ".pdf", "application/pdf" },
    { ".xml", "application/xml" },
    { NULL, NULL },
};

std::string make_response(const http::request &req, http::status::type status,
    const std::string &body, const std::string &content_type);
std::string make_error_response_impl(
    const http::request &req, http::status::type status, const Config &cfg);
std::string unquote_value(const std::string &value);

std::string content_type_for(const std::string &path)
{
    std::size_t dot = path.rfind('.');
    if (dot != std::string::npos) {
        std::string ext = path.substr(dot);
        for (int i = 0; MIME_TYPES[i].ext; i++) {
            if (ext == MIME_TYPES[i].ext)
                return MIME_TYPES[i].type;
        }
    }
    return "application/octet-stream";
}

bool read_file(const std::string &path, std::string &out)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f.is_open())
        return false;
    out.assign(
        std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !f.fail();
}

std::string escape_html(const std::string &value)
{
    std::string escaped;

    for (std::string::const_iterator it = value.begin(); it != value.end();
        ++it) {
        if (*it == '&')
            escaped += "&amp;";
        else if (*it == '<')
            escaped += "&lt;";
        else if (*it == '>')
            escaped += "&gt;";
        else if (*it == '"')
            escaped += "&quot;";
        else if (*it == '\'')
            escaped += "&#39;";
        else
            escaped += *it;
    }
    return escaped;
}

std::string encode_uri_component(const std::string &value)
{
    const char hex[] = "0123456789ABCDEF";
    std::string encoded;

    for (std::string::const_iterator it = value.begin(); it != value.end();
        ++it) {
        unsigned char c = static_cast<unsigned char>(*it);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += static_cast<char>(c);
        } else {
            encoded += '%';
            encoded += hex[c >> 4];
            encoded += hex[c & 0x0f];
        }
    }
    return encoded;
}

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

std::string handle_upload(const http::request &req, const Config &cfg)
{
    std::string boundary;
    bool ok;

    if (!upload_path_ready(cfg.upload_path))
        return make_error_response_impl(req, http::status::FORBIDDEN, cfg);
    if (cfg.client_max_body_size != 0
        && req.body.size() > cfg.client_max_body_size)
        return make_error_response_impl(
            req, http::status::PAYLOAD_TOO_LARGE, cfg);
    if (content_type_is_multipart(req, boundary))
        ok = !boundary.empty() && save_multipart_upload(req, cfg, boundary);
    else
        ok = save_raw_upload(req, cfg);
    if (!ok)
        return make_error_response_impl(req, http::status::BAD_REQUEST, cfg);
    return make_response(req, http::status::CREATED, "Created\n", "text/plain");
}

std::string make_response(const http::request &req, http::status::type status,
    const std::string &body, const std::string &content_type)
{
    if (req.version == http::versions::HTTP09)
        return body;

    std::ostringstream ss;
    ss << http::versions::strings[req.version] << " "
       << http::status::codes[status] << " " << http::status::reasons[status]
       << "\r\n";
    ss << "Content-Type: " << content_type << "\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Connection: " << (req.keep_alive ? "keep-alive" : "close") << "\r\n";
    ss << "\r\n";
    ss << body;
    return ss.str();
}

std::string make_error_response_impl(
    const http::request &req, http::status::type status, const Config &cfg)
{
    int code = http::status::codes[status];
    std::map<uint32_t, std::string>::const_iterator cit
        = cfg.error_pages.find(static_cast<uint32_t>(code));

    if (cit != cfg.error_pages.end()) {
        std::string ep_path = cfg.root;
        ep_path += cit->second;

        std::string content;
        if (read_file(ep_path, content))
            return make_response(req, status, content, "text/html");
    }

    std::ostringstream body;
    body << "<!DOCTYPE html>\n<html>\n<head><title>" << code << " "
         << http::status::reasons[status] << "</title></head>\n<body>\n<h1>"
         << code << " " << http::status::reasons[status]
         << "</h1>\n</body>\n</html>\n";
    return make_response(req, status, body.str(), "text/html");
}

std::string make_directory_index_response(
    const http::request &req, const std::string &fs_path, const Config &cfg)
{
    std::ostringstream ss;
    std::string escaped_uri = escape_html(req.uri);

    ss << "<html><head><title>Index of " << escaped_uri << "</title></head>";
    ss << "<body><h1>Index of " << escaped_uri << "</h1><hr>";
    ss << "<pre>";

    DIR *dir = opendir(fs_path.c_str());
    if (dir == NULL) {
        if (errno == EACCES)
            return make_error_response_impl(req, http::status::FORBIDDEN, cfg);
        return make_error_response_impl(
            req, http::status::INTERNAL_SERVER_ERROR, cfg);
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (std::strcmp(entry->d_name, ".") == 0)
            continue;

        struct stat st;
        std::string filename(entry->d_name);
        if (stat((fs_path + filename).c_str(), &st)) {
            closedir(dir);
            return make_error_response_impl(
                req, http::status::INTERNAL_SERVER_ERROR, cfg);
        }

        std::tm *time_last_change = localtime(&st.st_mtime);
        if (time_last_change == NULL) {
            closedir(dir);
            return make_error_response_impl(
                req, http::status::INTERNAL_SERVER_ERROR, cfg);
        }

        ss << "<a href=\"" << encode_uri_component(filename) << "\">"
           << escape_html(filename).substr(0, 39) << "</a>";

        char date[256];
        (void)std::strftime(
            date, sizeof(date), "%d-%b-%Y %H:%M", time_last_change);
        if (filename.size() < 40)
            ss << std::setw(static_cast<int>(40 - filename.size())) << " ";
        ss << date;
        ss << std::setw(20) << " ";
        if (S_ISREG(st.st_mode)) {
            ss << st.st_size;
        } else {
            ss << "-";
        }
        ss << "\n";
    }
    closedir(dir);

    ss << "</pre>";
    ss << "<hr></body>";
    ss << "</html>";

    return make_response(req, http::status::OK, ss.str(), "text/html");
}

std::string make_directory_redirect_response(const http::request &req)
{
    std::ostringstream ss;

    ss << http::versions::strings[req.version] << " 301 Moved Permanently\r\n"
       << "Location: " << req.uri << "/\r\n"
       << "Content-Length: 0\r\n"
       << "Connection: " << (req.keep_alive ? "keep-alive" : "close")
       << "\r\n\r\n";
    return ss.str();
}

bool try_directory_index(const http::request &req, const std::string &fs_path,
    const Config &cfg, std::string &response)
{
    std::vector<std::string> indexes = cfg.conf.index;

    if (indexes.empty())
        indexes.push_back("index.html");
    for (std::vector<std::string>::const_iterator it = indexes.begin();
        it != indexes.end(); ++it) {
        std::string index_path = fs_path + *it;
        struct stat index_stat;
        if (stat(index_path.c_str(), &index_stat) != 0) {
            if (errno == ENOENT || errno == ENOTDIR)
                continue;
            if (errno == EACCES)
                response = make_error_response_impl(
                    req, http::status::FORBIDDEN, cfg);
            else
                response = make_error_response_impl(
                    req, http::status::INTERNAL_SERVER_ERROR, cfg);
            return true;
        }
        if (!S_ISREG(index_stat.st_mode))
            continue;

        std::string content;
        if (access(index_path.c_str(), R_OK) != 0) {
            response
                = make_error_response_impl(req, http::status::FORBIDDEN, cfg);
            return true;
        }
        if (!read_file(index_path, content)) {
            response = make_error_response_impl(
                req, http::status::INTERNAL_SERVER_ERROR, cfg);
            return true;
        }
        response = make_response(req, http::status::OK, content, "text/html");
        return true;
    }
    return false;
}

std::string handle_get(
    const http::request &req, const std::string &fs_path, const Config &cfg)
{
    struct stat st;

    if (stat(fs_path.c_str(), &st) != 0) {

        if (errno == ENOENT || errno == ENOTDIR)
            return make_error_response_impl(req, http::status::NOT_FOUND, cfg);
        if (errno == EACCES)
            return make_error_response_impl(req, http::status::FORBIDDEN, cfg);
        if (errno == ENAMETOOLONG)
            return make_error_response_impl(
                req, http::status::BAD_REQUEST, cfg);
        return make_error_response_impl(
            req, http::status::INTERNAL_SERVER_ERROR, cfg);
    }

    if (S_ISDIR(st.st_mode)) {
        std::string response;

        if (req.uri.empty() || req.uri[req.uri.size() - 1] != '/')
            return make_directory_redirect_response(req);
        if (try_directory_index(req, fs_path, cfg, response))
            return response;

        if (cfg.autoindex)
            return make_directory_index_response(req, fs_path, cfg);
        return make_error_response_impl(req, http::status::FORBIDDEN, cfg);
    }

    if (!S_ISREG(st.st_mode))
        return make_error_response_impl(req, http::status::FORBIDDEN, cfg);

    if (access(fs_path.c_str(), R_OK) != 0)
        return make_error_response_impl(req, http::status::FORBIDDEN, cfg);

    std::string content;
    if (!read_file(fs_path, content))
        return make_error_response_impl(
            req, http::status::INTERNAL_SERVER_ERROR, cfg);

    return make_response(
        req, http::status::OK, content, content_type_for(fs_path));
}

}

const Config &dispatcher::config_for(
    const http::request &req, const Server &server)
{
    const Location *loc = server.find_location(req.uri);

    return loc ? loc->config : server.default_config();
}

std::string dispatcher::handle(const http::request &req, const Server &server)
{
    const Config &cfg = config_for(req, server);
    std::string filesystem_path;

    if (!cfg.allowed_methods[req.method])
        return make_error_response_impl(
            req, http::status::METHOD_NOT_ALLOWED, cfg);

    filesystem_path = cfg.root + req.uri;

    if (req.method == http::methods::POST && !cfg.upload_path.empty())
        return handle_upload(req, cfg);

    if (req.method != http::methods::GET)
        return make_error_response_impl(
            req, http::status::NOT_IMPLEMENTED, cfg);

    L_DEBUG("GET {} -> {}", req.uri, filesystem_path);

    return handle_get(req, filesystem_path, cfg);
}

std::string dispatcher::error_response(http::status::type status)
{
    Config empty_cfg = { };
    http::request empty_req = { };
    return make_error_response_impl(empty_req, status, empty_cfg);
}

std::string dispatcher::error_response(
    const http::request &req, const Config &cfg, http::status::type status)
{
    return make_error_response_impl(req, status, cfg);
}
