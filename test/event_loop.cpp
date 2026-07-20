/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   event_loop.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 19:23:48 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/20 13:40:35 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
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

Test(event_loop, listener_and_signal_sources_dispatch_readiness)
{
    logger::log_level() = logger::levels::NOTHING;

    uint16_t port = reserve_loopback_port();
    std::ostringstream listen_addr;
    listen_addr << "127.0.0.1:" << port;

    Config config = { };
    config.allowed_methods[http::methods::GET] = true;
    std::vector<Server> servers;
    servers.push_back(Server(
        std::vector<Location>(), "test", listen_addr.str(), config, true));

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
