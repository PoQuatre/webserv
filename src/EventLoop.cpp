/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 19:07:46 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/17 19:20:26 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "EventLoop.hpp"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

#include "dispatcher.hpp"
#include "logger.hpp"

#define MAX_EVENTS 16
#define EPOLL_RDONLY (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP)
#define EPOLL_WRONLY (EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP)

namespace {

int32_t g_signal_pipe[2] = { -1, -1 };

void signal_handler(int32_t signo)
{
    char byte = static_cast<char>(signo);

    if (g_signal_pipe[1] != -1)
        write(g_signal_pipe[1], &byte, 1);
}

void drain_signal_pipe()
{
    char buf[16];
    ssize_t n;

    while ((n = read(g_signal_pipe[0], buf, sizeof(buf))) > 0)
        for (ssize_t j = 0; j < n; ++j)
            L_DEBUG("Received signal {}, shutting down", (int)buf[j]);
}

}

EventLoop::EventLoop(std::vector<Server> &servers)
    : _servers(servers)
    , _epollfd(-1)
{
}

EventLoop::~EventLoop() { cleanup(); }

EventLoop::EventSource::EventSource()
    : type(SOURCE_SIGNAL)
    , server(NULL)
{
}

EventLoop::EventSource::EventSource(
    EventLoop::EventSource::Type source_type, const Server *server_context)
    : type(source_type)
    , server(server_context)
{
}

bool EventLoop::run()
{
    if (!setup()) {
        cleanup();
        return false;
    }

    L_INFO("Started webserv");

    epoll_event events[MAX_EVENTS];
    bool running = true;
    bool ok = true;
    while (running) {
        L_TRACE("Waiting for events");
        int32_t nfds = epoll_wait(_epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR)
                continue;
            L_ERROR("Failed to wait for epoll: {}", strerror(errno));
            ok = false;
            break;
        }

        L_TRACE("Got {} events", nfds);
        for (int32_t i = 0; i < nfds; ++i)
            process_io_event(events[i].data.fd, events[i].events, running);
    }

    if (ok)
        L_INFO("Stopped webserv");
    cleanup();
    return ok;
}

bool EventLoop::setup()
{
    if (!create_epoll())
        return false;
    if (!register_servers())
        return false;
    return init_signal_handlers();
}

bool EventLoop::create_epoll()
{
    L_DEBUG("Creating epoll instance");

    // NOTE: the parameter of epoll_create doesn't mean anything since
    // linux 2.6.8 (or 14/08/2004)
    _epollfd = epoll_create(42);
    if (_epollfd == -1) {
        L_ERROR("Failed to create epoll instance: {}", strerror(errno));
        return false;
    }

    if (fcntl(_epollfd, F_SETFD, FD_CLOEXEC) == -1) {
        L_ERROR("Failed to set options on epoll instance", strerror(errno));
        return false;
    }
    return true;
}

bool EventLoop::register_servers()
{
    for (std::size_t i = 0; i < _servers.size(); i++) {
        if (!_servers[i].init_socket())
            return false;

        if (!add_source(_servers[i].get_sockfd(), EPOLLIN,
                EventSource(EventSource::SOURCE_LISTENER, &_servers[i])))
            return false;
    }
    return true;
}

bool EventLoop::init_signal_handlers()
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

    return add_source(g_signal_pipe[0], EPOLLIN,
        EventSource(EventSource::SOURCE_SIGNAL, NULL));
}

bool EventLoop::add_source(
    int32_t fd, uint32_t events, const EventLoop::EventSource &source)
{
    epoll_event ev = { };
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(_epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        L_ERROR("Failed to add event source {} to epoll instance: {}", fd,
            strerror(errno));
        return false;
    }
    _sources[fd] = source;
    return true;
}

void EventLoop::remove_source(int32_t fd)
{
    if (_epollfd != -1 && fd != -1) {
        if (epoll_ctl(_epollfd, EPOLL_CTL_DEL, fd, NULL) == -1)
            L_WARN("Failed to remove event source {} from epoll instance: {}",
                fd, strerror(errno));
    }
    _sources.erase(fd);
}

void EventLoop::cleanup()
{
    for (std::map<int32_t, Connection>::iterator it = _connections.begin();
        it != _connections.end(); ++it) {
        if (_epollfd != -1)
            epoll_ctl(_epollfd, EPOLL_CTL_DEL, it->first, NULL);
        it->second.reset();
        close(it->first);
    }
    _connections.clear();

    if (g_signal_pipe[0] != -1) {
        remove_source(g_signal_pipe[0]);
        close(g_signal_pipe[0]);
        g_signal_pipe[0] = -1;
    }
    if (g_signal_pipe[1] != -1) {
        close(g_signal_pipe[1]);
        g_signal_pipe[1] = -1;
    }

    for (std::size_t i = 0; i < _servers.size(); i++) {
        if (_servers[i].get_sockfd() != -1)
            remove_source(_servers[i].get_sockfd());
        _servers[i].shutdown_socket();
    }

    _sources.clear();

    if (_epollfd != -1) {
        close(_epollfd);
        _epollfd = -1;
    }
}

void EventLoop::process_io_event(int32_t fd, uint32_t events, bool &running)
{
    std::map<int32_t, EventSource>::const_iterator source = _sources.find(fd);
    if (source != _sources.end()) {
        dispatch_source(fd, events, source->second, running);
        return;
    }

    process_client(fd, events, _connections[fd]);
}

void EventLoop::dispatch_source(int32_t fd, uint32_t events,
    const EventLoop::EventSource &source, bool &running)
{
    (void)events;
    if (source.type == EventSource::SOURCE_SIGNAL) {
        drain_signal_pipe();
        running = false;
        return;
    }
    if (source.server != NULL)
        accept_client(fd, *source.server);
}

void EventLoop::accept_client(int32_t sockfd, const Server &server)
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
    epoll_ctl(_epollfd, EPOLL_CTL_ADD, clientfd, &ev);

    _connections[clientfd] = Connection(clientfd, server);
}

void EventLoop::close_client(int32_t clientfd, Connection &conn)
{
    L_DEBUG("Closing client {}", clientfd);

    epoll_ctl(_epollfd, EPOLL_CTL_DEL, clientfd, NULL);
    conn.reset();
    close(clientfd);
    _connections.erase(clientfd);
}

void EventLoop::dispatch_pending(
    int32_t fd, uint32_t events, Connection &conn) const
{
    if (conn.is_parse_complete()) {
        conn.enqueue_response(
            dispatcher::handle(conn.request(), conn.server()));
        conn.on_writable();
        if (events & EPOLLIN) {
            epoll_event ev = { };
            ev.events = EPOLL_WRONLY;
            ev.data.fd = fd;
            epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &ev);
        }
    } else if (events & EPOLLOUT) {
        epoll_event ev = { };
        ev.events = EPOLL_RDONLY;
        ev.data.fd = fd;
        epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &ev);
    }
}

void EventLoop::process_client(int32_t fd, uint32_t events, Connection &conn)
{
    bool close_conn = false;

    if (events & EPOLLIN) {
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
                    epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &ev);
                }
            } else {
                close_conn = true;
            }
        } else if (conn.is_parse_complete()) {
            dispatch_pending(fd, events, conn);
        }
    }

    if (!close_conn && events & EPOLLOUT) {
        if (!conn.on_writable()) {
            close_conn = true;
        } else if (!conn.is_sending()) {
            if (!conn.keep_alive() || conn.is_parse_error()) {
                close_conn = true;
            } else {
                conn.reset();
                dispatch_pending(fd, events, conn);
            }
        }
    }

    if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
        close_conn = true;

    if (close_conn)
        close_client(fd, conn);
}
