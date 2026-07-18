/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 06:06:28 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/18 21:23:10 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>
#include <sys/types.h>

#include <string>

#include "Server.hpp"
#include "http.hpp"

namespace cgi {

struct Process {
    pid_t pid;
    int32_t stdin_fd;
    int32_t stdout_fd;
};

bool start_process(const http::request &req, const Config &cfg,
    const std::string &script_path, Process &process);

std::string translate_output(
    const std::string &output, const http::request &req);

}
