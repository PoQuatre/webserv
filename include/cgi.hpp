/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 06:06:28 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/26 10:19:41 by mle-flem         ###   ########.fr       */
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

    CompletionResult completion;
    int32_t stdin_fd;
    int32_t stdout_fd;
    bool found;
};

struct StartedRequest {
    StartedRequest();

    start::result status;
    int32_t stdin_fd;
    int32_t stdout_fd;
};

class Lifecycle {
public:
    start::result start_request(int32_t clientfd, const http::request &req,
        const Config &cfg, const std::string &script_path,
        StartedRequest &request);
    ReadinessResult process_stdin(int32_t fd, uint32_t events);
    ReadinessResult process_stdout(int32_t fd, uint32_t events);
    int32_t wait_timeout() const;
    std::vector<int32_t> expire_jobs();
    std::vector<int32_t> jobs_to_cancel_for(int32_t clientfd) const;
    CleanupResult cleanup_request(
        int32_t stdout_fd, job_cleanup::action action);
    std::vector<int32_t> abort_all_requests();
    void reap_pending_children();

private:
    static ReadinessResult process_stdin(Job &job, uint32_t events);
    static ReadinessResult process_stdout(Job &job, uint32_t events);
    static CompletionResult complete(Job job, bool child_ok);
    bool cleanup_child(const Job &job, job_cleanup::action action);
    void reap_child_later(pid_t pid);
    void terminate_child_nonblocking(pid_t pid);

    std::map<int32_t, Job> _jobs;
    std::vector<pid_t> _pending_reaps;
};

cgi::start::result start_process(const http::request &req, const Config &cfg,
    const std::string &script_path, Process &process);

std::string translate_output(
    const std::string &output, const http::request &req);

}
