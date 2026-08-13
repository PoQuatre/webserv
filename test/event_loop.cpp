/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   event_loop.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 19:23:48 by mle-flem          #+#    #+#             */
/*   Updated: 2026/08/13 04:43:40 by mle-flem         ###   ########.fr       */
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
#include "test_helpers.hpp"

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

static std::string read_response(int fd, long usec = 2000000)
{
    timeval timeout = { };
    timeout.tv_sec = usec / 1000000;
    timeout.tv_usec = usec % 1000000;
    cr_assert_eq(
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)), 0,
        "setsockopt() failed: %s", strerror(errno));

    std::string response;
    char buffer[512];
    ssize_t n = read(fd, buffer, sizeof(buffer));

    cr_assert_gt(n, 0, "read() failed: %s", strerror(errno));
    response.append(buffer, static_cast<std::size_t>(n));
    return response;
}

static std::string make_cgi_root()
{
    std::string root = test_tmpdir("webserv-cgi-test");

    cr_assert_eq(mkdir((root + "/cgi").c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    return root;
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

static bool wait_process_gone(pid_t pid)
{
    for (int attempts = 0; attempts < 1000; ++attempts) {
        errno = 0;
        if (kill(pid, 0) == -1) {
            cr_assert_eq(errno, ESRCH, "kill(%ld, 0) failed: %s",
                static_cast<long>(pid), strerror(errno));
            return true;
        }
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
    cgi_config.cgi_pass = cgi_pass;
    cgi_config.cgi_timeout = cgi_timeout;
    cgi_config.cgi_output_buffer_size = cgi_output_buffer_size;

    cgi_location.path = "/cgi";
    cgi_location.config = cgi_config;
    cgi_location.type = location::CLASSIC;
    locations.push_back(cgi_location);

    return Server(locations, "test", listen_addr.str(), config);
}

struct CgiHarness {
    CgiHarness()
        : root(make_cgi_root())
        , port(0)
        , loop(NULL)
        , running(false)
        , allow_delete(false)
        , cgi_pass("/bin/sh")
        , cgi_timeout(DEFAULT_CGI_TIMEOUT)
        , cgi_output_buffer_size(DEFAULT_CGI_OUTPUT_BUFFER_SIZE)
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
    bool allow_delete;
    std::string cgi_pass;
    uint32_t cgi_timeout;
    std::size_t cgi_output_buffer_size;
};

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
    if (!harness.running) {
        harness.port = reserve_loopback_port();
        harness.servers.push_back(make_cgi_server(harness.port, harness.root,
            harness.allow_delete, harness.cgi_pass, harness.cgi_timeout,
            harness.cgi_output_buffer_size));
        harness.loop = new EventLoop(harness.servers);
        harness.args.loop = harness.loop;
        harness.args.result = false;
        cr_assert_eq(
            pthread_create(&harness.thread, NULL, &run_loop, &harness.args), 0,
            "pthread_create() failed");
        harness.running = true;
    }
    int fd = connect_to_loopback(harness.port);

    write_all(fd, request);
    return fd;
}

static std::string perform_request(
    CgiHarness &harness, const std::string &request, long usec = 2000000)
{
    int fd = send_request(harness, request);
    std::string response = read_response(fd, usec);

    close(fd);
    return response;
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

Test(event_loop, duplicate_listen_values_share_first_listener)
{
    logger::log_level() = logger::levels::NOTHING;

    uint16_t port = reserve_loopback_port();
    std::ostringstream listen_addr;
    listen_addr << "127.0.0.1:" << port;

    std::string first_root = test_tmpdir("webserv-first-listener");
    std::string second_root = test_tmpdir("webserv-second-listener");
    test_write_file(first_root + "/index.html", "first server\n");
    test_write_file(second_root + "/index.html", "second server\n");

    Config first_config = { };
    first_config.root = first_root;
    first_config.allowed_methods[http::methods::GET] = true;
    Config second_config = { };
    second_config.root = second_root;
    second_config.allowed_methods[http::methods::GET] = true;

    std::vector<Server> servers;
    servers.push_back(Server(
        std::vector<Location>(), "first", listen_addr.str(), first_config));
    servers.push_back(Server(
        std::vector<Location>(), "second", listen_addr.str(), second_config));

    EventLoop loop(servers);
    LoopThreadArgs args = { &loop, false };
    pthread_t thread;
    cr_assert_eq(pthread_create(&thread, NULL, &run_loop, &args), 0,
        "pthread_create() failed");

    int clientfd = connect_to_loopback(port);
    write_all(clientfd, "GET /index.html HTTP/1.1\r\nHost: unmatched\r\n\r\n");

    std::string response = read_response(clientfd);
    test_assert_status(response, "HTTP/1.1 200 OK");
    cr_assert_neq(response.find("first server\n"), std::string::npos);
    cr_assert_eq(response.find("second server\n"), std::string::npos);

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
    test_write_file(harness.root + "/cgi/hello.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'method=%s query=%s\\n' \"$REQUEST_METHOD\" "
        "\"$QUERY_STRING\"\n");
    test_write_file(
        harness.root + "/cgi/slow.sh", "sleep 1\nprintf 'slow\\n'\n");
    test_write_file(harness.root + "/static.txt", "static body\n");

    std::string cgi_response = perform_request(harness,
        "GET /cgi/hello.sh?name=webserv HTTP/1.1\r\nHost: localhost\r\n\r\n");
    test_assert_status(cgi_response, "HTTP/1.1 200 OK");
    cr_assert_neq(
        cgi_response.find("Content-Type: text/plain"), std::string::npos);
    cr_assert_neq(cgi_response.find("method=GET query=name=webserv\n"),
        std::string::npos);

    int slow_fd = send_request(
        harness, "GET /cgi/slow.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string static_response = perform_request(
        harness, "GET /static.txt HTTP/1.1\r\nHost: localhost\r\n\r\n", 200000);
    test_assert_status(static_response, "HTTP/1.1 200 OK");
    cr_assert_neq(static_response.find("static body\n"), std::string::npos);

    close(slow_fd);
}

Test(event_loop, cgi_receives_post_and_chunked_bodies_on_stdin)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    test_write_file(harness.root + "/cgi/echo.sh",
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
    test_assert_status(post_response, "HTTP/1.1 200 OK");
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
    test_assert_status(chunked_response, "HTTP/1.1 200 OK");
    cr_assert_neq(
        chunked_response.find("content_length=9\n"), std::string::npos);
    cr_assert_neq(
        chunked_response.find("content_type=text/plain\n"), std::string::npos);
    cr_assert_neq(chunked_response.find("body=Wikipedia\n"), std::string::npos);
    cr_assert_neq(chunked_response.find("stdin_eof=yes\n"), std::string::npos);
}

Test(event_loop, cgi_executes_allowed_delete_method)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    test_write_file(harness.root + "/cgi/method.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'method=%s\\n' \"$REQUEST_METHOD\"\n");

    harness.allow_delete = true;

    std::string delete_response = perform_request(
        harness, "DELETE /cgi/method.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    test_assert_status(delete_response, "HTTP/1.1 200 OK");
    cr_assert_neq(delete_response.find("method=DELETE\n"), std::string::npos);
}

Test(event_loop, cgi_rejects_disallowed_method_without_running_script)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    std::string marker = harness.root + "/ran-delete";
    test_write_file(harness.root + "/cgi/method.sh",
        "printf marker > '" + marker
            + "'\n"
              "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
              "printf 'method=%s\\n' \"$REQUEST_METHOD\"\n");

    std::string delete_response = perform_request(
        harness, "DELETE /cgi/method.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    test_assert_status(delete_response, "HTTP/1.1 405 Method Not Allowed");
    cr_assert_eq(access(marker.c_str(), F_OK), -1,
        "disallowed CGI method executed script and created %s", marker.c_str());
    cr_assert_eq(errno, ENOENT, "access() failed: %s", strerror(errno));
}

Test(event_loop, cgi_head_executes_script_and_discards_response_body)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    std::string marker = harness.root + "/ran-head";
    test_write_file(harness.root + "/cgi/head.sh",
        "printf marker > '" + marker
            + "'\n"
              "printf 'Content-Type: text/plain\r\n\r\n'\n"
              "printf 'method=%s\n' \"$REQUEST_METHOD\"\n");

    std::string head_response = perform_request(
        harness, "HEAD /cgi/head.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    test_assert_status(head_response, "HTTP/1.1 200 OK");
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
    test_write_file(harness.root + "/cgi/unreadable.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\nnope\\n'\n");
    cr_assert_eq(chmod((harness.root + "/cgi/unreadable.sh").c_str(), 0000), 0,
        "chmod() failed: %s", strerror(errno));
    cr_assert_eq(mkdir((harness.root + "/cgi/directory").c_str(), 0700), 0,
        "mkdir() failed: %s", strerror(errno));

    test_assert_status(
        perform_request(
            harness, "GET /cgi/missing.sh HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        "HTTP/1.1 404 Not Found");
    test_assert_status(
        perform_request(harness,
            "GET /cgi/unreadable.sh HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        "HTTP/1.1 403 Forbidden");
    test_assert_status(
        perform_request(
            harness, "GET /cgi/directory HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        "HTTP/1.1 403 Forbidden");
}

Test(event_loop, cgi_invalid_interpreter_maps_to_bad_gateway)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    test_write_file(harness.root + "/cgi/hello.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\nhello\\n'\n");
    test_write_file(harness.root + "/not-executable", "not an interpreter\n");
    cr_assert_eq(chmod((harness.root + "/not-executable").c_str(), 0600), 0,
        "chmod() failed: %s", strerror(errno));

    harness.cgi_pass = harness.root + "/not-executable";

    test_assert_status(
        perform_request(
            harness, "GET /cgi/hello.sh HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        "HTTP/1.1 502 Bad Gateway");
}

Test(event_loop, cgi_timeout_returns_gateway_timeout)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    test_write_file(harness.root + "/cgi/slow.sh",
        "sleep 5\n"
        "printf 'Content-Type: text/plain\\r\\n\\r\\nlate\\n'\n");

    harness.cgi_timeout = 1;

    test_assert_status(
        perform_request(
            harness, "GET /cgi/slow.sh HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        "HTTP/1.1 504 Gateway Timeout");
}

Test(event_loop, cgi_output_cap_returns_bad_gateway)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    test_write_file(harness.root + "/cgi/large.sh",
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'this output is too large for the configured cap\\n'\n");

    harness.cgi_output_buffer_size = 32;

    test_assert_status(
        perform_request(
            harness, "GET /cgi/large.sh HTTP/1.1\r\nHost: localhost\r\n\r\n"),
        "HTTP/1.1 502 Bad Gateway");
}

Test(event_loop, disconnect_while_cgi_runs_cleans_up_child_and_keeps_clients)
{
    logger::log_level() = logger::levels::NOTHING;

    CgiHarness harness;
    std::string pidfile = harness.root + "/cgi.pid";
    test_write_file(harness.root + "/cgi/linger.sh",
        "printf '%s\\n' \"$$\" > '" + pidfile
            + "'\n"
              "printf 'Content-Type: text/plain\\r\\n\\r\\npartial output\\n'\n"
              "while :; do :; done\n");
    test_write_file(harness.root + "/static.txt", "static still works\n");

    int cgi_fd = send_request(
        harness, "GET /cgi/linger.sh HTTP/1.1\r\nHost: localhost\r\n\r\n");
    pid_t cgi_pid = wait_for_pid_file(pidfile);

    int static_fd = connect_to_loopback(harness.port);
    close(cgi_fd);
    write_all(static_fd, "GET /static.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string static_response = read_response(static_fd);
    close(static_fd);
    test_assert_status(static_response, "HTTP/1.1 200 OK");
    cr_assert_neq(
        static_response.find("static still works\n"), std::string::npos);
    bool cgi_gone = wait_process_gone(cgi_pid);

    cr_assert(cgi_gone, "process %ld still exists", static_cast<long>(cgi_pid));
}
