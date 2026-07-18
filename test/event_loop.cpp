/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   event_loop.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 19:23:48 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/18 23:04:15 by mle-flem         ###   ########.fr       */
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

    std::string root = make_tmpdir();
    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    write_file(root + "/cgi/hello.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'method=%s query=%s\\n' \"$REQUEST_METHOD\" "
        "\"$QUERY_STRING\"\n");
    write_file(root + "/cgi/slow.sh", "sleep 1\nprintf 'slow\\n'\n");
    write_file(root + "/static.txt", "static body\n");

    uint16_t port = reserve_loopback_port();
    std::vector<Server> servers;
    servers.push_back(make_cgi_server(port, root));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int cgi_fd = connect_to_loopback(port);
    write_all(cgi_fd,
        "GET /cgi/hello.sh?name=webserv HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string cgi_response = read_response(cgi_fd);
    cr_assert_neq(cgi_response.find("HTTP/1.1 200 OK"), std::string::npos);
    cr_assert_neq(
        cgi_response.find("Content-Type: text/plain"), std::string::npos);
    cr_assert_neq(cgi_response.find("method=GET query=name=webserv\n"),
        std::string::npos);

    int slow_fd = connect_to_loopback(port);
    write_all(slow_fd, "GET /cgi/slow.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    int static_fd = connect_to_loopback(port);
    write_all(static_fd, "GET /static.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string static_response = read_response_with_timeout(static_fd, 200000);
    cr_assert_neq(static_response.find("HTTP/1.1 200 OK"), std::string::npos);
    cr_assert_neq(static_response.find("static body\n"), std::string::npos);

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(cgi_fd);
    close(slow_fd);
    close(static_fd);

    cr_assert(args.result);
}

Test(event_loop, cgi_receives_post_and_chunked_bodies_on_stdin)
{
    logger::log_level() = logger::levels::NOTHING;

    std::string root = make_tmpdir();
    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    write_file(root + "/cgi/echo.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "body=$(dd bs=1 count=\"${CONTENT_LENGTH:-0}\" 2>/dev/null)\n"
        "extra=$(dd bs=1 count=1 2>/dev/null)\n"
        "printf 'method=%s\\n' \"$REQUEST_METHOD\"\n"
        "printf 'content_length=%s\\n' \"$CONTENT_LENGTH\"\n"
        "printf 'content_type=%s\\n' \"$CONTENT_TYPE\"\n"
        "printf 'body=%s\\n' \"$body\"\n"
        "if [ -z \"$extra\" ]; then printf 'stdin_eof=yes\\n'; fi\n");

    uint16_t port = reserve_loopback_port();
    std::vector<Server> servers;
    servers.push_back(make_cgi_server(port, root));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int post_fd = connect_to_loopback(port);
    write_all(post_fd,
        "POST /cgi/echo.sh HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 18\r\n"
        "\r\n"
        "alpha=one&beta=two");
    std::string post_response = read_response(post_fd);
    cr_assert_neq(post_response.find("HTTP/1.1 200 OK"), std::string::npos);
    cr_assert_neq(post_response.find("method=POST\n"), std::string::npos);
    cr_assert_neq(post_response.find("content_length=18\n"), std::string::npos);
    cr_assert_neq(
        post_response.find("content_type=application/x-www-form-urlencoded\n"),
        std::string::npos);
    cr_assert_neq(
        post_response.find("body=alpha=one&beta=two\n"), std::string::npos);
    cr_assert_neq(post_response.find("stdin_eof=yes\n"), std::string::npos);

    int chunked_fd = connect_to_loopback(port);
    write_all(chunked_fd,
        "POST /cgi/echo.sh HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 999\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n");
    std::string chunked_response = read_response(chunked_fd);
    cr_assert_neq(chunked_response.find("HTTP/1.1 200 OK"), std::string::npos);
    cr_assert_neq(
        chunked_response.find("content_length=9\n"), std::string::npos);
    cr_assert_neq(
        chunked_response.find("content_type=text/plain\n"), std::string::npos);
    cr_assert_neq(chunked_response.find("body=Wikipedia\n"), std::string::npos);
    cr_assert_neq(chunked_response.find("stdin_eof=yes\n"), std::string::npos);

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(post_fd);
    close(chunked_fd);

    cr_assert(args.result);
}

Test(event_loop, cgi_exposes_raw_query_meta_variables_and_http_headers)
{
    logger::log_level() = logger::levels::NOTHING;

    std::string root = make_tmpdir();
    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    write_file(root + "/cgi/env.sh",
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

    uint16_t port = reserve_loopback_port();
    std::vector<Server> servers;
    servers.push_back(make_cgi_server(port, root));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    std::ostringstream request;
    request << "GET /cgi/env.sh?raw=a%2Bb+c HTTP/1.1\r\n"
            << "Host: example.test:" << port << "\r\n"
            << "Accept: text/plain\r\n"
            << "Content-Type: text/plain\r\n"
            << "Content-Length: 0\r\n"
            << "X-Custom-Header: kept\r\n"
            << "\r\n";
    int clientfd = connect_to_loopback(port);
    write_all(clientfd, request.str());
    std::string response = read_response_until_idle(clientfd);
    cr_assert_neq(response.find("HTTP/1.1 200 OK"), std::string::npos);
    cr_assert_neq(response.find("REQUEST_METHOD=GET\n"), std::string::npos);
    cr_assert_neq(
        response.find("QUERY_STRING=raw=a%2Bb+c\n"), std::string::npos);
    cr_assert_neq(
        response.find("SCRIPT_NAME=/cgi/env.sh\n"), std::string::npos);
    std::string script_filename = "SCRIPT_FILENAME=" + root + "/cgi/env.sh\n";
    cr_assert_neq(response.find(script_filename), std::string::npos);
    cr_assert_neq(response.find("REMOTE_ADDR=127.0.0.1\n"), std::string::npos);
    cr_assert_neq(
        response.find("SERVER_NAME=example.test\n"), std::string::npos);
    std::ostringstream server_port;
    server_port << "SERVER_PORT=" << port << "\n";
    cr_assert_neq(response.find(server_port.str()), std::string::npos);
    cr_assert_neq(
        response.find("SERVER_PROTOCOL=HTTP/1.1\n"), std::string::npos);
    cr_assert_neq(response.find("CONTENT_LENGTH=0\n"), std::string::npos);
    cr_assert_neq(
        response.find("CONTENT_TYPE=text/plain\n"), std::string::npos);
    cr_assert_neq(response.find("HTTP_ACCEPT=text/plain\n"), std::string::npos);
    std::ostringstream http_host;
    http_host << "HTTP_HOST=example.test:" << port << "\n";
    cr_assert_neq(response.find(http_host.str()), std::string::npos);
    cr_assert_neq(
        response.find("HTTP_X_CUSTOM_HEADER=kept\n"), std::string::npos);

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(clientfd);

    cr_assert(args.result);
}

Test(event_loop, cgi_response_modes_cover_redirect_nph_and_body_only)
{
    logger::log_level() = logger::levels::NOTHING;

    std::string root = make_tmpdir();
    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    write_file(root + "/cgi/redirect.sh",
        "printf 'Location: /elsewhere\\r\\n\\r\\n'\n");
    write_file(root + "/cgi/nph-output.sh",
        "printf 'HTTP/1.1 204 No Content\\r\\nX-NPH: yes\\r\\n\\r\\n'\n");
    write_file(root + "/cgi/body-only.sh", "printf 'body-only\\n'\n");

    uint16_t port = reserve_loopback_port();
    std::vector<Server> servers;
    servers.push_back(make_cgi_server(port, root));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int redirect_fd = connect_to_loopback(port);
    write_all(redirect_fd,
        "GET /cgi/redirect.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string redirect_response = read_response_until_idle(redirect_fd);
    cr_assert_neq(redirect_response.find("HTTP/1.1 302 Moved Temporarily"),
        std::string::npos);
    cr_assert_neq(
        redirect_response.find("Location: /elsewhere\r\n"), std::string::npos);

    int nph_fd = connect_to_loopback(port);
    write_all(
        nph_fd, "GET /cgi/nph-output.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string nph_response = read_response_until_idle(nph_fd);
    cr_assert_eq(
        nph_response.find("HTTP/1.1 204 No Content\r\n"), std::size_t(0));
    cr_assert_neq(nph_response.find("X-NPH: yes\r\n"), std::string::npos);
    cr_assert_eq(nph_response.find("Content-Length:"), std::string::npos);

    int body_fd = connect_to_loopback(port);
    write_all(
        body_fd, "GET /cgi/body-only.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string body_response = read_response_until_idle(body_fd);
    cr_assert_neq(body_response.find("HTTP/1.1 200 OK"), std::string::npos);
    cr_assert_neq(body_response.find("body-only\n"), std::string::npos);

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(redirect_fd);
    close(nph_fd);
    close(body_fd);

    cr_assert(args.result);
}

Test(event_loop, cgi_executes_allowed_delete_method)
{
    logger::log_level() = logger::levels::NOTHING;

    std::string root = make_tmpdir();
    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    write_file(root + "/cgi/method.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'method=%s\\n' \"$REQUEST_METHOD\"\n");

    uint16_t port = reserve_loopback_port();
    std::vector<Server> servers;
    servers.push_back(make_cgi_server(port, root, true));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int delete_fd = connect_to_loopback(port);
    write_all(
        delete_fd, "DELETE /cgi/method.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string delete_response = read_response(delete_fd);
    cr_assert_neq(delete_response.find("HTTP/1.1 200 OK"), std::string::npos);
    cr_assert_neq(delete_response.find("method=DELETE\n"), std::string::npos);

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(delete_fd);

    cr_assert(args.result);
}

Test(event_loop, cgi_rejects_disallowed_method_without_running_script)
{
    logger::log_level() = logger::levels::NOTHING;

    std::string root = make_tmpdir();
    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    std::string marker = root + "/ran-delete";
    write_file(root + "/cgi/method.sh",
        "printf marker > '" + marker
            + "'\n"
              "printf 'Content-Type: text/plain\r\n\r\n'\n"
              "printf 'method=%s\n' \"$REQUEST_METHOD\"\n");

    uint16_t port = reserve_loopback_port();
    std::vector<Server> servers;
    servers.push_back(make_cgi_server(port, root));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int delete_fd = connect_to_loopback(port);
    write_all(
        delete_fd, "DELETE /cgi/method.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string delete_response = read_response(delete_fd);
    cr_assert_neq(delete_response.find("HTTP/1.1 405 Method Not Allowed"),
        std::string::npos);
    cr_assert_eq(access(marker.c_str(), F_OK), -1,
        "disallowed CGI method executed script and created %s", marker.c_str());
    cr_assert_eq(errno, ENOENT, "access() failed: %s", strerror(errno));

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(delete_fd);

    cr_assert(args.result);
}

Test(event_loop, cgi_head_executes_script_and_discards_response_body)
{
    logger::log_level() = logger::levels::NOTHING;

    std::string root = make_tmpdir();
    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    std::string marker = root + "/ran-head";
    write_file(root + "/cgi/head.sh",
        "printf marker > '" + marker
            + "'\n"
              "printf 'Content-Type: text/plain\r\n\r\n'\n"
              "printf 'method=%s\n' \"$REQUEST_METHOD\"\n");

    uint16_t port = reserve_loopback_port();
    std::vector<Server> servers;
    servers.push_back(make_cgi_server(port, root));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int head_fd = connect_to_loopback(port);
    write_all(head_fd, "HEAD /cgi/head.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string head_response = read_response(head_fd);
    cr_assert_neq(head_response.find("HTTP/1.1 200 OK"), std::string::npos);
    cr_assert_neq(
        head_response.find("Content-Length: 12\r\n"), std::string::npos);
    cr_assert_eq(head_response.find("method=HEAD\n"), std::string::npos,
        "HEAD response leaked CGI body:\n%s", head_response.c_str());
    cr_assert_eq(access(marker.c_str(), F_OK), 0,
        "HEAD did not execute CGI script: %s", strerror(errno));

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(head_fd);

    cr_assert(args.result);
}

Test(event_loop, cgi_script_path_errors_map_to_http_statuses)
{
    logger::log_level() = logger::levels::NOTHING;

    std::string root = make_tmpdir();
    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    write_file(root + "/cgi/unreadable.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\nnope\\n'\n");
    cr_assert_eq(chmod((root + "/cgi/unreadable.sh").c_str(), 0000), 0,
        "chmod() failed: %s", strerror(errno));
    cr_assert_eq(mkdir((root + "/cgi/directory").c_str(), 0700), 0,
        "mkdir() failed: %s", strerror(errno));

    uint16_t port = reserve_loopback_port();
    std::vector<Server> servers;
    servers.push_back(make_cgi_server(port, root));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int missing_fd = connect_to_loopback(port);
    write_all(
        missing_fd, "GET /cgi/missing.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert_status(read_response(missing_fd), "HTTP/1.1 404 Not Found");

    int unreadable_fd = connect_to_loopback(port);
    write_all(unreadable_fd,
        "GET /cgi/unreadable.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert_status(read_response(unreadable_fd), "HTTP/1.1 403 Forbidden");

    int directory_fd = connect_to_loopback(port);
    write_all(
        directory_fd, "GET /cgi/directory HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert_status(read_response(directory_fd), "HTTP/1.1 403 Forbidden");

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(missing_fd);
    close(unreadable_fd);
    close(directory_fd);

    cr_assert(args.result);
}

Test(event_loop, cgi_invalid_interpreter_maps_to_bad_gateway)
{
    logger::log_level() = logger::levels::NOTHING;

    std::string root = make_tmpdir();
    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    write_file(root + "/cgi/hello.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\nhello\\n'\n");
    write_file(root + "/not-executable", "not an interpreter\n");
    cr_assert_eq(chmod((root + "/not-executable").c_str(), 0600), 0,
        "chmod() failed: %s", strerror(errno));

    uint16_t port = reserve_loopback_port();
    std::vector<Server> servers;
    servers.push_back(
        make_cgi_server(port, root, false, root + "/not-executable"));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int clientfd = connect_to_loopback(port);
    write_all(
        clientfd, "GET /cgi/hello.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert_status(read_response(clientfd), "HTTP/1.1 502 Bad Gateway");

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(clientfd);

    cr_assert(args.result);
}

Test(event_loop, cgi_timeout_returns_gateway_timeout)
{
    logger::log_level() = logger::levels::NOTHING;

    std::string root = make_tmpdir();
    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    write_file(root + "/cgi/slow.sh",
        "sleep 5\n"
        "printf 'Content-Type: text/plain\\r\\n\\r\\nlate\\n'\n");

    uint16_t port = reserve_loopback_port();
    std::vector<Server> servers;
    servers.push_back(make_cgi_server(port, root, false, "/bin/sh", 1));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int clientfd = connect_to_loopback(port);
    write_all(clientfd, "GET /cgi/slow.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert_status(read_response(clientfd), "HTTP/1.1 504 Gateway Timeout");

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(clientfd);

    cr_assert(args.result);
}

Test(event_loop, cgi_output_cap_returns_bad_gateway)
{
    logger::log_level() = logger::levels::NOTHING;

    std::string root = make_tmpdir();
    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    write_file(root + "/cgi/large.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'this output is too large for the configured cap\\n'\n");

    uint16_t port = reserve_loopback_port();
    std::vector<Server> servers;
    servers.push_back(
        make_cgi_server(port, root, false, "/bin/sh", DEFAULT_CGI_TIMEOUT, 32));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int clientfd = connect_to_loopback(port);
    write_all(
        clientfd, "GET /cgi/large.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    assert_status(read_response(clientfd), "HTTP/1.1 502 Bad Gateway");

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(clientfd);

    cr_assert(args.result);
}

Test(event_loop, disconnect_while_cgi_runs_cleans_up_child_and_keeps_clients)
{
    logger::log_level() = logger::levels::NOTHING;

    std::string root = make_tmpdir();
    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    std::string pidfile = root + "/cgi.pid";
    write_file(root + "/cgi/linger.sh",
        "printf '%s\\n' \"$$\" > '" + pidfile
            + "'\n"
              "printf 'Content-Type: text/plain\\r\\n\\r\\npartial output\\n'\n"
              "while :; do :; done\n");
    write_file(root + "/static.txt", "static still works\n");

    uint16_t port = reserve_loopback_port();
    std::vector<Server> servers;
    servers.push_back(make_cgi_server(port, root));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int cgi_fd = connect_to_loopback(port);
    write_all(cgi_fd, "GET /cgi/linger.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    pid_t cgi_pid = wait_for_pid_file(pidfile);

    int static_fd = connect_to_loopback(port);
    close(cgi_fd);
    write_all(static_fd, "GET /static.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");

    std::string static_response = read_response(static_fd);
    cr_assert_neq(static_response.find("HTTP/1.1 200 OK"), std::string::npos);
    cr_assert_neq(
        static_response.find("static still works\n"), std::string::npos);
    bool cgi_gone = wait_process_gone(cgi_pid);

    cr_assert_eq(
        kill(getpid(), SIGTERM), 0, "kill() failed: %s", strerror(errno));
    cr_assert_eq(pthread_join(thread, NULL), 0, "pthread_join() failed");
    close(static_fd);

    cr_assert(args.result);
    cr_assert(cgi_gone, "process %ld still exists", static_cast<long>(cgi_pid));
}
