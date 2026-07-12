/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/02 19:36:38 by nlaporte          #+#    #+#             */
/*   Updated: 2026/07/12 10:26:43 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <sys/epoll.h>
#include <sys/types.h>

#include <string>

#include "Server.hpp"
#include "http.hpp"

#define CGI_WORKER 1024
#define CGI_BUFFER 1024
#define CGI_ENV_SIZE 200

typedef enum e_cgi_state { NOTSET = 0, INIT, WRITE, READ, END } cgi_state;

typedef struct s_cgi_worker {
    cgi_state state;
    int fd_in;
    int fd_client;
    int fd_out;
    bool has_send_header;
    pid_t pid;
    char buf[CGI_BUFFER];
    ssize_t buf_index;
    std::string body;
    std::string header;
    unsigned int strlength;
    unsigned int current;
} cgi_worker;

class Cgi {
public:
    static void handle_cgi(int32_t epollfd, int32_t fd, int32_t pid);
    static void create_worker(const int &epollfd, int32_t fd, const Config &cfg,
        const http::request &req, const std::string &uri, const char *path);
    static bool is_a_cgi(const std::string &uri);
    static bool parse_cgi_output(cgi_worker *worker);

private:
    static cgi_worker *get_worker(int32_t pid);
    static cgi_worker workers[CGI_WORKER];
};
