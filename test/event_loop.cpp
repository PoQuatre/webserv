/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   event_loop.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 19:23:48 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/18 21:40:26 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

#include <arpa/inet.h>
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
    timeout.tv_sec = 1;
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

static Server make_cgi_server(
    uint16_t port, const std::string &root, bool allow_delete = false)
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
    config.allowed_methods[http::methods::DELETE] = allow_delete;

    cgi_config = config;
    cgi_config.cgi_enabled = true;
    cgi_config.cgi_pass = "/bin/sh";
    cgi_config.cgi_timeout = DEFAULT_CGI_TIMEOUT;
    cgi_config.cgi_output_buffer_size = DEFAULT_CGI_OUTPUT_BUFFER_SIZE;

    cgi_location.path = "/cgi";
    cgi_location.config = cgi_config;
    cgi_location.type = location::CLASSIC;
    locations.push_back(cgi_location);

    return Server(locations, "test", listen_addr.str(), config);
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
