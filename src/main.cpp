/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 18:53:25 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/16 23:22:23 by mle-flem         ###   ########.fr       */
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
#include <iostream>
#include <vector>

#include "logger.hpp"
#include "webserv.hpp"

#define MAX_EVENTS 16

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

bool is_sockfd(const std::vector<Server> &servers, int32_t fd)
{
    for (std::vector<Server>::const_iterator it = servers.begin();
        it != servers.end(); ++it) {
        if (it->get_sockfd() == fd)
            return true;
    }
    return false;
}

void accept_client(int32_t epollfd, int32_t sockfd)
{

    int32_t clientfd = accept(sockfd, NULL, NULL);
    if (clientfd == -1) {
        L_WARN("Failed to accept client");
        return;
    }

    L_DEBUG("Accepting client");

    fcntl(clientfd, F_SETFL, O_NONBLOCK);

    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = clientfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev);
}

void close_client(int32_t epollfd, int32_t clientfd)
{
    L_DEBUG("Closing client nb: {}", clientfd);

    epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, NULL);
    close(clientfd);
}

void handle_client(int32_t epollfd, int32_t clientfd)
{
    L_INFO("Handling client");

    char buf[4096];
    ssize_t nbytes = recv(clientfd, buf, sizeof(buf), 0);
    if (nbytes <= 0) {
        L_WARN("failed to read from client, got 0 bytes");
        close_client(epollfd, clientfd);
        return;
    }

    L_TRACE("Got {} bytes from client data: {}", nbytes, buf);

    // TODO: the real http handling should go here
    char *path = buf + 5;
    *strchr(path, ' ') = 0;

    L_TRACE("Got path: {}", path);
    int32_t fd = open(path, O_RDONLY);
    if (fd == -1) {
        L_WARN("Failed to open path");
        close_client(epollfd, clientfd);
        return;
    }

    L_TRACE("Sending response");
    send(clientfd, "HTTP/1.0 200 OK\r\n\r\n", 19, 0);
    sendfile(clientfd, fd, 0, 4096);
    close(fd);
    close_client(epollfd, clientfd);
}

}

int32_t main(int32_t ac, char **av)
{
    if (ac < 2) {
        std::cerr << "Usage ./webserv <configuration>\n";
        return 1;
    }

    std::vector<Server> servers = parse_config(av[1]);

    // NOTE: the parameter of epoll_create doesn't mean anything since
    // linux 2.6.8 (or 14/08/2004)
    L_DEBUG("Creating epoll instance");
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

    epoll_event events[MAX_EVENTS];
    bool running = true;
    while (running) {

        L_TRACE("Waiting for connections");
        int32_t nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            // NOTE: epoll_wait interrupted by signal, loop again
            if (errno == EINTR)
                continue;

            L_ERROR("Failed to wait for epoll: {}", strerror(errno));
            return 1;
        }

        L_TRACE("Got {} events", nfds);
        for (int32_t i = 0; i < nfds; ++i) {
            int32_t fd = events[i].data.fd;

            if (fd == g_signal_pipe[0]) {
                drain_signal_pipe();
                running = false;
            } else if (is_sockfd(servers, fd)) {
                accept_client(epollfd, fd);
            } else {
                handle_client(epollfd, fd);
            }
        }
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
