/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/06 08:25:18 by nlaporte          #+#    #+#             */
/*   Updated: 2026/06/08 09:57:11 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <fcntl.h>
#include <sys/epoll.h>

#include <cstring>
#include <string>
#include <vector>

#include "http.hpp"

struct Config;
class Connection;

struct CgiType {
    const char *ext;
    const char *type;
};

const CgiType CgiList[] = {
    { ".php", "PHP" },
    { NULL, NULL },
};

class Cgi {
public:
    Cgi(const std::string &path);
    void add_to_env(const std::string &val);
    void rewrite_env(const std::string &val, int i);
    char **get_env();
    void start_work();
    void set_env(
        const Config &cgf, const std::string &uri, const http::request &req);
    const char *get_path() { return this->_path.c_str(); }
    static std::string handle_cgi(Cgi cgi, const http::request &req,
        const Config &cfg, const std::string &uri, const int &epollfd,
        const int &clientfd);
    static bool is_a_cgi(const std::string &uri);
    static void process(
        int32_t epollfd, int32_t fd, int32_t cgifd, epoll_event *ev);
    ~Cgi();

private:
    std::string _path;
    char *_env[50];
    int _env_size;
    static std::vector<struct cgi_worker> _workers;
};

struct cgi_worker {
    int client_fd;
    pid_t worker;
    Cgi *cgi;
};
