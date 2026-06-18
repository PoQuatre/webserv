/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 18:53:25 by mle-flem          #+#    #+#             */
/*   Updated: 2026/06/18 19:50:50 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <fcntl.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Connection.hpp"
#include "cli.hpp"
#include "config-parser.hpp"
#include "dispatcher.hpp"
#include "logger.hpp"

#define MAX_EVENTS 16
#define EPOLL_RDONLY (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP)
#define EPOLL_WRONLY (EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP)
#define EPOLL_RDWR (EPOLL_RDONLY | EPOLLOUT)

namespace {

int32_t g_signal_pipe[2] = { -1, -1 };

void signal_handler(int32_t signo)
{
    char byte = static_cast<char>(signo);
    write(g_signal_pipe[1], &byte, 1);
}

bool init_signal_handlers(int32_t epollfd)
{
    L_DEBUG("Initializing signal handlers");

    if (pipe(g_signal_pipe) == -1) {
        L_ERROR("Failed to create signal pipe: {}", strerror(errno));
        return false;
    }

    fcntl(g_signal_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(g_signal_pipe[1], F_SETFL, O_NONBLOCK);
    fcntl(g_signal_pipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(g_signal_pipe[1], F_SETFD, FD_CLOEXEC);

    (void)signal(SIGINT, &signal_handler);
    (void)signal(SIGTERM, &signal_handler);
    (void)signal(SIGQUIT, &signal_handler);

    epoll_event ev = { };
    ev.events = EPOLLIN;
    ev.data.fd = g_signal_pipe[0];
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, g_signal_pipe[0], &ev) == -1) {
        L_ERROR(
            "Failed to add signal pipe to epoll instance: {}", strerror(errno));
        close(g_signal_pipe[0]);
        close(g_signal_pipe[1]);
        return false;
    }

    return true;
}

void drain_signal_pipe()
{
    char buf[16];
    ssize_t n;

    while ((n = read(g_signal_pipe[0], buf, sizeof(buf))) > 0)
        for (ssize_t j = 0; j < n; ++j)
            L_DEBUG("Received signal {}, shutting down", (int)buf[j]);
}

void accept_client(int32_t epollfd, int32_t sockfd,
    std::map<int32_t, Connection> &connections, const Server &server)
{
    int32_t clientfd = accept(sockfd, NULL, NULL);
    if (clientfd == -1) {
        L_WARN("Failed to accept client: {}", strerror(errno));
        return;
    }

    L_DEBUG("Accepting client {}", clientfd);

    fcntl(clientfd, F_SETFL, O_NONBLOCK);

    epoll_event ev;
    ev.events = EPOLL_RDONLY;
    ev.data.fd = clientfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev);

    connections[clientfd] = Connection(clientfd, server);
}

void close_client(int32_t epollfd, int32_t clientfd, Connection &conn)
{
    L_DEBUG("Closing client {}", clientfd);

    epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, NULL);
    conn.reset();
    close(clientfd);
}

void dispatch_pending(
    int32_t epollfd, int32_t fd, uint32_t evts, Connection &conn)
{
    if (conn.is_parse_complete()) {
        conn.enqueue_response(
            dispatcher::handle(conn.request(), conn.server()));
        conn.on_writable();
        if (evts & EPOLLIN) {
            epoll_event ev = { };
            ev.events = EPOLL_WRONLY;
            ev.data.fd = fd;
            epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
        }
    } else if (evts & EPOLLOUT) {
        epoll_event ev = { };
        ev.events = EPOLL_RDONLY;
        ev.data.fd = fd;
        epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
    }
}

void process_client(
    int32_t epollfd, int32_t fd, uint32_t evts, Connection &conn)
{
    bool close_conn = false;

    if (evts & EPOLLIN) {
        if (!conn.on_readable()) {
            if (conn.is_parse_error()) {
                conn.enqueue_response(
                    dispatcher::error_response(conn.parse_error_code()));
                if (!conn.on_writable()) {
                    close_conn = true;
                } else {
                    epoll_event ev = { };
                    ev.events = EPOLL_WRONLY;
                    ev.data.fd = fd;
                    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
                }
            } else {
                close_conn = true;
            }
        } else if (conn.is_parse_complete()) {
            dispatch_pending(epollfd, fd, evts, conn);
        }
    }

    if (!close_conn && evts & EPOLLOUT) {
        if (!conn.on_writable()) {
            close_conn = true;
        } else if (!conn.is_sending()) {
            if (!conn.keep_alive() || conn.is_parse_error()) {
                close_conn = true;
            } else {
                conn.reset();
                dispatch_pending(epollfd, fd, evts, conn);
            }
        }
    }

    if (evts & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
        close_conn = true;

    if (close_conn)
        close_client(epollfd, fd, conn);
}

void process_io_events(int32_t epollfd, std::vector<Server> &servers,
    std::map<int32_t, Connection> &connections, bool &running,
    epoll_event (&events)[MAX_EVENTS], int32_t nfds)
{
    for (int32_t i = 0; i < nfds; ++i) {
        int32_t fd = events[i].data.fd;

        if (fd == g_signal_pipe[0]) {
            drain_signal_pipe();
            running = false;
            continue;
        }

        bool is_server_fd = false;
        for (std::size_t j = 0; j < servers.size(); ++j) {
            if (servers[j].get_sockfd() == fd) {
                accept_client(epollfd, fd, connections, servers[j]);
                is_server_fd = true;
                break;
            }
        }
        if (is_server_fd)
            continue;

        process_client(epollfd, fd, events[i].events, connections[fd]);
    }
}

}

int32_t main(int32_t ac, char **av)
{
    logger::print_date() = false;
    std::vector<Server> servers;

    cli::ParsedArgs args = cli::parse_arguments(ac, av);
    if (args.should_quit)
        return 1;

    if (args.flags[cli::flags::SILENT])
        logger::log_level() = logger::levels::NOTHING;
    if (args.flags[cli::flags::VERBOSE])
        logger::log_level() = logger::levels::TRACE;

    if (!ConfigParser::parse_config(args.config_path, servers))
        return 1;

    logger::print_date() = true;
    L_DEBUG("Creating epoll instance");

    // NOTE: the parameter of epoll_create doesn't mean anything since
    // linux 2.6.8 (or 14/08/2004)
    int32_t epollfd = epoll_create(42);
    if (epollfd == -1) {
        L_ERROR("Failed to create epoll instance: {}", strerror(errno));
        return 1;
    }

    if (fcntl(epollfd, F_SETFD, FD_CLOEXEC) == -1) {
        L_ERROR("Failed to set options on epoll instance", strerror(errno));
        return 1;
    }

    for (size_t i = 0; i < servers.size(); i++)
        if (!servers[i].init(epollfd))
            return 1;

    if (!init_signal_handlers(epollfd))
        return 1;

    L_INFO("Started webserv");

    std::map<int32_t, Connection> connections;

    epoll_event events[MAX_EVENTS];
    bool running = true;
    while (running) {
        L_TRACE("Waiting for events");
        int32_t nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            // NOTE: epoll_wait interrupted by signal, loop again
            if (errno == EINTR)
                continue;

            L_ERROR("Failed to wait for epoll: {}", strerror(errno));
            return 1;
        }

        L_TRACE("Got {} events", nfds);
        process_io_events(epollfd, servers, connections, running, events, nfds);
    }

    L_INFO("Stopped webserv");

    close(g_signal_pipe[0]);
    close(g_signal_pipe[1]);

    for (size_t i = 0; i < servers.size(); i++)
        servers[i].shutdown(epollfd);

    close(epollfd);

    L_INFO("Stopped webserv");

    return 0;
}
