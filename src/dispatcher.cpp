/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dispatcher.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/29 00:00:00 by mle-flem          #+#    #+#             */
/*   Updated: 2026/08/20 05:15:39 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "dispatcher.hpp"

#include <dirent.h>
#include <fcntl.h>
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
#include "upload.hpp"

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

std::string without_trailing_slash(std::string path)
{
    while (!path.empty() && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);
    return path;
}

bool autoindex_can_delete(const std::string &fs_path, const Config &cfg)
{
    return !cfg.upload_path.empty()
        && without_trailing_slash(cfg.upload_path)
        == without_trailing_slash(fs_path)
        && cfg.allowed_methods[http::methods::DELETE];
}

bool is_regular_entry(const std::string &path)
{
    struct stat st;

    return lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
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

std::string make_response_headers(const http::request &req,
    http::status::type status, off_t content_length,
    const std::string &content_type)
{
    if (req.version == http::versions::HTTP09)
        return "";

    std::ostringstream ss;
    ss << http::versions::strings[req.version] << " "
       << http::status::codes[status] << " " << http::status::reasons[status]
       << "\r\n";
    ss << "Content-Type: " << content_type << "\r\n";
    ss << "Content-Length: " << content_length << "\r\n";
    ss << "Connection: " << (req.keep_alive ? "keep-alive" : "close") << "\r\n";
    ss << "\r\n";
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
    bool show_delete = autoindex_can_delete(fs_path, cfg);

    ss << "<html><head><title>Index of " << escaped_uri << "</title></head>";
    ss << "<body><h1>Index of " << escaped_uri << "</h1><hr>";
    if (show_delete) {
        ss << "<script>function deleteEntry(path){if(!confirm('Delete '+path";
        ss << "+'?'))";
        ss << "return;fetch(path,{method:'DELETE'}).then(function(r){if(r.ok)";
        ss << "location.reload();else alert('Delete failed: '+r.status);});}";
        ss << "</script>";
    }
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
        std::string entry_path = fs_path + filename;

        if (stat(entry_path.c_str(), &st)) {
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

        std::string encoded_filename = encode_uri_component(filename);

        ss << "<a href=\"" << encoded_filename << "\">"
           << escape_html(filename).substr(0, 39) << "</a>";
        if (show_delete && is_regular_entry(entry_path))
            ss << " <button type=\"button\" onclick=\"deleteEntry('"
               << encoded_filename << "')\">delete</button>";

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

const char *reason_for_status(uint32_t status)
{
    for (std::size_t i = 0; i < http::status::COUNT; ++i) {
        if (http::status::codes[i] == static_cast<int32_t>(status))
            return http::status::reasons[i];
    }
    return "Found";
}

std::string make_redirect_response(
    const http::request &req, uint32_t status, const std::string &target)
{
    std::ostringstream ss;

    ss << http::versions::strings[req.version] << " " << status << " "
       << reason_for_status(status) << "\r\n"
       << "Location: " << target << "\r\n"
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
        std::string index_path = fs_path + "/" + *it;
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
            return make_redirect_response(req, 301, req.uri + "/");
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

std::string dispatcher::filesystem_path_for(
    const http::request &req, const Server &server)
{
    const Location *loc = server.find_location(req.uri);
    const Config &cfg = loc ? loc->config : server.default_config();

    if (loc && !cfg.conf.root.empty() && loc->type != location::CASE_SENSITIVE
        && loc->type != location::CASE_INSENSITIVE
        && (req.uri.size() == loc->path.size()
            || req.uri[loc->path.size()] == '/')) {
        return cfg.root + "/" + req.uri.substr(loc->path.size());
    }
    return cfg.root + req.uri;
}

bool dispatcher::open_static_file_response(const http::request &req,
    const Config &cfg, const std::string &fs_path, std::string &headers,
    int32_t &filefd)
{
    struct stat st;

    filefd = -1;
    headers.clear();
    if (req.method != http::methods::GET)
        return false;
    if (stat(fs_path.c_str(), &st) != 0) {
        if (errno == ENOENT || errno == ENOTDIR)
            headers
                = make_error_response_impl(req, http::status::NOT_FOUND, cfg);
        else if (errno == EACCES)
            headers
                = make_error_response_impl(req, http::status::FORBIDDEN, cfg);
        else if (errno == ENAMETOOLONG)
            headers
                = make_error_response_impl(req, http::status::BAD_REQUEST, cfg);
        else
            headers = make_error_response_impl(
                req, http::status::INTERNAL_SERVER_ERROR, cfg);
        return true;
    }
    if (S_ISDIR(st.st_mode))
        return false;
    if (!S_ISREG(st.st_mode)) {
        headers = make_error_response_impl(req, http::status::FORBIDDEN, cfg);
        return true;
    }
    filefd = open(fs_path.c_str(), O_RDONLY);
    if (filefd == -1) {
        if (errno == EACCES)
            headers
                = make_error_response_impl(req, http::status::FORBIDDEN, cfg);
        else if (errno == ENOENT || errno == ENOTDIR)
            headers
                = make_error_response_impl(req, http::status::NOT_FOUND, cfg);
        else
            headers = make_error_response_impl(
                req, http::status::INTERNAL_SERVER_ERROR, cfg);
        return true;
    }
    fcntl(filefd, F_SETFD, FD_CLOEXEC);
    if (fstat(filefd, &st) != 0) {
        close(filefd);
        filefd = -1;
        headers = make_error_response_impl(
            req, http::status::INTERNAL_SERVER_ERROR, cfg);
        return true;
    }
    if (!S_ISREG(st.st_mode)) {
        close(filefd);
        filefd = -1;
        headers = make_error_response_impl(req, http::status::FORBIDDEN, cfg);
        return true;
    }
    headers = make_response_headers(
        req, http::status::OK, st.st_size, content_type_for(fs_path));
    return true;
}

bool dispatcher::request_body_too_large(
    const http::request &req, const Config &cfg)
{
    return cfg.client_max_body_size != 0
        && req.body.size() > cfg.client_max_body_size;
}

std::string dispatcher::handle(const http::request &req, const Server &server)
{
    const Config &cfg = config_for(req, server);

    if (cfg.redirect_status != 0 && !cfg.redirect_target.empty())
        return make_redirect_response(
            req, cfg.redirect_status, cfg.redirect_target);

    std::string filesystem_path = filesystem_path_for(req, server);

    if (!cfg.allowed_methods[req.method])
        return make_error_response_impl(
            req, http::status::METHOD_NOT_ALLOWED, cfg);
    if (request_body_too_large(req, cfg))
        return make_error_response_impl(
            req, http::status::PAYLOAD_TOO_LARGE, cfg);

    if (req.method == http::methods::POST && !cfg.upload_path.empty()) {
        http::status::type status = upload::save(req, cfg);

        if (status != http::status::CREATED)
            return make_error_response_impl(req, status, cfg);
        return make_response(req, status, "Created\n", "text/plain");
    }
    if (req.method == http::methods::DELETE && !cfg.upload_path.empty()) {
        http::status::type status = upload::remove(req, cfg);

        if (status != http::status::NO_CONTENT)
            return make_error_response_impl(req, status, cfg);
        return make_response(req, status, "", "text/plain");
    }

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
