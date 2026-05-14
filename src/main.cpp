/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 18:53:25 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/14 21:40:40 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <fcntl.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

#include "webserv.hpp"

#define MAX_EVENTS 16

namespace {

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
    logger::info("Accepting client");

    int32_t clientfd = accept(sockfd, NULL, NULL);
    if (clientfd == -1) {
        logger::error("Failed to accept client");
        return;
    }

    fcntl(clientfd, F_SETFL, O_NONBLOCK);

    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = clientfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev);
}

void close_client(int32_t epollfd, int32_t clientfd)
{
    logger::info("Closing client");

    epoll_ctl(epollfd, EPOLL_CTL_DEL, clientfd, NULL);
    close(clientfd);
}

void handle_client(int32_t epollfd, int32_t clientfd)
{
    logger::info("Handling client");

    char buf[4096];
    ssize_t nbytes = recv(clientfd, buf, sizeof(buf), 0);
    if (nbytes <= 0) {
        logger::error("failed to read from client");
        close_client(epollfd, clientfd);
        return;
    }

    logger::info("Got {} bytes from client data: {}", nbytes, buf);

    // TODO: the real http handling should go here
    char *path = buf + 5;
    *strchr(path, ' ') = 0;

    logger::info("Got path: {}", path);
    int32_t fd = open(path, O_RDONLY);
    if (fd == -1) {
        logger::error("Failed to open path");
        close_client(epollfd, clientfd);
        return;
    }

    logger::info("Sending response");
    send(clientfd, "HTTP/1.0 200 OK\r\n\r\n", 19, 0);
    sendfile(clientfd, fd, 0, 4096);
    close(fd);
    close_client(epollfd, clientfd);
    logger::info("Closing connection");
}

}

int32_t main(int32_t ac, char **av)
{
    if (ac < 2) {
        std::cerr << "Usage ./webserv <configuration>\n";
        return 1;
    }

    logger::info("Started webserv");

    std::vector<Server> servers = parse_config(av[1]);

    // NOTE: the parameter of epoll_create doesn't mean anything since
    // linux 2.6.8 (or 14/08/2004)
    logger::info("Creating epoll instance");
    int32_t epollfd = epoll_create(42);
    if (epollfd == -1) {
        logger::error("Failed to create epoll instance");
        perror("epoll_create");
        return 1;
    }

    if (fcntl(epollfd, F_SETFD, FD_CLOEXEC) == -1) {
        logger::error("Failed to set options on epoll instance");
        perror("fcntl");
        return 1;
    }

    for (size_t i = 0; i < servers.size(); i++) {
        if (!servers[i].init(epollfd)) {
            return 1;
        }
    }

    logger::info("Starting main wait loop");

    epoll_event events[MAX_EVENTS];
    while (true) {

        logger::info("Waiting for connections");
        int32_t nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            logger::error("Failed to wait for epoll");
            perror("epoll_wait");
            return 1;
        }

        logger::info("Got {} connections ", nfds);
        for (int32_t i = 0; i < nfds; ++i) {
            if (is_sockfd(servers, events[i].data.fd)) {
                accept_client(epollfd, events[i].data.fd);
            } else {
                handle_client(epollfd, events[i].data.fd);
            }
        }
    }

    logger::info("Stopped webserv");

    return 0;
}
