/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 19:07:46 by mle-flem          #+#    #+#             */
/*   Updated: 2026/08/13 22:54:52 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "EventLoop.hpp"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <set>

#include "cgi.hpp"
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

std::string lower_ascii(std::string value)
{
    for (std::size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(
            std::tolower(static_cast<unsigned char>(value[i])));
    return value;
}

std::string host_name(const http::request &req)
{
    std::map<std::string, std::string>::const_iterator it
        = req.headers.find("host");

    if (it == req.headers.end() || it->second.empty())
        return "";
    std::string host = it->second;
    if (host[0] == '[')
        return "";
    std::size_t colon = host.find(':');
    if (colon == std::string::npos)
        return lower_ascii(host);
    if (colon == 0 || host.find(':', colon + 1) != std::string::npos
        || colon + 1 == host.size())
        return "";
    return lower_ascii(host.substr(0, colon));
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
    , connection(NULL)
{
}

EventLoop::EventSource::EventSource(const Server &server_context)
    : type(SOURCE_LISTENER)
    , server(&server_context)
    , connection(NULL)
{
}

EventLoop::EventSource::EventSource(Connection &connection_context)
    : type(SOURCE_CLIENT)
    , server(NULL)
    , connection(&connection_context)
{
}

EventLoop::EventSource::EventSource(EventLoop::EventSource::Type cgi_type)
    : type(cgi_type)
    , server(NULL)
    , connection(NULL)
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
        _cgi_lifecycle.reap_pending_children();
        expire_cgi_jobs();
        int32_t nfds = epoll_wait(
            _epollfd, events, MAX_EVENTS, _cgi_lifecycle.wait_timeout());
        if (nfds == -1) {
            if (errno == EINTR)
                continue;
            L_ERROR("Failed to wait for epoll: {}", strerror(errno));
            ok = false;
            break;
        }

        expire_cgi_jobs();
        _cgi_lifecycle.reap_pending_children();
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
    std::set<std::string> listeners;

    for (std::size_t i = 0; i < _servers.size(); i++) {
        if (!listeners.insert(_servers[i].listen_addr()).second)
            continue;

        if (!_servers[i].init_socket())
            return false;

        if (!add_source(
                _servers[i].get_sockfd(), EPOLLIN, EventSource(_servers[i])))
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
    (void)signal(SIGPIPE, SIG_IGN);

    return add_source(g_signal_pipe[0], EPOLLIN, EventSource());
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

bool EventLoop::update_source_events(int32_t fd, uint32_t events) const
{
    if (_sources.find(fd) == _sources.end()) {
        L_WARN("Ignoring event interest update for unknown source {}", fd);
        return false;
    }

    epoll_event ev = { };
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        L_WARN("Failed to update event source {} in epoll instance: {}", fd,
            strerror(errno));
        return false;
    }
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
    std::vector<int32_t> cgi_fds = _cgi_lifecycle.abort_all_requests();
    for (std::size_t i = 0; i < cgi_fds.size(); ++i)
        close_cgi_fd(cgi_fds[i]);

    _cgi_lifecycle.reap_pending_children();

    for (std::map<int32_t, Connection>::iterator it = _connections.begin();
        it != _connections.end(); ++it) {
        remove_source(it->first);
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

    L_WARN("Ignoring event for unknown or stale descriptor {}", fd);
}

void EventLoop::dispatch_source(int32_t fd, uint32_t events,
    const EventLoop::EventSource &source, bool &running)
{
    switch (source.type) {
    case EventSource::SOURCE_LISTENER:
        if (source.server != NULL)
            accept_client(fd, *source.server);
        else
            L_WARN("Ignoring listener event source {} without server", fd);
        break;

    case EventSource::SOURCE_SIGNAL:
        drain_signal_pipe();
        running = false;
        break;

    case EventSource::SOURCE_CLIENT:
        if (source.connection != NULL)
            process_client(fd, events, *source.connection);
        else
            L_WARN("Ignoring client event source {} without connection", fd);
        break;

    case EventSource::SOURCE_CGI_STDIN:
        if (_cgi_lifecycle.process_stdin(fd, events))
            close_cgi_fd(fd);
        break;

    case EventSource::SOURCE_CGI_STDOUT:
        if (_cgi_lifecycle.process_stdout(fd, events))
            finish_cgi_job(fd);
        break;
    }
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
    fcntl(clientfd, F_SETFD, FD_CLOEXEC);

    _connections[clientfd] = Connection(clientfd, server);
    Connection &conn = _connections[clientfd];
    if (!add_source(clientfd, EPOLL_RDONLY, EventSource(conn))) {
        conn.reset();
        close(clientfd);
        _connections.erase(clientfd);
    }
}

void EventLoop::close_client(int32_t clientfd, Connection &conn)
{
    L_DEBUG("Closing client {}", clientfd);

    cancel_cgi_jobs_for(clientfd);
    remove_source(clientfd);
    conn.reset();
    close(clientfd);
    _connections.erase(clientfd);
}

void EventLoop::dispatch_pending(int32_t fd, uint32_t events, Connection &conn)
{
    if (conn.is_parse_complete()) {
        conn.set_server(*select_server(conn.default_server(), conn.request()));
        const Config &cfg
            = dispatcher::config_for(conn.request(), conn.server());
        std::string filesystem_path = cfg.root + conn.request().uri;

        if (conn.is_waiting_cgi())
            return;
        if (!cfg.allowed_methods[conn.request().method]) {
            conn.enqueue_response(dispatcher::error_response(
                conn.request(), cfg, http::status::METHOD_NOT_ALLOWED));
        } else if (!cfg.cgi_pass.empty()) {
            http::status::type error_status = http::status::BAD_GATEWAY;

            L_DEBUG("CGI {} {} -> {}",
                http::methods::strings[conn.request().method],
                conn.request().uri, filesystem_path);
            if (start_cgi_request(fd, conn, cfg, filesystem_path, error_status))
                return;
            conn.enqueue_response(
                dispatcher::error_response(conn.request(), cfg, error_status));
        } else {
            std::string response;
            int32_t filefd = -1;

            if (dispatcher::open_static_file_response(
                    conn.request(), cfg, response, filefd)) {
                conn.enqueue_response(response);
                if (filefd != -1)
                    conn.enqueue_file(filefd);
            } else {
                conn.enqueue_response(
                    dispatcher::handle(conn.request(), conn.server()));
            }
        }
        conn.on_writable();
        if (events & EPOLLIN)
            update_source_events(fd, EPOLL_WRONLY);
    } else if (events & EPOLLOUT) {
        update_source_events(fd, EPOLL_RDONLY);
    }
}

const Server *EventLoop::select_server(
    const Server &default_server, const http::request &req) const
{
    std::string host = host_name(req);

    if (host.empty())
        return &default_server;
    for (std::size_t i = 0; i < _servers.size(); ++i) {
        if (_servers[i].listen_addr() != default_server.listen_addr())
            continue;
        const std::vector<std::string> &names
            = _servers[i].server_name_aliases();
        for (std::size_t j = 0; j < names.size(); ++j)
            if (lower_ascii(names[j]) == host)
                return &_servers[i];
    }
    return &default_server;
}

bool EventLoop::start_cgi_request(int32_t clientfd, Connection &conn,
    const Config &cfg, const std::string &script_path,
    http::status::type &error_status)
{
    cgi::Process process;
    cgi::start::result result;

    result = _cgi_lifecycle.start_request(
        clientfd, conn.request(), cfg, script_path, process);
    if (result != cgi::start::STARTED) {
        if (result == cgi::start::NOT_FOUND)
            error_status = http::status::NOT_FOUND;
        else if (result == cgi::start::FORBIDDEN)
            error_status = http::status::FORBIDDEN;
        else
            error_status = http::status::BAD_GATEWAY;
        return false;
    }
    if (!add_source(process.stdout_fd, EPOLL_RDONLY,
            EventSource(EventSource::SOURCE_CGI_STDOUT))) {
        cleanup_cgi_job(process.stdout_fd, cgi::job_cleanup::ABORT);
        return false;
    }
    if (!add_source(process.stdin_fd, EPOLL_WRONLY,
            EventSource(EventSource::SOURCE_CGI_STDIN))) {
        cleanup_cgi_job(process.stdout_fd, cgi::job_cleanup::ABORT);
        return false;
    }
    conn.wait_for_cgi();
    update_source_events(clientfd, EPOLL_RDONLY);
    return true;
}

void EventLoop::close_cgi_fd(int32_t fd)
{
    if (fd == -1)
        return;
    if (_sources.find(fd) != _sources.end())
        remove_source(fd);
    close(fd);
}

cgi::CleanupResult EventLoop::cleanup_cgi_job(
    int32_t stdout_fd, cgi::job_cleanup::action action)
{
    cgi::CleanupResult result
        = _cgi_lifecycle.cleanup_request(stdout_fd, action);

    close_cgi_fd(result.stdout_fd);
    close_cgi_fd(result.stdin_fd);
    return result;
}

void EventLoop::expire_cgi_jobs()
{
    std::vector<int32_t> expired = _cgi_lifecycle.expire_jobs();

    for (std::size_t i = 0; i < expired.size(); ++i)
        finish_cgi_job(expired[i]);
}

void EventLoop::finish_cgi_job(int32_t fd)
{
    cgi::CleanupResult result = cleanup_cgi_job(fd, cgi::job_cleanup::COMPLETE);

    if (!result.found)
        return;
    const cgi::CompletionResult &completion = result.completion;
    std::map<int32_t, Connection>::iterator conn_it
        = _connections.find(completion.clientfd);
    if (conn_it == _connections.end())
        return;
    if (completion.failed) {
        const Config &cfg = dispatcher::config_for(
            completion.request, conn_it->second.server());

        conn_it->second.enqueue_response(dispatcher::error_response(
            completion.request, cfg, completion.failure_status));
    } else {
        conn_it->second.enqueue_response(
            cgi::translate_output(completion.output, completion.request));
    }
    update_source_events(completion.clientfd, EPOLL_WRONLY);
}

void EventLoop::cancel_cgi_jobs_for(int32_t clientfd)
{
    std::vector<int32_t> jobs_to_cancel
        = _cgi_lifecycle.jobs_to_cancel_for(clientfd);

    for (std::size_t i = 0; i < jobs_to_cancel.size(); ++i)
        cleanup_cgi_job(jobs_to_cancel[i], cgi::job_cleanup::ABORT);
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
                    update_source_events(fd, EPOLL_WRONLY);
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
