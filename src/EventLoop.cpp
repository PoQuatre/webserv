/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 19:07:46 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/20 17:44:25 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "EventLoop.hpp"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

#include "cgi.hpp"
#include "dispatcher.hpp"
#include "logger.hpp"

#define MAX_EVENTS 16
#define EPOLL_CLOSED (EPOLLERR | EPOLLHUP | EPOLLRDHUP)
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

bool reap_child(pid_t pid, int options)
{
    while (true) {
        pid_t waited = waitpid(pid, NULL, options);

        if (waited == pid || (waited == -1 && errno == ECHILD))
            return true;
        if (waited == 0)
            return false;
        if (errno != EINTR)
            return true;
    }
}

bool terminate_child(pid_t pid, int options)
{
    if (pid <= 0)
        return true;
    kill(pid, SIGKILL);
    return reap_child(pid, options);
}

pid_t wait_child_status(pid_t pid, int *status, int options)
{
    while (true) {
        pid_t waited = waitpid(pid, status, options);

        if (waited != -1 || errno != EINTR)
            return waited;
    }
}

uint64_t monotonic_millis()
{
    timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
        return 0;
    return (static_cast<uint64_t>(ts.tv_sec) * 1000)
        + static_cast<uint64_t>(ts.tv_nsec / 1000000);
}

http::status::type cgi_start_error(cgi::start::result result)
{
    if (result == cgi::start::NOT_FOUND)
        return http::status::NOT_FOUND;
    if (result == cgi::start::FORBIDDEN)
        return http::status::FORBIDDEN;
    return http::status::BAD_GATEWAY;
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
    , clientfd(-1)
{
}

EventLoop::EventSource::EventSource(const Server &server_context)
    : type(SOURCE_LISTENER)
    , server(&server_context)
    , connection(NULL)
    , clientfd(-1)
{
}

EventLoop::EventSource::EventSource(Connection &connection_context)
    : type(SOURCE_CLIENT)
    , server(NULL)
    , connection(&connection_context)
    , clientfd(connection_context.fd())
{
}

EventLoop::EventSource::EventSource(
    EventLoop::EventSource::Type cgi_type, int32_t owner_clientfd)
    : type(cgi_type)
    , server(NULL)
    , connection(NULL)
    , clientfd(owner_clientfd)
{
}

EventLoop::CgiCleanupResult::CgiCleanupResult()
    : child_ok(true)
    , found(false)
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
        reap_pending_children();
        expire_cgi_jobs();
        int32_t nfds
            = epoll_wait(_epollfd, events, MAX_EVENTS, cgi_epoll_timeout());
        if (nfds == -1) {
            if (errno == EINTR)
                continue;
            L_ERROR("Failed to wait for epoll: {}", strerror(errno));
            ok = false;
            break;
        }

        expire_cgi_jobs();
        reap_pending_children();
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
    while (!_cgi_jobs.empty())
        cleanup_cgi_job(_cgi_jobs.begin(), CGI_CLEANUP_ABORT);

    reap_pending_children();
    _pending_reaps.clear();

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
        process_cgi_stdin(fd, events);
        break;

    case EventSource::SOURCE_CGI_STDOUT:
        process_cgi_stdout(fd, events);
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
        dispatcher::Outcome outcome
            = dispatcher::handle(conn.request(), conn.server());

        if (conn.is_waiting_cgi())
            return;
        if (outcome.type == dispatcher::Outcome::CGI_REQUIRED) {
            http::status::type error_status = http::status::BAD_GATEWAY;

            if (start_cgi_request(fd, conn, *outcome.config,
                    outcome.filesystem_path, error_status))
                return;
            conn.enqueue_response(dispatcher::error_response(
                conn.request(), *outcome.config, error_status));
        } else {
            conn.enqueue_response(outcome.response);
        }
        conn.on_writable();
        if (events & EPOLLIN)
            update_source_events(fd, EPOLL_WRONLY);
    } else if (events & EPOLLOUT) {
        update_source_events(fd, EPOLL_RDONLY);
    }
}

bool EventLoop::start_cgi_request(int32_t clientfd, Connection &conn,
    const Config &cfg, const std::string &script_path,
    http::status::type &error_status)
{
    cgi::StartedRequest started;
    cgi::start::result result;

    result = cgi::Lifecycle::start_request(
        clientfd, conn.request(), cfg, script_path, started);
    if (result != cgi::start::STARTED) {
        error_status = cgi_start_error(result);
        return false;
    }
    _cgi_jobs[started.job.stdout_fd] = started.job;
    for (std::size_t i = 0; i < started.descriptors.size(); ++i) {
        const cgi::Descriptor &descriptor = started.descriptors[i];
        EventSource::Type source_type = EventSource::SOURCE_CGI_STDOUT;
        uint32_t events = EPOLL_RDONLY;

        if (descriptor.type == cgi::descriptor::CGI_STDIN) {
            source_type = EventSource::SOURCE_CGI_STDIN;
            events = EPOLL_WRONLY;
        }
        if (!add_source(
                descriptor.fd, events, EventSource(source_type, clientfd))) {
            cleanup_cgi_job(
                _cgi_jobs.find(started.job.stdout_fd), CGI_CLEANUP_ABORT);
            return false;
        }
    }
    conn.wait_for_cgi();
    update_source_events(clientfd, EPOLL_RDONLY);
    return true;
}

void EventLoop::process_cgi_stdin(int32_t fd, uint32_t events)
{
    for (std::map<int32_t, cgi::Job>::iterator it = _cgi_jobs.begin();
        it != _cgi_jobs.end(); ++it) {
        if (it->second.stdin_fd == fd) {
            cgi::Job &job = it->second;
            const std::string &body = job.request.body;

            if (events & EPOLLERR)
                job.failed = true;
            if (job.body_written < body.size()) {
                ssize_t n = write(fd, body.c_str() + job.body_written,
                    body.size() - job.body_written);
                if (n > 0)
                    job.body_written += static_cast<std::size_t>(n);
                else if (!(events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)))
                    return;
            }
            if (job.body_written < body.size() && !job.failed)
                return;
            close_cgi_fd(job.stdin_fd);
            return;
        }
    }
}

void EventLoop::process_cgi_stdout(int32_t fd, uint32_t events)
{
    std::map<int32_t, cgi::Job>::iterator it = _cgi_jobs.find(fd);
    char buffer[4096];
    ssize_t bytes_read;
    bool done = false;

    if (it == _cgi_jobs.end())
        return;
    bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        if (it->second.max_output != 0
            && it->second.output.size() + static_cast<std::size_t>(bytes_read)
                > it->second.max_output) {
            it->second.failed = true;
            it->second.failure_status = http::status::BAD_GATEWAY;
            done = true;
        } else {
            it->second.output.append(
                buffer, static_cast<std::size_t>(bytes_read));
        }
    } else if (bytes_read == 0) {
        done = true;
    } else if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        if (events & EPOLLERR)
            it->second.failed = true;
        done = true;
    }
    if (done)
        finish_cgi_job(fd);
}

int32_t EventLoop::cgi_epoll_timeout() const
{
    uint64_t now;
    uint64_t nearest = 0;

    if (!_pending_reaps.empty())
        return 0;
    if (_cgi_jobs.empty())
        return -1;
    now = monotonic_millis();
    for (std::map<int32_t, cgi::Job>::const_iterator it = _cgi_jobs.begin();
        it != _cgi_jobs.end(); ++it) {
        if (it->second.deadline_millis <= now)
            return 0;
        if (nearest == 0 || it->second.deadline_millis < nearest)
            nearest = it->second.deadline_millis;
    }
    return static_cast<int32_t>(
        std::min(nearest - now, static_cast<uint64_t>(INT32_MAX)));
}

void EventLoop::reap_pending_children()
{
    for (std::vector<pid_t>::iterator it = _pending_reaps.begin();
        it != _pending_reaps.end();) {
        if (reap_child(*it, WNOHANG))
            it = _pending_reaps.erase(it);
        else
            ++it;
    }
}

void EventLoop::reap_child_later(pid_t pid)
{
    if (pid <= 0)
        return;
    if (std::find(_pending_reaps.begin(), _pending_reaps.end(), pid)
        == _pending_reaps.end())
        _pending_reaps.push_back(pid);
}

void EventLoop::terminate_child_nonblocking(pid_t pid)
{
    if (!terminate_child(pid, WNOHANG))
        reap_child_later(pid);
}

void EventLoop::close_cgi_fd(int32_t &fd)
{
    if (fd == -1)
        return;
    if (_sources.find(fd) != _sources.end())
        remove_source(fd);
    close(fd);
    fd = -1;
}

EventLoop::CgiCleanupResult EventLoop::cleanup_cgi_job(
    std::map<int32_t, cgi::Job>::iterator job_it,
    EventLoop::CgiCleanupAction action)
{
    CgiCleanupResult result;
    int32_t stdout_fd;
    int32_t stdin_fd;
    int status;
    pid_t waited;

    if (job_it == _cgi_jobs.end())
        return result;
    result.found = true;
    result.job = job_it->second;
    stdout_fd = job_it->first;
    stdin_fd = result.job.stdin_fd;
    _cgi_jobs.erase(job_it);
    close_cgi_fd(stdout_fd);
    close_cgi_fd(stdin_fd);
    if (action == CGI_CLEANUP_ABORT) {
        terminate_child_nonblocking(result.job.pid);
        return result;
    }
    if (result.job.pid <= 0)
        return result;
    status = 0;
    waited = wait_child_status(result.job.pid, &status, WNOHANG);
    result.child_ok = waited != -1 || errno == ECHILD;
    if (waited == result.job.pid)
        result.child_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    else if (waited == 0)
        terminate_child_nonblocking(result.job.pid);
    return result;
}

void EventLoop::expire_cgi_jobs()
{
    uint64_t now = monotonic_millis();

    for (std::map<int32_t, cgi::Job>::iterator it = _cgi_jobs.begin();
        it != _cgi_jobs.end();) {
        int32_t fd = it->first;

        if (it->second.deadline_millis > now) {
            ++it;
            continue;
        }
        it->second.failed = true;
        it->second.failure_status = http::status::GATEWAY_TIMEOUT;
        ++it;
        finish_cgi_job(fd);
    }
}

void EventLoop::finish_cgi_job(int32_t fd)
{
    CgiCleanupResult result;

    result = cleanup_cgi_job(_cgi_jobs.find(fd), CGI_CLEANUP_COMPLETE);
    if (!result.found)
        return;
    if (result.job.stdin_fd != -1
        && result.job.body_written < result.job.request.body.size())
        result.job.failed = true;
    std::map<int32_t, Connection>::iterator conn_it
        = _connections.find(result.job.clientfd);
    if (conn_it == _connections.end())
        return;
    if (result.job.failed || !result.child_ok) {
        const Config &cfg = dispatcher::config_for(
            result.job.request, conn_it->second.server());

        conn_it->second.enqueue_response(dispatcher::error_response(
            result.job.request, cfg, result.job.failure_status));
    } else {
        conn_it->second.enqueue_response(
            cgi::translate_output(result.job.output, result.job.request));
    }
    update_source_events(result.job.clientfd, EPOLL_WRONLY);
}

void EventLoop::cancel_cgi_jobs_for(int32_t clientfd)
{
    for (std::map<int32_t, cgi::Job>::iterator it = _cgi_jobs.begin();
        it != _cgi_jobs.end();) {
        if (it->second.clientfd == clientfd) {
            std::map<int32_t, cgi::Job>::iterator current = it++;

            cleanup_cgi_job(current, CGI_CLEANUP_ABORT);
        } else {
            ++it;
        }
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
