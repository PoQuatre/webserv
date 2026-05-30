/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dispatcher.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/29 00:00:00 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/30 03:57:53 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "dispatcher.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
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

std::string make_response(http::status::type status, const std::string &body,
    const std::string &content_type)
{
    std::ostringstream ss;
    ss << "HTTP/1.1 " << http::status::codes[status] << " "
       << http::status::reasons[status] << "\r\n";
    ss << "Content-Type: " << content_type << "\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Connection: close\r\n";
    ss << "\r\n";
    ss << body;
    return ss.str();
}

std::string make_error_response_impl(
    http::status::type status, const Config &cfg)
{
    int code = http::status::codes[status];
    std::map<uint32_t, std::string>::const_iterator it
        = cfg.error_pages.find(static_cast<uint32_t>(code));

    if (it != cfg.error_pages.end()) {
        std::string ep_path = cfg.root;
        ep_path += it->second;

        std::string content;
        if (read_file(ep_path, content))
            return make_response(status, content, "text/html");
    }

    std::ostringstream body;
    body << "<!DOCTYPE html>\n<html>\n<head><title>" << code << " "
         << http::status::reasons[status] << "</title></head>\n<body>\n<h1>"
         << code << " " << http::status::reasons[status]
         << "</h1>\n</body>\n</html>\n";
    return make_response(status, body.str(), "text/html");
}

std::string handle_get(
    const std::string &uri_path, const std::string &fs_path, const Config &cfg)
{
    struct stat st;
    if (stat(fs_path.c_str(), &st) != 0) {
        if (errno == EACCES)
            return make_error_response_impl(http::status::FORBIDDEN, cfg);
        return make_error_response_impl(http::status::NOT_FOUND, cfg);
    }

    if (S_ISDIR(st.st_mode)) {
        if (uri_path.empty() || uri_path[uri_path.size() - 1] != '/') {
            std::ostringstream ss;
            ss << "HTTP/1.1 301 Moved Permanently\r\n"
               << "Location: " << uri_path << "/\r\n"
               << "Content-Length: 0\r\n"
               << "Connection: close\r\n\r\n";
            return ss.str();
        }

        std::string index_path = fs_path + "index.html";
        struct stat ist;
        if (stat(index_path.c_str(), &ist) == 0 && S_ISREG(ist.st_mode)) {
            std::string content;
            if (read_file(index_path, content))
                return make_response(http::status::OK, content, "text/html");
        }

        return make_error_response_impl(http::status::FORBIDDEN, cfg);
    }

    if (!S_ISREG(st.st_mode))
        return make_error_response_impl(http::status::FORBIDDEN, cfg);

    if (access(fs_path.c_str(), R_OK) != 0)
        return make_error_response_impl(http::status::FORBIDDEN, cfg);

    std::string content;
    if (!read_file(fs_path, content))
        return make_error_response_impl(
            http::status::INTERNAL_SERVER_ERROR, cfg);

    return make_response(http::status::OK, content, content_type_for(fs_path));
}

}

std::string dispatcher::handle(const http::request &req, const Server &server)
{
    const Location *loc = server.find_location(req.uri);
    const Config &cfg = loc ? loc->config : server.default_config();

    if (!cfg.allowed_methods[req.method])
        return make_error_response_impl(http::status::METHOD_NOT_ALLOWED, cfg);

    if (req.method != http::methods::GET)
        return make_error_response_impl(http::status::NOT_IMPLEMENTED, cfg);

    std::string fs_path = cfg.root + req.uri;
    L_DEBUG("GET {} -> {}", req.uri, fs_path);

    return handle_get(req.uri, fs_path, cfg);
}

std::string dispatcher::error_response(http::status::type status)
{
    Config empty_cfg;
    return make_error_response_impl(status, empty_cfg);
}
