/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 06:06:28 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/20 09:54:22 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cgi.hpp"

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <csignal>
#include <cstdlib>
#include <map>
#include <sstream>
#include <vector>

#include "config-parser.hpp"

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

bool set_nonblock_cloexec(int32_t fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
        return false;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        return false;
    return fcntl(fd, F_SETFD, FD_CLOEXEC) != -1;
}

void close_if_open(int32_t &fd)
{
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
}

cgi::start::result check_script_path(const std::string &path)
{
    struct stat st;

    if (stat(path.c_str(), &st) != 0) {
        if (errno == ENOENT || errno == ENOTDIR)
            return cgi::start::NOT_FOUND;
        return cgi::start::FORBIDDEN;
    }
    if (!S_ISREG(st.st_mode) || access(path.c_str(), R_OK) != 0)
        return cgi::start::FORBIDDEN;
    return cgi::start::STARTED;
}

bool is_executable_file(const std::string &path)
{
    struct stat st;

    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISREG(st.st_mode) && access(path.c_str(), X_OK) == 0;
}

std::string number_string(std::size_t value)
{
    std::ostringstream ss;

    ss << value;
    return ss.str();
}

std::string directory_name(const std::string &path)
{
    std::size_t slash = path.rfind('/');

    if (slash == std::string::npos)
        return ".";
    if (slash == 0)
        return "/";
    return path.substr(0, slash);
}

std::string path_from_current_directory(const std::string &path)
{
    char cwd[PATH_MAX];

    if (path.empty() || path[0] == '/')
        return path;
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        return "";
    return std::string(cwd) + "/" + path;
}

void add_env(std::vector<std::string> &env, const std::string &name,
    const std::string &value)
{
    env.push_back(name + "=" + value);
}

std::string header_value(const http::request &req, const std::string &name)
{
    std::map<std::string, std::string>::const_iterator it
        = req.headers.find(name);

    if (it == req.headers.end())
        return "";
    return it->second;
}

std::string host_name(const std::string &host)
{
    std::size_t end;
    std::size_t colon;

    if (host.empty())
        return "";
    if (host[0] == '[') {
        end = host.find(']');
        if (end != std::string::npos && end > 1
            && (end + 1 == host.size() || host[end + 1] == ':'))
            return host.substr(1, end - 1);
        return "";
    }
    colon = host.find(':');
    if (colon == 0)
        return "";
    if (colon != std::string::npos) {
        if (host.find(':', colon + 1) != std::string::npos)
            return "";
        return host.substr(0, colon);
    }
    return host;
}

std::string port_from_authority(const std::string &authority)
{
    std::size_t end;
    std::size_t colon;

    if (authority.empty())
        return "";
    if (authority[0] == '[') {
        end = authority.find(']');
        if (end != std::string::npos && end + 1 < authority.size()
            && authority[end + 1] == ':')
            return authority.substr(end + 2);
        return "";
    }
    colon = authority.rfind(':');
    if (colon != std::string::npos)
        return authority.substr(colon + 1);
    return "";
}

std::string first_config_value(const std::vector<std::string> &values)
{
    if (values.empty())
        return "";
    return values[0];
}

std::string server_name(const http::request &req, const Config &cfg)
{
    std::string name = host_name(header_value(req, "host"));

    if (!name.empty())
        return name;
    return first_config_value(cfg.conf.server_name);
}

std::string server_port(const Config &cfg)
{
    std::string port = port_from_authority(first_config_value(cfg.conf.listen));

    if (!port.empty())
        return port;
    port = port_from_authority(DEFAULT_LISTEN);
    if (!port.empty())
        return port;
    return "80";
}

bool excluded_http_header(const std::string &name)
{
    return name == "content-length" || name == "content-type"
        || name == "transfer-encoding" || name == "connection"
        || name == "proxy";
}

bool safe_http_header_name(const std::string &name)
{
    if (name.empty())
        return false;
    for (std::size_t i = 0; i < name.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(name[i]);

        if (!std::isalnum(c) && name[i] != '-')
            return false;
    }
    return true;
}

std::string http_env_name(const std::string &header_name)
{
    std::string name = "HTTP_";

    for (std::size_t i = 0; i < header_name.size(); ++i) {
        if (header_name[i] == '-') {
            name += '_';
        } else {
            name += static_cast<char>(
                std::toupper(static_cast<unsigned char>(header_name[i])));
        }
    }
    return name;
}

std::vector<std::string> make_cgi_environment(
    const http::request &req, const Config &cfg, const std::string &script_path)
{
    std::vector<std::string> env;
    std::string content_length = number_string(req.body.size());
    std::string content_type = header_value(req, "content-type");

    add_env(env, "PATH", "/usr/bin:/bin");
    add_env(env, "GATEWAY_INTERFACE", "CGI/1.1");
    add_env(env, "REQUEST_METHOD", http::methods::strings[req.method]);
    add_env(env, "QUERY_STRING", req.query_string);
    add_env(env, "SCRIPT_FILENAME", script_path);
    add_env(env, "SCRIPT_NAME", req.uri);
    add_env(env, "REMOTE_ADDR", req.remote_addr);
    add_env(env, "SERVER_NAME", server_name(req, cfg));
    add_env(env, "SERVER_PORT", server_port(cfg));
    add_env(env, "SERVER_PROTOCOL", version_string(req));
    add_env(env, "SERVER_SOFTWARE", "webserv");
    add_env(env, "CONTENT_LENGTH", content_length);
    if (!content_type.empty())
        add_env(env, "CONTENT_TYPE", content_type);
    for (std::map<std::string, std::string>::const_iterator it
        = req.headers.begin();
        it != req.headers.end(); ++it) {
        if (!excluded_http_header(it->first)
            && safe_http_header_name(it->first))
            add_env(env, http_env_name(it->first), it->second);
    }
    return env;
}

std::vector<char *> make_envp(std::vector<std::string> &env_values)
{
    std::vector<char *> envp;

    envp.reserve(env_values.size() + 1);
    for (std::size_t i = 0; i < env_values.size(); ++i)
        envp.push_back(const_cast<char *>(env_values[i].c_str()));
    envp.push_back(NULL);
    return envp;
}

void child_exec_cgi(const http::request &req, const Config &cfg,
    const std::string &script_path, int32_t stdin_pipe[2],
    int32_t stdout_pipe[2])
{
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    if (dup2(stdin_pipe[0], STDIN_FILENO) == -1
        || dup2(stdout_pipe[1], STDOUT_FILENO) == -1)
        _exit(127);
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    (void)signal(SIGPIPE, SIG_DFL);
    std::string interpreter_path = path_from_current_directory(cfg.cgi_pass);
    std::string script_filename = path_from_current_directory(script_path);
    if (interpreter_path.empty() || script_filename.empty())
        _exit(127);
    if (chdir(directory_name(script_filename).c_str()) == -1)
        _exit(127);
    std::vector<std::string> env_values
        = make_cgi_environment(req, cfg, script_filename);
    std::vector<char *> envp = make_envp(env_values);
    char *argv[] = { const_cast<char *>(cfg.cgi_pass.c_str()),
        const_cast<char *>(script_filename.c_str()), NULL };
    execve(interpreter_path.c_str(), argv, &envp[0]);
    _exit(127);
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

cgi::start::result cgi::start_process(const http::request &req,
    const Config &cfg, const std::string &script_path, cgi::Process &process)
{
    int32_t stdin_pipe[2] = { -1, -1 };
    int32_t stdout_pipe[2] = { -1, -1 };
    cgi::start::result script_status;

    process.pid = -1;
    process.stdin_fd = -1;
    process.stdout_fd = -1;
    script_status = check_script_path(script_path);
    if (script_status != cgi::start::STARTED)
        return script_status;
    if (cfg.cgi_pass.empty() || !is_executable_file(cfg.cgi_pass))
        return cgi::start::BAD_GATEWAY;
    if (pipe(stdin_pipe) == -1)
        return cgi::start::BAD_GATEWAY;
    if (pipe(stdout_pipe) == -1) {
        close_if_open(stdin_pipe[0]);
        close_if_open(stdin_pipe[1]);
        return cgi::start::BAD_GATEWAY;
    }
    if (!set_nonblock_cloexec(stdin_pipe[1])
        || !set_nonblock_cloexec(stdout_pipe[0])) {
        close_if_open(stdin_pipe[0]);
        close_if_open(stdin_pipe[1]);
        close_if_open(stdout_pipe[0]);
        close_if_open(stdout_pipe[1]);
        return cgi::start::BAD_GATEWAY;
    }

    process.pid = fork();
    if (process.pid == -1) {
        close_if_open(stdin_pipe[0]);
        close_if_open(stdin_pipe[1]);
        close_if_open(stdout_pipe[0]);
        close_if_open(stdout_pipe[1]);
        return cgi::start::BAD_GATEWAY;
    }
    if (process.pid == 0)
        child_exec_cgi(req, cfg, script_path, stdin_pipe, stdout_pipe);

    close_if_open(stdin_pipe[0]);
    close_if_open(stdout_pipe[1]);
    process.stdin_fd = stdin_pipe[1];
    process.stdout_fd = stdout_pipe[0];
    return cgi::start::STARTED;
}
