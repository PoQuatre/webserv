/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dispatcher.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/29 00:00:00 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/28 01:34:43 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "dispatcher.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

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
    L_DEBUG("Responded with error code {}", http::status::codes[status]);

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

std::string create_index_directory(
    const http::request &req, const std::string &fs_path, const Config &cfg)
{
    std::ostringstream ss;
    ss << "<html><head><title>Index of " << req.uri << "</title></head>";
    ss << "<body><h1>Index of " << req.uri << "</h1><hr>";
    ss << "<pre>";

    DIR *dp;
    struct dirent *ep;
    dp = opendir(fs_path.c_str());
    if (dp != NULL) {
        if (!readdir(dp)) {
            closedir(dp);
            return make_error_response_impl(
                req, http::status::INTERNAL_SERVER_ERROR, cfg);
        }
        while ((ep = readdir(dp)) != NULL) {

            struct stat st;
            std::string filename(ep->d_name);

            if (stat((fs_path + filename).c_str(), &st)) {
                closedir(dp);
                return make_error_response_impl(
                    req, http::status::INTERNAL_SERVER_ERROR, cfg);
            }

            std::tm *time_last_change = localtime(&st.st_mtim.tv_sec);

            ss << "<a href=\"" << filename << "\">" << filename.substr(0, 39)
               << "</a>";

            if (std::strcmp(ep->d_name, "../") != 0) {
                char date[256];
                if (std::strftime(date, 256, "%d-%b-%Y %R", time_last_change)
                    == 0) {
                    return make_error_response_impl(
                        req, http::status::INTERNAL_SERVER_ERROR, cfg);
                }
                ss << std::setw(static_cast<int>(40 - filename.size())) << " ";
                ss << date;
                ss << std::setw(20) << " ";
                if (ep->d_type == DT_REG) {
                    ss << st.st_size;
                } else {
                    ss << "-";
                }
            }
            ss << "\n";
        }
        closedir(dp);
    }

    ss << "</pre>";
    ss << "<hr></body>";
    ss << "</html>";

    return make_response(req, http::status::OK, ss.str(), "text/html");
}

std::string handle_get(
    const http::request &req, const std::string &fs_path, const Config &cfg)
{
    struct stat st;

    if (stat(fs_path.c_str(), &st) != 0) {

        if (errno == ENOENT)
            return make_error_response_impl(req, http::status::NOT_FOUND, cfg);
        if (errno == EACCES)
            return make_error_response_impl(req, http::status::FORBIDDEN, cfg);
    }

    std::string uri_path = req.uri;
    if (S_ISDIR(st.st_mode)) {
        if (uri_path.empty() || uri_path[uri_path.size() - 1] != '/') {
            std::ostringstream ss;
            ss << http::versions::strings[req.version]
               << " 301 Moved Permanently\r\n"
               << "Location: " << uri_path << "/\r\n"
               << "Content-Length: 0\r\n"
               << "Connection: " << (req.keep_alive ? "keep-alive" : "close")
               << "\r\n\r\n";
            return ss.str();
        }

        std::string index_path = fs_path + "index.html";
        struct stat ist;
        if (stat(index_path.c_str(), &ist) == 0 && S_ISREG(ist.st_mode)) {
            std::string content;
            if (read_file(index_path, content))
                return make_response(
                    req, http::status::OK, content, "text/html");
        }

        if (cfg.autoindex)
            return create_index_directory(req, fs_path, cfg);
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
