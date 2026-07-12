/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/02 19:52:44 by nlaporte          #+#    #+#             */
/*   Updated: 2026/07/12 10:27:08 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cgi.hpp"

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "dispatcher.hpp"
#include "http.hpp"
#include "logger.hpp"

cgi_worker Cgi::workers[CGI_WORKER] = { };

namespace {

void add_to_env(const std::string &val, char *env[])
{
    int env_size = 0;
    while (env_size < CGI_ENV_SIZE && env[env_size])
        env_size++;
    if (env_size >= CGI_ENV_SIZE)
        return;
    char *s = strndup(val.c_str(), val.size());
    if (!s)
        return;
    env[env_size] = s;
}

void set_env(const Config &cfg, const std::string &uri,
    const http::request &req, char *env[])
{
    std::stringstream ss;
    ss << req.body.size();

    char path_buf[2000];
    if (getcwd(path_buf, sizeof(path_buf)) == NULL)
        return;

    std::string script_file = std::string(path_buf) + uri;

    add_to_env("GATEWAY_INTERFACE=CGI/1.1", env);
    add_to_env("SERVER_PROTOCOL=HTTP/1.1", env);
    add_to_env("REDIRECT_STATUS=200", env);
    add_to_env(
        "REQUEST_METHOD=" + std::string(http::methods::strings[req.method]),
        env);
    add_to_env("SCRIPT_NAME=" + uri, env);
    add_to_env("SCRIPT_FILENAME=" + script_file, env);
    add_to_env("DOCUMENT_ROOT=" + std::string(path_buf), env);
    add_to_env("PATH_INFO=", env);
    add_to_env("QUERY_STRING=" + req.query, env);
    add_to_env("CONTENT_LENGTH=" + ss.str(), env);
    add_to_env("HTTP_HOST=" + *cfg.conf.listen.begin(), env);

    if (req.query.empty())
        add_to_env("REQUEST_URI=" + uri, env);
    else
        add_to_env("REQUEST_URI=" + uri + "?" + req.query, env);
    for (std::map<std::string, std::string>::const_iterator it
        = req.headers.begin();
        it != req.headers.end(); ++it) {
        std::string key = it->first;
        for (std::size_t i = 0; i < key.length(); ++i) {
            if (key[i] == '-')
                key[i] = '_';
            else if (key[i] >= 'a' && key[i] <= 'z')
                key[i] -= 32;
        }
        if (key == "CONTENT_TYPE")
            add_to_env("CONTENT_TYPE=" + it->second, env);
        else if (key == "CONTENT_LENGTH" || key == "TRANSFER_ENCODING")
            continue;
        else
            add_to_env("HTTP_" + key + "=" + it->second, env);
    }
    (void)cfg;
}

void free_env(char *env[])
{
    for (std::size_t i = 0; i < CGI_ENV_SIZE; i++) {
        if (!env[i])
            return;
        std::free(env[i]);
        env[i] = NULL;
    }
}

void child_part(const char *path, char *args[4], char *env[15], int pipes[2],
    int pipes_in[2], const cgi_state &state)
{
    if (dup2(pipes[1], STDOUT_FILENO) == -1) {
        close(pipes[0]);
        close(pipes[1]);
        if (state == WRITE) {
            close(pipes_in[0]);
            close(pipes_in[1]);
        }
        free_env(env);
        exit(1);
    }
    if (state == WRITE) {
        if (dup2(pipes_in[0], STDIN_FILENO) == -1) {
            close(pipes[0]);
            close(pipes[1]);
            close(pipes_in[0]);
            close(pipes_in[1]);
            free_env(env);
            exit(1);
        }
        close(pipes_in[0]);
        close(pipes_in[1]);
    }
    close(pipes[0]);
    close(pipes[1]);
    execve(path, args, env);
    free_env(env);
    exit(1);
}

std::string get_header_val(const std::string &str, const std::string &key)
{
    std::size_t i = str.find(key);
    if (i == std::string::npos)
        return "";
    i += key.size();
    while (i < str.size() && (str[i] == ' ' || str[i] == '\t'))
        i++;
    std::size_t end = str.find("\r\n", i);
    if (end == std::string::npos)
        end = str.size();
    return str.substr(i, end - i);
}

std::string get_header(const std::string &str)
{
    std::string result;
    std::size_t start = 0;

    while (start < str.size()) {
        std::size_t end = str.find("\r\n", start);
        if (end == std::string::npos)
            end = str.size();
        std::string line = str.substr(start, end - start);
        if (line.compare(0, 7, "Status:") != 0
            && line.compare(0, 7, "status:") != 0) {
            result += line;
            result += "\r\n";
        }
        if (end == str.size())
            break;
        start = end + 2;
    }
    return result;
}
}

bool Cgi::is_a_cgi(const std::string &uri)
{
    if (uri.empty())
        return false;

    char *p = std::strrchr(const_cast<char *>(uri.c_str()), '.');
    if (p) {
        if (std::strncmp(p, ".php", 4) == 0) {
            return true;
        }
    }
    return false;
}

cgi_worker *Cgi::get_worker(int32_t pid)
{
    for (int i = 0; i < CGI_WORKER; i++) {
        if (workers[i].state != NOTSET && workers[i].pid == pid) {
            return &workers[i];
        }
    }
    return 0;
}

void Cgi::create_worker(const int &epollfd, int32_t fd, const Config &cfg,
    const http::request &req, const std::string &uri, const char *path)
{
    cgi_worker *worker = 0;
    int pipes[2];
    int pipes_in[2];
    int i;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        L_ERROR("Cant signal");
        return;
    }
    for (i = 0; i < CGI_WORKER; i++) {
        if (workers[i].state == NOTSET) {
            worker = &workers[i];
            break;
        }
    }
    if (i == CGI_WORKER) {
        L_ERROR("Too much workers");
        return;
    }

    if (req.method == http::methods::GET) {
        worker->state = READ;
    } else {
        worker->state = WRITE;
        if (pipe(pipes_in) == -1) {
            *worker = cgi_worker();
            return;
        }
        fcntl(pipes_in[1], F_SETFL, O_NONBLOCK);
        worker->fd_in = pipes_in[1];
        worker->body = req.body;
        worker->strlength = req.body.length();
        worker->current = 0;
    }
    worker->fd_client = fd;

    if (pipe(pipes) == -1) {
        if (worker->state == WRITE) {
            close(pipes_in[0]);
            close(pipes_in[1]);
        }
        *worker = cgi_worker();
        return;
    }
    fcntl(pipes[0], F_SETFL, O_NONBLOCK);
    epoll_ctl(epollfd, EPOLL_CTL_DEL, worker->fd_client, 0);

    worker->fd_out = pipes[0];
    worker->pid = fork();

    if (worker->pid == -1) {
        close(pipes[0]);
        close(pipes[1]);
        if (worker->state == WRITE) {
            close(pipes_in[0]);
            close(pipes_in[1]);
        }
        *worker = cgi_worker();
        return;
    }
    if (worker->pid == 0) {
        char *args[4] = { };
        char *env[CGI_ENV_SIZE + 1] = { };
        args[0] = (char *)path;
        args[1] = (char *)&uri.c_str()[1];
        args[2] = (char *)req.query.c_str();
        set_env(cfg, uri, req, env);
        child_part(path, args, env, pipes, pipes_in, worker->state);
    } else {
        epoll_event ev = { };
        close(pipes[1]);
        ev.data.u64 = worker->fd_out + ((uint64_t)worker->pid << 32);
        ev.events = EPOLLIN;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, pipes[0], &ev);
        if (req.method == http::methods::POST) {
            ev = epoll_event();
            close(pipes_in[0]);
            ev.data.u64 = pipes_in[1] + ((uint64_t)worker->pid << 32);
            epoll_ctl(epollfd, EPOLL_CTL_DEL, worker->fd_client, 0);
            ev.events = EPOLLOUT;
            epoll_ctl(epollfd, EPOLL_CTL_ADD, pipes_in[1], &ev);
        }
    }
}

bool Cgi::parse_cgi_output(cgi_worker *worker)
{
    long code = read(worker->fd_out, worker->buf, CGI_BUFFER - 1);
    if (code <= 0)
        return false;
    worker->header.append(worker->buf, code);
    std::size_t end = worker->header.find("\r\n\r\n");
    if (end == std::string::npos)
        return false;
    end = worker->header.find("\r\n\r\n");
    std::string to_parse = worker->header.substr(0, end);
    std::string status = get_header_val(to_parse, "Status: ");
    std::string to_print;
    if (status.empty())
        status = "200 OK";
    to_print = "HTTP/1.1 " + status + "\r\n" + get_header(to_parse)
        + "Connection: close\r\n\r\n";
    write(worker->fd_client, to_print.c_str(), to_print.length());
    write(worker->fd_client, &worker->header[end + 4],
        worker->header.length() - (end + 4));
    worker->has_send_header = 1;
    return true;
}

void Cgi::handle_cgi(int32_t epollfd, int32_t fd, int32_t pid)
{
    cgi_worker *worker = get_worker(pid);
    if (worker == 0)
        return;

    unsigned int len_to_write;
    if (fd == worker->fd_out) {
        if (!worker->has_send_header) {
            if (!parse_cgi_output(worker))
                return;
        }
        while (
            (worker->buf_index = read(worker->fd_out, worker->buf, CGI_BUFFER))
            > 0)
            write(worker->fd_client, worker->buf, worker->buf_index);
        if (worker->buf_index == 0) {
            epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
            if (worker->fd_out > 0)
                close(worker->fd_out);
            if (worker->fd_in > 0)
                close(worker->fd_in);
            if (worker->fd_client > 0)
                close(worker->fd_client);
            kill(worker->pid, SIGKILL);
            waitpid(worker->pid, 0, 0);
            *worker = cgi_worker();
            return;
        }
    }

    if (fd == worker->fd_in) {
        len_to_write
            = std::min(worker->strlength - worker->current, (unsigned int)255);
        ssize_t code = write(worker->fd_in,
            &worker->body.c_str()[worker->current], len_to_write);
        if (code != -1)
            worker->current += code;
        if (worker->strlength <= worker->current) {
            worker->state = READ;
            epoll_ctl(epollfd, EPOLL_CTL_DEL, worker->fd_in, 0);
            close(worker->fd_in);
            worker->fd_in = -1;
        }
    }
}
