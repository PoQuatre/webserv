/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   event_loop.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 19:23:48 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/20 10:31:39 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "EventLoop.hpp"
#include "logger.hpp"

struct LoopThreadArgs {
    EventLoop *loop;
    bool result;
};

static void *run_loop(void *data)
{
    LoopThreadArgs *args = static_cast<LoopThreadArgs *>(data);

    args->result = args->loop->run();
    return NULL;
}

static uint16_t reserve_loopback_port()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    cr_assert_neq(fd, -1, "socket() failed: %s", strerror(errno));

    sockaddr_in addr = { };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    cr_assert_eq(bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), 0,
        "bind() failed: %s", strerror(errno));

    socklen_t addrlen = sizeof(addr);
    cr_assert_eq(getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &addrlen),
        0, "getsockname() failed: %s", strerror(errno));
    close(fd);
    return ntohs(addr.sin_port);
}

static int connect_to_loopback(uint16_t port)
{
    for (int attempts = 0; attempts < 500; ++attempts) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        cr_assert_neq(fd, -1, "socket() failed: %s", strerror(errno));
        cr_assert_neq(fcntl(fd, F_SETFD, FD_CLOEXEC), -1, "fcntl() failed: %s",
            strerror(errno));

        sockaddr_in addr = { };
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);
        if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0)
            return fd;

        close(fd);
        usleep(1000);
    }

    cr_assert_fail("event loop listener did not accept connections");
    return -1;
}

static void write_all(int fd, const std::string &data)
{
    std::size_t written = 0;

    while (written < data.size()) {
        ssize_t n = write(fd, data.c_str() + written, data.size() - written);
        cr_assert_gt(n, 0, "write() failed: %s", strerror(errno));
        written += static_cast<std::size_t>(n);
    }
}

static std::string read_response(int fd)
{
    timeval timeout = { };
    timeout.tv_sec = 2;
    cr_assert_eq(
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)), 0,
        "setsockopt() failed: %s", strerror(errno));

    std::string response;
    char buffer[256];
    ssize_t n = read(fd, buffer, sizeof(buffer));
    cr_assert_gt(n, 0, "read() failed: %s", strerror(errno));
    response.append(buffer, static_cast<std::size_t>(n));
    return response;
}

static std::string read_response_until_idle(int fd)
{
    timeval timeout = { };
    timeout.tv_usec = 200000;
    cr_assert_eq(
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)), 0,
        "setsockopt() failed: %s", strerror(errno));

    std::string response;
    char buffer[512];
    while (true) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n > 0) {
            response.append(buffer, static_cast<std::size_t>(n));
        } else if (n == 0 || errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        } else {
            cr_assert_fail("read() failed: %s", strerror(errno));
        }
    }
    cr_assert(!response.empty(), "empty response");
    return response;
}

static std::string read_response_with_timeout(int fd, long usec)
{
    timeval timeout = { };
    timeout.tv_usec = usec;
    cr_assert_eq(
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)), 0,
        "setsockopt() failed: %s", strerror(errno));

    std::string response;
    char buffer[256];
    ssize_t n = read(fd, buffer, sizeof(buffer));
    cr_assert_gt(n, 0, "read() failed: %s", strerror(errno));
    response.append(buffer, static_cast<std::size_t>(n));
    return response;
}

static std::string make_tmpdir()
{
    char tmpl[] = "/tmp/webserv-cgi-test-XXXXXX";
    char *dir = mkdtemp(tmpl);

    cr_assert_not_null(dir, "mkdtemp() failed: %s", strerror(errno));
    return dir;
}

static std::string make_cgi_root()
{
    std::string root = make_tmpdir();

    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    return root;
}

static void write_file(const std::string &path, const std::string &content)
{
    std::ofstream out(path.c_str(), std::ios::binary);

    cr_assert(out.is_open(), "failed to open %s", path.c_str());
    out << content;
    cr_assert(!out.fail(), "failed to write %s", path.c_str());
}

static pid_t wait_for_pid_file(const std::string &path)
{
    for (int attempts = 0; attempts < 1000; ++attempts) {
        std::ifstream in(path.c_str());
        long pid = 0;

        if (in >> pid && pid > 0)
            return static_cast<pid_t>(pid);
        usleep(1000);
    }
    cr_assert_fail("timed out waiting for pid file %s", path.c_str());
    return -1;
}

static bool process_exists(pid_t pid)
{
    errno = 0;
    if (kill(pid, 0) == 0)
        return true;
    cr_assert_eq(errno, ESRCH, "kill(%ld, 0) failed: %s",
        static_cast<long>(pid), strerror(errno));
    return false;
}

static bool wait_process_gone(pid_t pid)
{
    for (int attempts = 0; attempts < 1000; ++attempts) {
        if (!process_exists(pid))
            return true;
        usleep(1000);
    }
    return false;
}

static Server make_cgi_server(uint16_t port, const std::string &root,
    bool allow_delete = false, const std::string &cgi_pass = "/bin/sh",
    uint32_t cgi_timeout = DEFAULT_CGI_TIMEOUT,
    std::size_t cgi_output_buffer_size = DEFAULT_CGI_OUTPUT_BUFFER_SIZE)
{
    std::ostringstream listen_addr;
    Config config = { };
    Config cgi_config = { };
    Location cgi_location;
    std::vector<Location> locations;

    listen_addr << "127.0.0.1:" << port;
    config.conf.listen.push_back(listen_addr.str());
    config.conf.server_name.push_back("test");
    config.root = root;
    config.allowed_methods[http::methods::GET] = true;
    config.allowed_methods[http::methods::POST] = true;
    config.allowed_methods[http::methods::HEAD] = true;
    config.allowed_methods[http::methods::DELETE] = true;

    cgi_config = config;
    cgi_config.allowed_methods[http::methods::DELETE] = allow_delete;
    cgi_config.cgi_enabled = true;
    cgi_config.cgi_pass = cgi_pass;
    cgi_config.cgi_timeout = cgi_timeout;
    cgi_config.cgi_output_buffer_size = cgi_output_buffer_size;

    cgi_location.path = "/cgi";
    cgi_location.config = cgi_config;
    cgi_location.type = location::CLASSIC;
    locations.push_back(cgi_location);

    return Server(locations, "test", listen_addr.str(), config);
}

struct CgiHarnessOptions {
    CgiHarnessOptions()
        : allow_delete(false)
        , cgi_pass("/bin/sh")
        , cgi_timeout(DEFAULT_CGI_TIMEOUT)
        , cgi_output_buffer_size(DEFAULT_CGI_OUTPUT_BUFFER_SIZE)
    {
    }

    bool allow_delete;
    std::string cgi_pass;
    uint32_t cgi_timeout;
    std::size_t cgi_output_buffer_size;
};

struct CgiHarness {
    CgiHarness()
        : root(make_cgi_root())
        , port(0)
        , loop(NULL)
        , running(false)
    {
        args.loop = NULL;
        args.result = false;
    }
    ~CgiHarness();

    std::string root;
    uint16_t port;
    std::vector<Server> servers;
    EventLoop *loop;
    LoopThreadArgs args;
    pthread_t thread;
    bool running;
    CgiHarnessOptions options;
};

static void start_cgi_harness(CgiHarness &harness)
{
    cr_assert(!harness.running, "CGI harness is already running");
    harness.port = reserve_loopback_port();
    harness.servers.push_back(make_cgi_server(harness.port, harness.root,
        harness.options.allow_delete, harness.options.cgi_pass,
        harness.options.cgi_timeout, harness.options.cgi_output_buffer_size));
    harness.loop = new EventLoop(harness.servers);
    harness.args.loop = harness.loop;
    harness.args.result = false;
    cr_assert_eq(
        pthread_create(&harness.thread, NULL, &run_loop, &harness.args), 0,
        "pthread_create() failed");
    harness.running = true;
}

static void ensure_cgi_harness_started(CgiHarness &harness)
{
    if (!harness.running)
        start_cgi_harness(harness);
}

static void stop_cgi_harness(CgiHarness &harness)
{
    if (!harness.running)
        return;
    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(
        pthread_join(harness.thread, NULL), 0, "pthread_join() failed");
    cr_assert(harness.args.result);
    delete harness.loop;
    harness.loop = NULL;
    harness.args.loop = NULL;
    harness.running = false;
}

CgiHarness::~CgiHarness() { stop_cgi_harness(*this); }

static int send_request(CgiHarness &harness, const std::string &request)
{
    ensure_cgi_harness_started(harness);
    int fd = connect_to_loopback(harness.port);

    write_all(fd, request);
    return fd;
}

static std::string perform_request(
    CgiHarness &harness, const std::string &request)
{
    int fd = send_request(harness, request);
    std::string response = read_response(fd);

    close(fd);
    return response;
}

static std::string perform_request_until_idle(
    CgiHarness &harness, const std::string &request)
{
    int fd = send_request(harness, request);
    std::string response = read_response_until_idle(fd);

    close(fd);
    return response;
}

static std::string perform_request_with_timeout(
    CgiHarness &harness, const std::string &request, long usec)
{
    int fd = send_request(harness, request);
    std::string response = read_response_with_timeout(fd, usec);

    close(fd);
    return response;
}

static void assert_status(const std::string &response, const char *status)
{
    cr_assert_neq(response.find(status), std::string::npos,
        "missing status '%s' in response:\n%s", status, response.c_str());
}

Test(event_loop, listener_and_signal_sources_dispatch_readiness)
{
    logger::log_level() = logger::levels::NOTHING;

    uint16_t port = reserve_loopback_port();
    std::ostringstream listen_addr;
    listen_addr << "127.0.0.1:" << port;

    Config config = { };
    config.allowed_methods[http::methods::GET] = true;
    std::vector<Server> servers;
    servers.push_back(
        Server(std::vector<Location>(), "test", listen_addr.str(), config));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int clientfd = connect_to_loopback(port);
    write_all(clientfd, "GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n");

    std::string response = read_response(clientfd);
    cr_assert_neq(response.find("404 Not Found"), std::string::npos);

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(clientfd);

    cr_assert(args.result);
}

Test(event_loop, cgi_get_executes_script_and_static_requests_still_work)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    write_file(harness.root + "/cgi/hello.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'method=%s query=%s\\n' \"$REQUEST_METHOD\" "
        "\"$QUERY_STRING\"\n");
    write_file(harness.root + "/cgi/slow.sh", "sleep 1\nprintf 'slow\\n'\n");
    write_file(harness.root + "/static.txt", "static body\n");

    std::string cgi_response = perform_request(harness,
        "GET /cgi/hello.sh?name=webserv HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert_status(cgi_response, "HTTP/1.1 200 OK");
    cr_assert_neq(
        cgi_response.find("Content-Type: text/plain"), std::string::npos);
    cr_assert_neq(cgi_response.find("method=GET query=name=webserv\n"),
        std::string::npos);

    int slow_fd = send_request(
        harness, "GET /cgi/slow.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string static_response = perform_request_with_timeout(
        harness, "GET /static.txt HTTP/1.1\r\nHost: localhost\r\n\r\n", 200000);
    assert_status(static_response, "HTTP/1.1 200 OK");
    cr_assert_neq(static_response.find("static body\n"), std::string::npos);

    close(slow_fd);
}

Test(event_loop, cgi_receives_post_and_chunked_bodies_on_stdin)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    write_file(harness.root + "/cgi/echo.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "body=$(dd bs=1 count=\"${CONTENT_LENGTH:-0}\" 2>/dev/null)\n"
        "extra=$(dd bs=1 count=1 2>/dev/null)\n"
        "printf 'method=%s\\n' \"$REQUEST_METHOD\"\n"
        "printf 'content_length=%s\\n' \"$CONTENT_LENGTH\"\n"
        "printf 'content_type=%s\\n' \"$CONTENT_TYPE\"\n"
        "printf 'body=%s\\n' \"$body\"\n"
        "if [ -z \"$extra\" ]; then printf 'stdin_eof=yes\\n'; fi\n");

    std::string post_response = perform_request(harness,
        "POST /cgi/echo.sh HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 18\r\n"
        "\r\n"
        "alpha=one&beta=two");
    assert_status(post_response, "HTTP/1.1 200 OK");
    cr_assert_neq(post_response.find("method=POST\n"), std::string::npos);
    cr_assert_neq(post_response.find("content_length=18\n"), std::string::npos);
    cr_assert_neq(
        post_response.find("content_type=application/x-www-form-urlencoded\n"),
        std::string::npos);
    cr_assert_neq(
        post_response.find("body=alpha=one&beta=two\n"), std::string::npos);
    cr_assert_neq(post_response.find("stdin_eof=yes\n"), std::string::npos);

    std::string chunked_response = perform_request(harness,
        "POST /cgi/echo.sh HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 999\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n");
    assert_status(chunked_response, "HTTP/1.1 200 OK");
    cr_assert_neq(
        chunked_response.find("content_length=9\n"), std::string::npos);
    cr_assert_neq(
        chunked_response.find("content_type=text/plain\n"), std::string::npos);
    cr_assert_neq(chunked_response.find("body=Wikipedia\n"), std::string::npos);
    cr_assert_neq(chunked_response.find("stdin_eof=yes\n"), std::string::npos);
}

Test(event_loop, cgi_exposes_raw_query_meta_variables_and_http_headers)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    write_file(harness.root + "/cgi/env.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'REQUEST_METHOD=%s\\n' \"$REQUEST_METHOD\"\n"
        "printf 'QUERY_STRING=%s\\n' \"$QUERY_STRING\"\n"
        "printf 'SCRIPT_NAME=%s\\n' \"$SCRIPT_NAME\"\n"
        "printf 'SCRIPT_FILENAME=%s\\n' \"$SCRIPT_FILENAME\"\n"
        "printf 'REMOTE_ADDR=%s\\n' \"$REMOTE_ADDR\"\n"
        "printf 'SERVER_NAME=%s\\n' \"$SERVER_NAME\"\n"
        "printf 'SERVER_PORT=%s\\n' \"$SERVER_PORT\"\n"
        "printf 'SERVER_PROTOCOL=%s\\n' \"$SERVER_PROTOCOL\"\n"
        "printf 'CONTENT_LENGTH=%s\\n' \"$CONTENT_LENGTH\"\n"
        "printf 'CONTENT_TYPE=%s\\n' \"$CONTENT_TYPE\"\n"
        "printf 'HTTP_ACCEPT=%s\\n' \"$HTTP_ACCEPT\"\n"
        "printf 'HTTP_HOST=%s\\n' \"$HTTP_HOST\"\n"
        "printf 'HTTP_X_CUSTOM_HEADER=%s\\n' \"$HTTP_X_CUSTOM_HEADER\"\n");

    const std::string request_host = "example.test:1";
    std::ostringstream request;
    request << "GET /cgi/env.sh?raw=a%2Bb+c HTTP/1.1\r\n"
            << "Host: " << request_host << "\r\n"
            << "Accept: text/plain\r\n"
            << "Content-Type: text/plain\r\n"
            << "Content-Length: 0\r\n"
            << "X-Custom-Header: kept\r\n"
            << "\r\n";
    std::string response = perform_request_until_idle(harness, request.str());
    assert_status(response, "HTTP/1.1 200 OK");
    cr_assert_neq(response.find("REQUEST_METHOD=GET\n"), std::string::npos);
    cr_assert_neq(
        response.find("QUERY_STRING=raw=a%2Bb+c\n"), std::string::npos);
    cr_assert_neq(
        response.find("SCRIPT_NAME=/cgi/env.sh\n"), std::string::npos);
    std::string script_filename
        = "SCRIPT_FILENAME=" + harness.root + "/cgi/env.sh\n";
    cr_assert_neq(response.find(script_filename), std::string::npos);
    cr_assert_neq(response.find("REMOTE_ADDR=127.0.0.1\n"), std::string::npos);
    cr_assert_neq(
        response.find("SERVER_NAME=example.test\n"), std::string::npos);
    std::ostringstream server_port;
    server_port << "SERVER_PORT=" << harness.port << "\n";
    cr_assert_neq(response.find(server_port.str()), std::string::npos);
    cr_assert_neq(
        response.find("SERVER_PROTOCOL=HTTP/1.1\n"), std::string::npos);
    cr_assert_neq(response.find("CONTENT_LENGTH=0\n"), std::string::npos);
    cr_assert_neq(
        response.find("CONTENT_TYPE=text/plain\n"), std::string::npos);
    cr_assert_neq(response.find("HTTP_ACCEPT=text/plain\n"), std::string::npos);
    std::string http_host = "HTTP_HOST=" + request_host + "\n";
    cr_assert_neq(response.find(http_host), std::string::npos);
    cr_assert_neq(
        response.find("HTTP_X_CUSTOM_HEADER=kept\n"), std::string::npos);

    std::string fallback_response = perform_request_until_idle(harness,
        "GET /cgi/env.sh HTTP/1.0\r\n"
        "Accept: text/plain\r\n"
        "\r\n");
    assert_status(fallback_response, "HTTP/1.0 200 OK");
    cr_assert_neq(
        fallback_response.find("SERVER_NAME=test\n"), std::string::npos);
    cr_assert_neq(fallback_response.find(server_port.str()), std::string::npos);

    std::string unusable_host_response = perform_request_until_idle(harness,
        "GET /cgi/env.sh HTTP/1.1\r\n"
        "Host: 2001:db8::1\r\n"
        "\r\n");
    assert_status(unusable_host_response, "HTTP/1.1 200 OK");
    cr_assert_neq(
        unusable_host_response.find("SERVER_NAME=test\n"), std::string::npos);
    cr_assert_neq(
        unusable_host_response.find(server_port.str()), std::string::npos);
}

Test(event_loop, cgi_response_modes_cover_redirect_nph_and_missing_separator)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    write_file(harness.root + "/cgi/redirect.sh",
        "printf 'Location: /elsewhere\\r\\n\\r\\n'\n");
    write_file(harness.root + "/cgi/nph-output.sh",
        "printf 'HTTP/1.1 204 No Content\\r\\n'\n"
        "printf 'X-NPH: yes\\r\\n'\n"
        "printf 'Connection: close\\r\\n'\n"
        "printf 'Transfer-Encoding: chunked\\r\\n'\n"
        "printf '\\r\\n'\n");
    write_file(harness.root + "/cgi/missing-separator.sh",
        "printf 'missing-separator\\n'\n");

    std::string redirect_response = perform_request_until_idle(
        harness, "GET /cgi/redirect.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert_status(redirect_response, "HTTP/1.1 302 Moved Temporarily");
    cr_assert_neq(
        redirect_response.find("Location: /elsewhere\r\n"), std::string::npos);

    std::string nph_response = perform_request_until_idle(
        harness, "GET /cgi/nph-output.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    cr_assert_eq(
        nph_response.find("HTTP/1.1 204 No Content\r\n"), std::size_t(0));
    cr_assert_neq(nph_response.find("X-NPH: yes\r\n"), std::string::npos);
    cr_assert_neq(
        nph_response.find("Content-Length: 0\r\n"), std::string::npos);
    cr_assert_neq(
        nph_response.find("Connection: keep-alive\r\n"), std::string::npos);
    cr_assert_eq(nph_response.find("Connection: close\r\n"), std::string::npos);
    cr_assert_eq(
        nph_response.find("Transfer-Encoding: chunked\r\n"), std::string::npos);

    std::string missing_separator_response = perform_request_until_idle(harness,
        "GET /cgi/missing-separator.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert_status(missing_separator_response, "HTTP/1.1 502 Bad Gateway");
    cr_assert_eq(missing_separator_response.find("missing-separator\n"),
        std::string::npos);
}

Test(event_loop, cgi_response_filters_unsafe_headers)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    write_file(harness.root + "/cgi/headers.sh",
        "printf 'Status: 201 Created\\r\\n'\n"
        "printf 'Content-Type: text/plain\\r\\n'\n"
        "printf 'Set-Cookie: session=abc\\r\\n'\n"
        "printf 'X-App-Header: kept\\r\\n'\n"
        "printf 'Content-Length: 999\\r\\n'\n"
        "printf 'Connection: close, X-Hop\\r\\n'\n"
        "printf 'X-Hop: dropped\\r\\n'\n"
        "printf 'Transfer-Encoding: chunked\\r\\n'\n"
        "printf '\\r\\nbody\\n'\n");

    std::string response = perform_request_until_idle(
        harness, "GET /cgi/headers.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");

    assert_status(response, "HTTP/1.1 201 Created");
    cr_assert_neq(
        response.find("Content-Type: text/plain\r\n"), std::string::npos);
    cr_assert_neq(
        response.find("Set-Cookie: session=abc\r\n"), std::string::npos);
    cr_assert_neq(response.find("X-App-Header: kept\r\n"), std::string::npos);
    cr_assert_neq(response.find("Content-Length: 5\r\n"), std::string::npos);
    cr_assert_eq(response.find("Status:"), std::string::npos);
    cr_assert_eq(response.find("Content-Length: 999\r\n"), std::string::npos);
    cr_assert_eq(
        response.find("Connection: close, X-Hop\r\n"), std::string::npos);
    cr_assert_eq(response.find("X-Hop: dropped\r\n"), std::string::npos);
    cr_assert_eq(
        response.find("Transfer-Encoding: chunked\r\n"), std::string::npos);
    cr_assert_neq(response.find("body\n"), std::string::npos);
}

Test(event_loop, cgi_executes_allowed_delete_method)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    write_file(harness.root + "/cgi/method.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'method=%s\\n' \"$REQUEST_METHOD\"\n");

    harness.options.allow_delete = true;

    std::string delete_response = perform_request(
        harness, "DELETE /cgi/method.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert_status(delete_response, "HTTP/1.1 200 OK");
    cr_assert_neq(delete_response.find("method=DELETE\n"), std::string::npos);
}

Test(event_loop, cgi_rejects_disallowed_method_without_running_script)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    std::string marker = harness.root + "/ran-delete";
    write_file(harness.root + "/cgi/method.sh",
        "printf marker > '" + marker
            + "'\n"
              "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
              "printf 'method=%s\\n' \"$REQUEST_METHOD\"\n");

    std::string delete_response = perform_request(
        harness, "DELETE /cgi/method.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert_status(delete_response, "HTTP/1.1 405 Method Not Allowed");
    cr_assert_eq(access(marker.c_str(), F_OK), -1,
        "disallowed CGI method executed script and created %s", marker.c_str());
    cr_assert_eq(errno, ENOENT, "access() failed: %s", strerror(errno));
}

Test(event_loop, cgi_head_executes_script_and_discards_response_body)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    std::string marker = harness.root + "/ran-head";
    write_file(harness.root + "/cgi/head.sh",
        "printf marker > '" + marker
            + "'\n"
              "printf 'Content-Type: text/plain\r\n\r\n'\n"
              "printf 'method=%s\n' \"$REQUEST_METHOD\"\n");

    std::string head_response = perform_request(
        harness, "HEAD /cgi/head.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert_status(head_response, "HTTP/1.1 200 OK");
    cr_assert_neq(
        head_response.find("Content-Length: 12\r\n"), std::string::npos);
    cr_assert_eq(head_response.find("method=HEAD\n"), std::string::npos,
        "HEAD response leaked CGI body:\n%s", head_response.c_str());
    cr_assert_eq(access(marker.c_str(), F_OK), 0,
        "HEAD did not execute CGI script: %s", strerror(errno));
}

Test(event_loop, cgi_script_path_errors_map_to_http_statuses)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    write_file(harness.root + "/cgi/unreadable.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\nnope\\n'\n");
    cr_assert_eq(chmod((harness.root + "/cgi/unreadable.sh").c_str(), 0000), 0,
        "chmod() failed: %s", strerror(errno));
    cr_assert_eq(mkdir((harness.root + "/cgi/directory").c_str(), 0700), 0,
        "mkdir() failed: %s", strerror(errno));

    assert_status(
        perform_request(
            harness, "GET /cgi/missing.sh HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        "HTTP/1.1 404 Not Found");
    assert_status(
        perform_request(harness,
            "GET /cgi/unreadable.sh HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        "HTTP/1.1 403 Forbidden");
    assert_status(perform_request(harness,
                      "GET /cgi/directory HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        "HTTP/1.1 403 Forbidden");
}

Test(event_loop, cgi_invalid_interpreter_maps_to_bad_gateway)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    write_file(harness.root + "/cgi/hello.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\nhello\\n'\n");
    write_file(harness.root + "/not-executable", "not an interpreter\n");
    cr_assert_eq(chmod((harness.root + "/not-executable").c_str(), 0600), 0,
        "chmod() failed: %s", strerror(errno));

    harness.options.cgi_pass = harness.root + "/not-executable";

    assert_status(perform_request(harness,
                      "GET /cgi/hello.sh HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        "HTTP/1.1 502 Bad Gateway");
}

Test(event_loop, cgi_timeout_returns_gateway_timeout)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    write_file(harness.root + "/cgi/slow.sh",
        "sleep 5\n"
        "printf 'Content-Type: text/plain\\r\\n\\r\\nlate\\n'\n");

    harness.options.cgi_timeout = 1;

    assert_status(perform_request(harness,
                      "GET /cgi/slow.sh HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        "HTTP/1.1 504 Gateway Timeout");
}

Test(event_loop, cgi_output_cap_returns_bad_gateway)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    write_file(harness.root + "/cgi/large.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'this output is too large for the configured cap\\n'\n");

    harness.options.cgi_output_buffer_size = 32;

    assert_status(perform_request(harness,
                      "GET /cgi/large.sh HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        "HTTP/1.1 502 Bad Gateway");
}

Test(event_loop, disconnect_while_cgi_runs_cleans_up_child_and_keeps_clients)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    std::string pidfile = harness.root + "/cgi.pid";
    write_file(harness.root + "/cgi/linger.sh",
        "printf '%s\\n' \"$$\" > '" + pidfile
            + "'\n"
              "printf 'Content-Type: text/plain\\r\\n\\r\\npartial output\\n'\n"
              "while :; do :; done\n");
    write_file(harness.root + "/static.txt", "static still works\n");

    int cgi_fd = send_request(
        harness, "GET /cgi/linger.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    pid_t cgi_pid = wait_for_pid_file(pidfile);

    int static_fd = connect_to_loopback(harness.port);
    close(cgi_fd);
    write_all(static_fd, "GET /static.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string static_response = read_response(static_fd);
    close(static_fd);
    assert_status(static_response, "HTTP/1.1 200 OK");
    cr_assert_neq(
        static_response.find("static still works\n"), std::string::npos);
    bool cgi_gone = wait_process_gone(cgi_pid);

    cr_assert(cgi_gone, "process %ld still exists", static_cast<long>(cgi_pid));
}
