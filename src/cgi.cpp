/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/06 08:26:34 by nlaporte          #+#    #+#             */
/*   Updated: 2026/06/08 12:50:43 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cgi.hpp"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include <ostream>
#include <sstream>
#include <vector>

#include "Connection.hpp"
#include "dispatcher.hpp"
#include "http.hpp"
#include "logger.hpp"

std::vector<cgi_worker> Cgi::_workers;

Cgi::Cgi(const std::string &path)
    : _path(path)
    , _env()
    , _env_size(0)
{
}

Cgi::~Cgi()
{
    for (int i = 0; i < _env_size; i++) {
        free(_env[i]);
    }
}

void Cgi::add_to_env(const std::string &val)
{
    if (_env_size >= 50)
        return;
    char *s = strndup(val.c_str(), val.size());
    if (!s)
        return;
    _env[_env_size++] = s;
}

void Cgi::rewrite_env(const std::string &val, int i)
{
    if (i >= 50 || !_env[i])
        return;
    free(_env[i]);
    char *s = strndup(val.c_str(), val.size());
    if (!s)
        return;
    _env[i] = s;
}

void Cgi::set_env(
    const Config &cfg, const std::string &uri, const http::request &req)
{
    // MUST
    std::stringstream ss;
    ss << req.body.size();
    (void)cfg;
    (void)uri;

    // NOTE: can be switch string view
    std::string filename = uri.substr(cfg.root.size(), uri.length());

    // FIX:
    this->add_to_env("CONTENT_TYPE=text/plain");

    this->add_to_env("SCRIPT_NAME=" + filename);
    this->add_to_env("SCRIPT_FILENAME=" + uri);
    this->add_to_env("PATH_INFO=");
    this->add_to_env("CONTENT_LENGTH=" + ss.str());
    this->add_to_env("QUERY_STRING=" + req.query);
    this->add_to_env("REQUEST_URI=" + filename + "?" + req.query);
    this->add_to_env("SERVER_NAME=" + cfg.server_name);
    /*
    this->add_to_env("GATEWAY_INTERFACE=PHP/8.5");
    this->add_to_env("SERVER_PROTOCOL=HTTP/1.1");
    this->add_to_env("REMOTE_ADDR=");
    this->add_to_env("REMOTE_USER=");
    this->add_to_env("SERVER_PORT=");
    */

    // MUST IF
    /*
    this->add_to_env("AUTH_TYPE=NONE");
    this->add_to_env("PATH_INFO=");
    this->add_to_env("QUERY_STRING=");
    */

    // MAY
    /*
    this->add_to_env("REMOTE_HOST=");
    this->add_to_env("REQUEST_METHOD=GET");
    */
}

std::string Cgi::handle_cgi(Cgi cgi, const http::request &req,
    const Config &cfg, const std::string &uri, const int &epollfd,
    const int &clientfd)
{
    (void)req;
    int pipes[2];
    if (pipe(pipes) == -1) {
        // L_ERROR("MERDE ALORS peux pas pipe");
        return dispatcher::error_response(http::status::INTERNAL_SERVER_ERROR);
    }
    pid_t pid = fork();
    (void)cfg;
    if (pid == -1) {
        L_ERROR("MERDE ALORS peux pas fork");
        return dispatcher::error_response(http::status::INTERNAL_SERVER_ERROR);
    }
    if (pid == 0) {
        char *args[4];
        args[0] = (char *)(cgi.get_path());
        args[1] = (char *)uri.c_str();
        args[2] = (char *)req.query.c_str();
        args[3] = 0;
        cgi.set_env(cfg, uri, req);
        if (dup2(pipes[1], STDOUT_FILENO) == -1) {
            close(pipes[0]);
            close(pipes[1]);
            return dispatcher::error_response(
                http::status::INTERNAL_SERVER_ERROR);
        }
        close(pipes[0]);
        close(pipes[1]);
        if (execve(cgi.get_path(), args, cgi.get_env()) == -1) {
            L_ERROR("CANT EXECVE");
        }
        exit(1);
    } else {
        struct epoll_event ev = { };
        ev.events = EPOLLIN;
        close(pipes[1]);
        fcntl(pipes[0], F_SETFL, O_NONBLOCK);
        //  32 bits for client fd, 31bits for cgifd 2^31 > INT_MAX, use this bit
        //  for write header flag
        ev.data.u64 = (uint32_t)clientfd | ((uint64_t)pipes[0] << 32)
            | ((uint64_t)0 << 63);
        _workers.push_back((cgi_worker) { clientfd, pid, &cgi });
        epoll_ctl(epollfd, EPOLL_CTL_ADD, pipes[0], &ev);
    }
    return "";
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

char **Cgi::get_env() { return _env; }

void Cgi::process(int32_t epollfd, int32_t fd, int32_t cgifd, epoll_event *ev)
{
    bool header_is_send = (ev->data.u64 >> 63);
    char buf[10];

    if (header_is_send == 0) {
        const char h[] = "HTTP/1.1 200 OK\n";
        write(fd, h, strlen(h));
        ev->data.u64 |= (1ULL << 63);
        epoll_ctl(epollfd, EPOLL_CTL_MOD, cgifd, ev);
    }

    ssize_t i = 0;
    while ((i = read(cgifd, buf, sizeof(buf))) > 0) {
        write(fd, buf, i);
    }
    if (i == -1)
        return;
    if (i == 0) {
        for (std::vector<cgi_worker>::iterator it = _workers.begin();
            it != _workers.end(); it++) {
            if ((*it).client_fd == fd) {
                waitpid((*it).worker, 0, 0);
                epoll_ctl(epollfd, EPOLL_CTL_DEL, cgifd, ev);
                close(fd);
                close(cgifd);
                _workers.erase(it);
                break;
            }
        }
    }
}
