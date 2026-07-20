/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 06:06:28 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/20 18:03:43 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cgi.hpp"

#include <fcntl.h>
#include <limits.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
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

bool should_filter_cgi_response_header(const std::string &lower_name)
{
    return lower_name == "status" || lower_name == "content-length"
        || lower_name == "connection" || lower_name == "keep-alive"
        || lower_name == "proxy-authenticate"
        || lower_name == "proxy-authorization" || lower_name == "te"
        || lower_name == "trailer" || lower_name == "transfer-encoding"
        || lower_name == "upgrade";
}

bool contains_header_name(
    const std::vector<std::string> &names, const std::string &name)
{
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name)
            return true;
    }
    return false;
}

void add_connection_options(
    std::vector<std::string> &options, const std::string &value)
{
    std::size_t start = 0;

    while (start <= value.size()) {
        std::size_t comma = value.find(',', start);
        std::string option = trim(value.substr(start,
            comma == std::string::npos ? std::string::npos : comma - start));

        if (!option.empty())
            options.push_back(lowercase(option));
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }
}

std::vector<std::string> connection_options(const std::vector<Header> &headers)
{
    std::vector<std::string> options;

    for (std::size_t i = 0; i < headers.size(); ++i) {
        if (headers[i].lower_name == "connection")
            add_connection_options(options, headers[i].value);
    }
    return options;
}

bool should_filter_cgi_response_header(const std::string &lower_name,
    const std::vector<std::string> &connection_header_options)
{
    return should_filter_cgi_response_header(lower_name)
        || contains_header_name(connection_header_options, lower_name);
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

uint64_t monotonic_millis()
{
    timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
        return 0;
    return (static_cast<uint64_t>(ts.tv_sec) * 1000)
        + static_cast<uint64_t>(ts.tv_nsec / 1000000);
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

void append_forwarded_cgi_headers(
    std::ostringstream &ss, const std::vector<Header> &headers)
{
    std::vector<std::string> connection_header_options
        = connection_options(headers);

    for (std::size_t i = 0; i < headers.size(); ++i) {
        if (should_filter_cgi_response_header(
                headers[i].lower_name, connection_header_options))
            continue;
        ss << headers[i].name << ": " << headers[i].value << "\r\n";
    }
}

void append_server_framing_and_body(
    std::ostringstream &ss, const http::request &req, const std::string &body)
{
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Connection: " << (req.keep_alive ? "keep-alive" : "close") << "\r\n";
    ss << "\r\n";
    if (req.method != http::methods::HEAD)
        ss << body;
}

std::string make_response(const http::request &req, int code,
    const std::string &reason, const std::vector<Header> &headers,
    const std::string &body)
{
    std::ostringstream ss;

    ss << version_string(req) << " " << code << " " << reason << "\r\n";
    append_forwarded_cgi_headers(ss, headers);
    append_server_framing_and_body(ss, req, body);
    return ss.str();
}

std::string make_nph_response(const std::string &first_line,
    const std::vector<Header> &headers, const std::string &body,
    const http::request &req)
{
    std::ostringstream ss;

    ss << first_line << "\r\n";
    append_forwarded_cgi_headers(ss, headers);
    append_server_framing_and_body(ss, req, body);
    return ss.str();
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
    return make_nph_response(first_line, headers,
        output.substr(header_end.pos + header_end.len), req);
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
        return make_bad_gateway(req);
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
        return make_bad_gateway(req);
    return translate_parsed(output, req, header_end);
}

cgi::Descriptor::Descriptor()
    : type(cgi::descriptor::CGI_STDIN)
    , fd(-1)
{
}

cgi::Descriptor::Descriptor(
    cgi::descriptor::type descriptor_type, int32_t descriptor_fd)
    : type(descriptor_type)
    , fd(descriptor_fd)
{
}

cgi::Job::Job()
    : clientfd(-1)
    , pid(-1)
    , stdin_fd(-1)
    , stdout_fd(-1)
    , body_written(0)
    , max_output(0)
    , deadline_millis(0)
    , request()
    , failure_status(http::status::BAD_GATEWAY)
    , failed(false)
{
}

cgi::ReadinessResult::ReadinessResult()
    : action(cgi::readiness::CONTINUE)
    , descriptor_fd(-1)
{
}

cgi::CompletionResult::CompletionResult()
    : clientfd(-1)
    , request()
    , failure_status(http::status::BAD_GATEWAY)
    , failed(false)
{
}

cgi::StartedRequest::StartedRequest()
    : status(cgi::start::BAD_GATEWAY)
{
}

cgi::start::result cgi::Lifecycle::start_request(int32_t clientfd,
    const http::request &req, const Config &cfg, const std::string &script_path,
    cgi::StartedRequest &request)
{
    cgi::Process process;

    request = cgi::StartedRequest();
    request.status = cgi::start_process(req, cfg, script_path, process);
    if (request.status != cgi::start::STARTED)
        return request.status;
    request.job.clientfd = clientfd;
    request.job.pid = process.pid;
    request.job.stdin_fd = process.stdin_fd;
    request.job.stdout_fd = process.stdout_fd;
    request.job.max_output = cfg.cgi_output_buffer_size;
    request.job.deadline_millis
        = monotonic_millis() + (static_cast<uint64_t>(cfg.cgi_timeout) * 1000);
    request.job.request = req;
    request.descriptors.push_back(
        cgi::Descriptor(cgi::descriptor::CGI_STDOUT, process.stdout_fd));
    request.descriptors.push_back(
        cgi::Descriptor(cgi::descriptor::CGI_STDIN, process.stdin_fd));
    return request.status;
}

cgi::ReadinessResult cgi::Lifecycle::process_stdin(
    cgi::Job &job, uint32_t events)
{
    cgi::ReadinessResult result;
    const std::string &body = job.request.body;

    result.descriptor_fd = job.stdin_fd;
    if (events & EPOLLERR)
        job.failed = true;
    if (job.body_written < body.size()) {
        ssize_t n = write(job.stdin_fd, body.c_str() + job.body_written,
            body.size() - job.body_written);

        if (n > 0)
            job.body_written += static_cast<std::size_t>(n);
        else if (!(events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)))
            return result;
    }
    if (job.body_written < body.size() && !job.failed)
        return result;
    result.action = cgi::readiness::CLOSE_STDIN;
    return result;
}

cgi::ReadinessResult cgi::Lifecycle::process_stdout(
    cgi::Job &job, uint32_t events)
{
    cgi::ReadinessResult result;
    char buffer[4096];
    ssize_t bytes_read;

    result.descriptor_fd = job.stdout_fd;
    bytes_read = read(job.stdout_fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        if (job.max_output != 0
            && job.output.size() + static_cast<std::size_t>(bytes_read)
                > job.max_output) {
            job.failed = true;
            job.failure_status = http::status::BAD_GATEWAY;
            result.action = cgi::readiness::COMPLETE;
        } else {
            job.output.append(buffer, static_cast<std::size_t>(bytes_read));
        }
    } else if (bytes_read == 0) {
        result.action = cgi::readiness::COMPLETE;
    } else if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        if (events & EPOLLERR)
            job.failed = true;
        result.action = cgi::readiness::COMPLETE;
    }
    return result;
}

cgi::CompletionResult cgi::Lifecycle::complete(cgi::Job job, bool child_ok)
{
    cgi::CompletionResult result;

    if (job.stdin_fd != -1 && job.body_written < job.request.body.size())
        job.failed = true;
    result.clientfd = job.clientfd;
    result.request = job.request;
    result.output = job.output;
    result.failure_status = job.failure_status;
    result.failed = job.failed || !child_ok;
    return result;
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
