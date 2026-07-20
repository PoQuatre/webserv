/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 06:06:28 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/20 18:32:58 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>
#include <sys/types.h>

#include <map>
#include <string>
#include <vector>

#include "Server.hpp"
#include "http.hpp"

namespace cgi {

namespace start {

enum result {
    STARTED,
    NOT_FOUND,
    FORBIDDEN,
    BAD_GATEWAY,
};

}

struct Process {
    pid_t pid;
    int32_t stdin_fd;
    int32_t stdout_fd;
};

namespace descriptor {

enum type {
    CGI_STDIN,
    CGI_STDOUT,
};

}

struct Descriptor {
    Descriptor();
    Descriptor(descriptor::type descriptor_type, int32_t descriptor_fd);

    descriptor::type type;
    int32_t fd;
};

struct Job {
    Job();

    int32_t clientfd;
    pid_t pid;
    int32_t stdin_fd;
    int32_t stdout_fd;
    std::size_t body_written;
    std::size_t max_output;
    uint64_t deadline_millis;
    std::string output;
    http::request request;
    http::status::type failure_status;
    bool failed;
};

namespace readiness {

enum action {
    CONTINUE,
    CLOSE_STDIN,
    COMPLETE,
};

}

struct ReadinessResult {
    ReadinessResult();

    readiness::action action;
    int32_t descriptor_fd;
};

struct CompletionResult {
    CompletionResult();

    int32_t clientfd;
    http::request request;
    std::string output;
    http::status::type failure_status;
    bool failed;
};

namespace job_cleanup {

enum action {
    COMPLETE,
    ABORT,
};

}

struct CleanupResult {
    CleanupResult();

    Job job;
    bool child_ok;
};

struct StartedRequest {
    StartedRequest();

    start::result status;
    Job job;
    std::vector<Descriptor> descriptors;
};

class Lifecycle {
public:
    static start::result start_request(int32_t clientfd,
        const http::request &req, const Config &cfg,
        const std::string &script_path, StartedRequest &request);
    static ReadinessResult process_stdin(Job &job, uint32_t events);
    static ReadinessResult process_stdout(Job &job, uint32_t events);
    static CompletionResult complete(Job job, bool child_ok);

    int32_t wait_timeout(const std::map<int32_t, Job> &jobs) const;
    static std::vector<int32_t> expire_jobs(std::map<int32_t, Job> &jobs);
    std::vector<int32_t> jobs_to_cancel_for(
        int32_t clientfd, const std::map<int32_t, Job> &jobs) const;
    void reap_pending_children();
    CleanupResult cleanup(const Job &job, job_cleanup::action action);

private:
    void reap_child_later(pid_t pid);
    void terminate_child_nonblocking(pid_t pid);

    std::vector<pid_t> _pending_reaps;
};

cgi::start::result start_process(const http::request &req, const Config &cfg,
    const std::string &script_path, Process &process);

std::string translate_output(
    const std::string &output, const http::request &req);

}
