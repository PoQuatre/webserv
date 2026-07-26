/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi_response.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 06:06:28 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/26 10:32:43 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "cgi.hpp"
#include "http.hpp"

static http::request make_req(http::methods::type method)
{
    http::request req = { };

    req.method = method;
    req.version = http::versions::HTTP11;
    req.keep_alive = true;
    return req;
}

static std::string make_tmpdir()
{
    char tmpl[] = "/tmp/webserv-cgi-env-test-XXXXXX";
    char *dir = mkdtemp(tmpl);

    cr_assert_not_null(dir, "mkdtemp() failed: %s", strerror(errno));
    return dir;
}

static void write_file(const std::string &path, const std::string &content)
{
    std::ofstream out(path.c_str(), std::ios::binary);

    cr_assert(out.is_open(), "failed to open %s", path.c_str());
    out << content;
    cr_assert(!out.fail(), "failed to write %s", path.c_str());
}

static std::string read_all(int fd)
{
    std::string out;
    char buffer[256];

    while (true) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n > 0) {
            out.append(buffer, static_cast<std::size_t>(n));
        } else if (n == 0) {
            break;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            cr_assert_fail("read() failed: %s", strerror(errno));
        } else {
            usleep(1000);
        }
    }
    return out;
}

static void assert_contains(const std::string &haystack, const char *needle)
{
    cr_assert_neq(haystack.find(needle), std::string::npos,
        "missing '%s' in output:\n%s", needle, haystack.c_str());
}

static void assert_not_contains(const std::string &haystack, const char *needle)
{
    cr_assert_eq(haystack.find(needle), std::string::npos,
        "unexpected '%s' in output:\n%s", needle, haystack.c_str());
}

static bool process_is_gone(pid_t pid)
{
    return kill(pid, 0) == -1 && errno == ESRCH;
}

static pid_t wait_for_pid_file(const std::string &path)
{
    for (int attempts = 0; attempts < 1000; ++attempts) {
        std::ifstream in(path.c_str());
        long pid = 0;

        if (in >> pid && pid > 0)
            return static_cast<pid_t>(pid);
        usleep(1000);
    }
    cr_assert_fail("timed out waiting for pid file %s", path.c_str());
    return -1;
}

static void close_cleanup_descriptors(const cgi::CleanupResult &cleanup)
{
    if (cleanup.stdout_fd != -1)
        close(cleanup.stdout_fd);
    if (cleanup.stdin_fd != -1)
        close(cleanup.stdin_fd);
}

static void start_lifecycle_script(cgi::Lifecycle &lifecycle, int32_t clientfd,
    const http::request &req, const Config &cfg, const std::string &script_body,
    cgi::StartedRequest &started)
{
    std::string root = make_tmpdir();
    std::string script = root + "/script.sh";

    write_file(script, script_body);
    cr_assert_eq(lifecycle.start_request(clientfd, req, cfg, script, started),
        cgi::start::STARTED);
}

static void close_lifecycle_stdin(
    cgi::Lifecycle &lifecycle, const cgi::StartedRequest &started)
{
    cgi::ReadinessResult result = lifecycle.process_stdin(started.stdin_fd, 0);

    cr_assert_eq(result.action, cgi::readiness::CLOSE_STDIN);
    close(result.descriptor_fd);
}

static cgi::CleanupResult finish_lifecycle_request(
    cgi::Lifecycle &lifecycle, int32_t stdout_fd)
{
    for (int attempts = 0; attempts < 1000; ++attempts) {
        cgi::ReadinessResult result = lifecycle.process_stdout(stdout_fd, 0);

        if (result.action == cgi::readiness::COMPLETE) {
            cgi::CleanupResult cleanup = lifecycle.cleanup_request(
                stdout_fd, cgi::job_cleanup::COMPLETE);

            close_cleanup_descriptors(cleanup);
            return cleanup;
        }
        usleep(1000);
    }
    cr_assert_fail("timed out waiting for CGI lifecycle completion");
    return cgi::CleanupResult();
}

static void start_lifecycle_request(cgi::Lifecycle &lifecycle, int32_t clientfd,
    uint32_t timeout, cgi::StartedRequest &started)
{
    http::request req = make_req(http::methods::GET);
    Config cfg = { };

    req.uri = "/cgi/wait.sh";
    cfg.cgi_pass = "/bin/sh";
    cfg.cgi_timeout = timeout;
    cfg.cgi_output_buffer_size = 4096;
    start_lifecycle_script(lifecycle, clientfd, req, cfg,
        "trap 'exit 0' TERM\n"
        "sleep 10\n"
        "printf 'Content-Type: text/plain\\r\\n\\r\\nlate\\n'\n",
        started);
}

static void assert_bad_gateway(const std::string &response)
{
    const char *status = "HTTP/1.1 502 Bad Gateway\r\n";

    cr_assert(strncmp(response.c_str(), status, strlen(status)) == 0);
}

Test(cgi_response, parsed_headers_become_http_response)
{
    std::string out = cgi::translate_output(
        "Content-Type: text/plain\r\nX-CGI: yes\r\n\r\nhello",
        make_req(http::methods::GET));

    cr_assert_str_eq(out.c_str(),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "X-CGI: yes\r\n"
        "Content-Length: 5\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        "hello");
}

Test(cgi_response, header_only_output_with_separator_becomes_empty_response)
{
    std::string out = cgi::translate_output(
        "Content-Type: text/plain\r\n\r\n", make_req(http::methods::GET));

    cr_assert_str_eq(out.c_str(),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 0\r\n"
        "Connection: keep-alive\r\n"
        "\r\n");
}

Test(cgi_response, status_header_controls_status_line)
{
    std::string out = cgi::translate_output(
        "Status: 201 Created\r\nContent-Type: text/plain\r\n\r\nmade",
        make_req(http::methods::GET));

    cr_assert(strncmp(out.c_str(), "HTTP/1.1 201 Created\r\n", 22) == 0);
    cr_assert_eq(out.find("Status:"), std::string::npos);
}

Test(cgi_response, parsed_output_filters_unsafe_headers)
{
    std::string out = cgi::translate_output("Status: 201 Created\r\n"
                                            "Content-Type: text/plain\r\n"
                                            "Set-Cookie: session=abc\r\n"
                                            "X-App-Header: kept\r\n"
                                            "Content-Length: 999\r\n"
                                            "Connection: close, X-Hop\r\n"
                                            "X-Hop: dropped\r\n"
                                            "Transfer-Encoding: chunked\r\n"
                                            "\r\nbody\n",
        make_req(http::methods::GET));

    cr_assert_str_eq(out.c_str(),
        "HTTP/1.1 201 Created\r\n"
        "Content-Type: text/plain\r\n"
        "Set-Cookie: session=abc\r\n"
        "X-App-Header: kept\r\n"
        "Content-Length: 5\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        "body\n");
}

Test(cgi_response, location_header_defaults_to_302_redirect)
{
    std::string out = cgi::translate_output(
        "Location: /elsewhere\r\n\r\n", make_req(http::methods::GET));

    cr_assert_str_eq(out.c_str(),
        "HTTP/1.1 302 Moved Temporarily\r\n"
        "Location: /elsewhere\r\n"
        "Content-Length: 0\r\n"
        "Connection: keep-alive\r\n"
        "\r\n");
}

Test(cgi_response, nph_output_filters_unsafe_headers)
{
    std::string out = cgi::translate_output("HTTP/1.1 204 No Content\r\n"
                                            "X-NPH: yes\r\n"
                                            "Connection: close\r\n"
                                            "Transfer-Encoding: chunked\r\n"
                                            "\r\n",
        make_req(http::methods::GET));

    cr_assert_str_eq(out.c_str(),
        "HTTP/1.1 204 No Content\r\n"
        "X-NPH: yes\r\n"
        "Content-Length: 0\r\n"
        "Connection: keep-alive\r\n"
        "\r\n");
}

Test(cgi_response, headerless_output_becomes_bad_gateway)
{
    std::string out = cgi::translate_output(
        "<h1>Hello</h1>\n", make_req(http::methods::GET));

    assert_bad_gateway(out);
}

Test(cgi_response, header_like_output_without_separator_becomes_bad_gateway)
{
    std::string out = cgi::translate_output(
        "Content-Type text/plain\r\nhello", make_req(http::methods::GET));

    assert_bad_gateway(out);
    assert_not_contains(out, "hello");
}

Test(cgi_response, malformed_output_becomes_bad_gateway)
{
    std::string out = cgi::translate_output(
        "Content-Type text/plain\r\n\r\nhello", make_req(http::methods::GET));

    assert_bad_gateway(out);
    assert_not_contains(out, "hello");
}

Test(cgi_response, head_discards_body_and_preserves_content_length)
{
    std::string out = cgi::translate_output(
        "Content-Type: text/plain\r\n\r\nhello", make_req(http::methods::HEAD));

    cr_assert_str_eq(out.c_str(),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 5\r\n"
        "Connection: keep-alive\r\n"
        "\r\n");
}

Test(cgi_process, receives_cgi_meta_variables_in_clean_environment)
{
    std::string root = make_tmpdir();
    std::string script = root + "/env.sh";
    write_file(script,
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "env | sort\n");
    cr_assert_eq(
        chmod(script.c_str(), 0700), 0, "chmod() failed: %s", strerror(errno));

    http::request req = make_req(http::methods::GET);
    req.uri = "/cgi/env.sh";
    req.query_string = "raw=a%2Bb+c";
    req.remote_addr = "127.0.0.1";
    req.headers["host"] = "client.example:9999";
    req.headers["accept"] = "text/plain";
    req.headers["x-custom-header"] = "kept";
    req.headers["content-length"] = "123";
    req.headers["content-type"] = "text/plain; charset=utf-8";
    req.headers["transfer-encoding"] = "chunked";
    req.headers["connection"] = "keep-alive";
    req.headers["proxy"] = "http://attacker.example";

    setenv("WEBSERV_TEST_SHOULD_NOT_LEAK", "yes", 1);
    Config cfg = { };
    cfg.cgi_pass = "/bin/sh";
    cfg.conf.server_name.push_back("example.test");
    cfg.conf.listen.push_back("127.0.0.1:8080");
    cgi::Process process;
    cr_assert_eq(
        cgi::start_process(req, cfg, script, process), cgi::start::STARTED);
    close(process.stdin_fd);
    std::string output = read_all(process.stdout_fd);
    close(process.stdout_fd);
    int status = 0;
    cr_assert_eq(waitpid(process.pid, &status, 0), process.pid,
        "waitpid() failed: %s", strerror(errno));
    unsetenv("WEBSERV_TEST_SHOULD_NOT_LEAK");
    cr_assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    assert_contains(output, "GATEWAY_INTERFACE=CGI/1.1\n");
    assert_contains(output, "REQUEST_METHOD=GET\n");
    assert_contains(output, "QUERY_STRING=raw=a%2Bb+c\n");
    assert_contains(output, "SCRIPT_NAME=/cgi/env.sh\n");
    assert_contains(output, ("SCRIPT_FILENAME=" + script + "\n").c_str());
    assert_contains(output, "REMOTE_ADDR=127.0.0.1\n");
    assert_contains(output, "SERVER_NAME=client.example\n");
    assert_contains(output, "SERVER_PORT=8080\n");
    assert_contains(output, "SERVER_PROTOCOL=HTTP/1.1\n");
    assert_contains(output, "SERVER_SOFTWARE=webserv\n");
    assert_contains(output, "CONTENT_LENGTH=0\n");
    assert_contains(output, "CONTENT_TYPE=text/plain; charset=utf-8\n");
    assert_contains(output, "HTTP_ACCEPT=text/plain\n");
    assert_contains(output, "HTTP_HOST=client.example:9999\n");
    assert_contains(output, "HTTP_X_CUSTOM_HEADER=kept\n");
    assert_contains(output, "PATH=/usr/bin:/bin\n");
    assert_not_contains(output, "HTTP_CONTENT_LENGTH=");
    assert_not_contains(output, "HTTP_CONTENT_TYPE=");
    assert_not_contains(output, "HTTP_TRANSFER_ENCODING=");
    assert_not_contains(output, "HTTP_CONNECTION=");
    assert_not_contains(output, "HTTP_PROXY=");
    assert_not_contains(output, "WEBSERV_TEST_SHOULD_NOT_LEAK=");
}

Test(cgi_process,
    runs_readable_script_without_executable_bit_through_interpreter)
{
    std::string root = make_tmpdir();
    std::string script = root + "/not-executable.sh";
    write_file(script,
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'script-ran\\n'\n");
    cr_assert_eq(
        chmod(script.c_str(), 0600), 0, "chmod() failed: %s", strerror(errno));

    http::request req = make_req(http::methods::GET);
    req.uri = "/cgi/not-executable.sh";
    Config cfg = { };
    cfg.cgi_pass = "/bin/sh";
    cgi::Process process;
    cr_assert_eq(
        cgi::start_process(req, cfg, script, process), cgi::start::STARTED);
    close(process.stdin_fd);
    std::string output = read_all(process.stdout_fd);
    close(process.stdout_fd);
    int status = 0;
    cr_assert_eq(waitpid(process.pid, &status, 0), process.pid,
        "waitpid() failed: %s", strerror(errno));

    cr_assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert_contains(output, "script-ran\n");
}

Test(cgi_process, runs_script_from_script_directory)
{
    std::string root = make_tmpdir();
    std::string script = root + "/read-neighbor.sh";
    write_file(root + "/data.txt", "neighbor-data\n");
    write_file(script,
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "cat data.txt\n");
    cr_assert_eq(
        chmod(script.c_str(), 0600), 0, "chmod() failed: %s", strerror(errno));

    http::request req = make_req(http::methods::GET);
    req.uri = "/cgi/read-neighbor.sh";
    Config cfg = { };
    cfg.cgi_pass = "/bin/sh";
    cgi::Process process;
    cr_assert_eq(
        cgi::start_process(req, cfg, script, process), cgi::start::STARTED);
    close(process.stdin_fd);
    std::string output = read_all(process.stdout_fd);
    close(process.stdout_fd);
    int status = 0;
    cr_assert_eq(waitpid(process.pid, &status, 0), process.pid,
        "waitpid() failed: %s", strerror(errno));

    cr_assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert_contains(output, "neighbor-data\n");
}

Test(cgi_process, resolves_relative_interpreter_before_script_chdir)
{
    char original_cwd[4096];
    std::string root = make_tmpdir();
    std::string script_dir = root + "/cgi-bin";
    std::string script_rel = "cgi-bin/read-neighbor.sh";
    std::string script = script_dir + "/read-neighbor.sh";

    cr_assert_not_null(getcwd(original_cwd, sizeof(original_cwd)),
        "getcwd() failed: %s", strerror(errno));
    cr_assert_eq(mkdir(script_dir.c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
    cr_assert_eq(symlink("/bin/sh", (root + "/sh").c_str()), 0,
        "symlink() failed: %s", strerror(errno));
    write_file(script_dir + "/data.txt", "relative-interpreter\n");
    write_file(script,
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'SCRIPT_FILENAME=%s\\n' \"$SCRIPT_FILENAME\"\n"
        "cat data.txt\n");
    cr_assert_eq(
        chmod(script.c_str(), 0600), 0, "chmod() failed: %s", strerror(errno));

    http::request req = make_req(http::methods::GET);
    req.uri = "/cgi/read-neighbor.sh";
    Config cfg = { };
    cfg.cgi_pass = "./sh";
    cgi::Process process;
    cr_assert_eq(chdir(root.c_str()), 0, "chdir() failed: %s", strerror(errno));
    cgi::start::result result
        = cgi::start_process(req, cfg, script_rel, process);
    cr_assert_eq(
        chdir(original_cwd), 0, "chdir() restore failed: %s", strerror(errno));
    cr_assert_eq(result, cgi::start::STARTED);
    close(process.stdin_fd);
    std::string output = read_all(process.stdout_fd);
    close(process.stdout_fd);
    int status = 0;
    cr_assert_eq(waitpid(process.pid, &status, 0), process.pid,
        "waitpid() failed: %s", strerror(errno));

    cr_assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert_contains(output, ("SCRIPT_FILENAME=" + script + "\n").c_str());
    assert_contains(output, "relative-interpreter\n");
}

Test(cgi_process, rejects_missing_interpreter_path)
{
    std::string root = make_tmpdir();
    std::string script = root + "/hello.sh";
    write_file(script,
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'hello\\n'\n");

    http::request req = make_req(http::methods::GET);
    req.uri = "/cgi/hello.sh";
    Config cfg = { };
    cfg.cgi_pass = root + "/missing-interpreter";
    cgi::Process process;

    cr_assert_eq(
        cgi::start_process(req, cfg, script, process), cgi::start::BAD_GATEWAY);
}

Test(cgi_process, reports_script_error_before_interpreter_error)
{
    std::string root = make_tmpdir();
    http::request req = make_req(http::methods::GET);
    req.uri = "/cgi/missing.sh";
    Config cfg = { };
    cfg.cgi_pass = root + "/missing-interpreter";
    cgi::Process process;

    cr_assert_eq(cgi::start_process(req, cfg, root + "/missing.sh", process),
        cgi::start::NOT_FOUND);
}

Test(cgi_lifecycle, start_request_reports_descriptors_to_monitor)
{
    std::string root = make_tmpdir();
    std::string script = root + "/hello.sh";
    write_file(script,
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "printf 'hello-lifecycle\\n'\n");
    cr_assert_eq(
        chmod(script.c_str(), 0600), 0, "chmod() failed: %s", strerror(errno));

    http::request req = make_req(http::methods::GET);
    req.uri = "/cgi/hello.sh";
    Config cfg = { };
    cfg.cgi_pass = "/bin/sh";
    cfg.cgi_timeout = 1;
    cfg.cgi_output_buffer_size = 4096;
    cgi::Lifecycle lifecycle;
    cgi::StartedRequest started;

    cr_assert_eq(lifecycle.start_request(42, req, cfg, script, started),
        cgi::start::STARTED);
    cr_assert_neq(started.stdin_fd, -1);
    cr_assert_neq(started.stdout_fd, -1);

    cgi::ReadinessResult stdin_result
        = lifecycle.process_stdin(started.stdin_fd, 0);
    cr_assert_eq(stdin_result.action, cgi::readiness::CLOSE_STDIN);
    close(stdin_result.descriptor_fd);
    std::string output = read_all(started.stdout_fd);
    cgi::CleanupResult cleanup = lifecycle.cleanup_request(
        started.stdout_fd, cgi::job_cleanup::COMPLETE);
    close_cleanup_descriptors(cleanup);
    assert_contains(output, "hello-lifecycle\n");
}

Test(cgi_lifecycle, stdin_readiness_writes_body_and_reports_fd_to_close)
{
    cgi::Lifecycle lifecycle;
    http::request req = make_req(http::methods::POST);
    Config cfg = { };
    cgi::StartedRequest started;
    cgi::CleanupResult cleanup;
    int32_t stdout_fd;

    req.uri = "/cgi/body.sh";
    req.body = "alpha=one&beta=two";
    cfg.cgi_pass = "/bin/sh";
    cfg.cgi_timeout = DEFAULT_CGI_TIMEOUT;
    cfg.cgi_output_buffer_size = 4096;
    start_lifecycle_script(lifecycle, 77, req, cfg,
        "printf 'Content-Type: text/plain\\r\\n\\r\\n'\n"
        "body=$(dd bs=1 count=18 2>/dev/null)\n"
        "printf '%s' \"$body\"\n",
        started);
    stdout_fd = started.stdout_fd;

    close_lifecycle_stdin(lifecycle, started);
    cleanup = finish_lifecycle_request(lifecycle, stdout_fd);
    cr_assert_not(cleanup.completion.failed);
    cr_assert_str_eq(cleanup.completion.output.c_str(),
        "Content-Type: text/plain\r\n\r\nalpha=one&beta=two");
}

Test(cgi_lifecycle, stdout_readiness_accumulates_output_and_reports_completion)
{
    cgi::Lifecycle lifecycle;
    const char *output = "Content-Type: text/plain\r\n\r\nhello";
    http::request req = make_req(http::methods::GET);
    Config cfg = { };
    cgi::StartedRequest started;
    cgi::CleanupResult cleanup;
    int32_t stdout_fd;

    req.uri = "/cgi/output.sh";
    cfg.cgi_pass = "/bin/sh";
    cfg.cgi_timeout = DEFAULT_CGI_TIMEOUT;
    cfg.cgi_output_buffer_size = 4096;
    start_lifecycle_script(lifecycle, 88, req, cfg,
        "printf 'Content-Type: text/plain\\r\\n\\r\\nhello'\n", started);
    stdout_fd = started.stdout_fd;

    close_lifecycle_stdin(lifecycle, started);
    cleanup = finish_lifecycle_request(lifecycle, stdout_fd);
    cr_assert_not(cleanup.completion.failed);
    cr_assert_str_eq(cleanup.completion.output.c_str(), output);
}

Test(cgi_lifecycle, stdout_readiness_enforces_output_limit)
{
    cgi::Lifecycle lifecycle;
    http::request req = make_req(http::methods::GET);
    Config cfg = { };
    cgi::StartedRequest started;
    cgi::CleanupResult cleanup;
    int32_t stdout_fd;

    req.uri = "/cgi/large.sh";
    cfg.cgi_pass = "/bin/sh";
    cfg.cgi_timeout = DEFAULT_CGI_TIMEOUT;
    cfg.cgi_output_buffer_size = 4;
    start_lifecycle_script(
        lifecycle, 99, req, cfg, "printf 'too-large'\n", started);
    stdout_fd = started.stdout_fd;

    close_lifecycle_stdin(lifecycle, started);
    cleanup = finish_lifecycle_request(lifecycle, stdout_fd);
    cr_assert(cleanup.completion.failed);
    cr_assert_eq(cleanup.completion.failure_status, http::status::BAD_GATEWAY);
    cr_assert(cleanup.completion.output.empty());
}

Test(cgi_lifecycle, completion_identifies_waiting_client)
{
    cgi::Lifecycle lifecycle;
    http::request req = make_req(http::methods::POST);
    Config cfg = { };
    cgi::StartedRequest started;
    cgi::CleanupResult cleanup;
    int32_t stdout_fd;

    req.uri = "/cgi/echo.sh";
    req.body = "complete-body";
    cfg.cgi_pass = "/bin/sh";
    cfg.cgi_timeout = DEFAULT_CGI_TIMEOUT;
    cfg.cgi_output_buffer_size = 4096;
    start_lifecycle_script(lifecycle, 123, req, cfg,
        "dd bs=1 count=13 >/dev/null 2>/dev/null\n"
        "printf 'Content-Type: text/plain\\r\\n\\r\\nhello'\n",
        started);
    stdout_fd = started.stdout_fd;

    close_lifecycle_stdin(lifecycle, started);
    cleanup = finish_lifecycle_request(lifecycle, stdout_fd);
    cr_assert_eq(cleanup.completion.clientfd, 123);
    cr_assert_not(cleanup.completion.failed);
    cr_assert_str_eq(cleanup.completion.request.uri.c_str(), "/cgi/echo.sh");
    cr_assert_str_eq(cleanup.completion.output.c_str(),
        "Content-Type: text/plain\r\n\r\nhello");
}

Test(cgi_lifecycle, wait_timeout_comes_from_cgi_deadlines)
{
    cgi::Lifecycle lifecycle;
    cgi::StartedRequest started;
    cgi::CleanupResult cleanup;
    int32_t stdout_fd;

    cr_assert_eq(lifecycle.wait_timeout(), -1);
    start_lifecycle_request(lifecycle, 12, 0, started);
    stdout_fd = started.stdout_fd;
    cr_assert_eq(lifecycle.wait_timeout(), 0);
    cleanup = lifecycle.cleanup_request(stdout_fd, cgi::job_cleanup::ABORT);
    close_cleanup_descriptors(cleanup);
}

Test(cgi_lifecycle, expire_jobs_marks_timed_out_jobs)
{
    cgi::Lifecycle lifecycle;
    cgi::StartedRequest started;
    cgi::CleanupResult cleanup;
    int32_t stdout_fd;

    start_lifecycle_request(lifecycle, 12, 0, started);
    stdout_fd = started.stdout_fd;
    std::vector<int32_t> expired = lifecycle.expire_jobs();
    cr_assert_eq(expired.size(), 1);
    cr_assert_eq(expired[0], stdout_fd);
    cleanup = lifecycle.cleanup_request(stdout_fd, cgi::job_cleanup::COMPLETE);
    close_cleanup_descriptors(cleanup);
    cr_assert(cleanup.completion.failed);
    cr_assert_eq(
        cleanup.completion.failure_status, http::status::GATEWAY_TIMEOUT);
}

Test(cgi_lifecycle, jobs_to_cancel_for_client_reports_only_matching_jobs)
{
    cgi::Lifecycle lifecycle;
    cgi::StartedRequest first;
    cgi::StartedRequest other;
    cgi::StartedRequest second;
    int32_t first_stdout;
    int32_t other_stdout;
    int32_t second_stdout;

    start_lifecycle_request(lifecycle, 42, DEFAULT_CGI_TIMEOUT, first);
    start_lifecycle_request(lifecycle, 99, DEFAULT_CGI_TIMEOUT, other);
    start_lifecycle_request(lifecycle, 42, DEFAULT_CGI_TIMEOUT, second);
    first_stdout = first.stdout_fd;
    other_stdout = other.stdout_fd;
    second_stdout = second.stdout_fd;
    std::vector<int32_t> jobs_to_cancel = lifecycle.jobs_to_cancel_for(42);

    cr_assert_eq(jobs_to_cancel.size(), 2);
    cr_assert_eq(jobs_to_cancel[0], first_stdout);
    cr_assert_eq(jobs_to_cancel[1], second_stdout);
    close_cleanup_descriptors(
        lifecycle.cleanup_request(first_stdout, cgi::job_cleanup::ABORT));
    close_cleanup_descriptors(
        lifecycle.cleanup_request(second_stdout, cgi::job_cleanup::ABORT));
    close_cleanup_descriptors(
        lifecycle.cleanup_request(other_stdout, cgi::job_cleanup::ABORT));
}

Test(cgi_lifecycle, abort_cleanup_terminates_and_reaps_child)
{
    cgi::Lifecycle lifecycle;
    cgi::StartedRequest started;
    cgi::CleanupResult cleanup;
    std::string root = make_tmpdir();
    std::string pidfile = root + "/cgi.pid";
    http::request req = make_req(http::methods::GET);
    Config cfg = { };
    int32_t stdout_fd;
    pid_t pid;

    req.uri = "/cgi/linger.sh";
    cfg.cgi_pass = "/bin/sh";
    cfg.cgi_timeout = DEFAULT_CGI_TIMEOUT;
    cfg.cgi_output_buffer_size = 4096;
    start_lifecycle_script(lifecycle, 77, req, cfg,
        "printf '%s\\n' \"$$\" > '" + pidfile
            + "'\n"
              "while :; do :; done\n",
        started);
    stdout_fd = started.stdout_fd;
    pid = wait_for_pid_file(pidfile);

    cleanup = lifecycle.cleanup_request(stdout_fd, cgi::job_cleanup::ABORT);
    close_cleanup_descriptors(cleanup);
    for (int i = 0; i < 100; ++i) {
        lifecycle.reap_pending_children();
        if (process_is_gone(pid))
            return;
        usleep(1000);
    }
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    cr_assert_fail("CGI lifecycle did not reap aborted child");
}

Test(cgi_lifecycle, complete_cleanup_reaps_exited_child)
{
    cgi::Lifecycle lifecycle;
    cgi::StartedRequest started;
    cgi::CleanupResult cleanup;
    std::string root = make_tmpdir();
    std::string pidfile = root + "/cgi.pid";
    http::request req = make_req(http::methods::GET);
    Config cfg = { };
    int32_t stdout_fd;
    pid_t pid;

    req.uri = "/cgi/done.sh";
    cfg.cgi_pass = "/bin/sh";
    cfg.cgi_timeout = DEFAULT_CGI_TIMEOUT;
    cfg.cgi_output_buffer_size = 4096;
    start_lifecycle_script(lifecycle, 88, req, cfg,
        "printf '%s\\n' \"$$\" > '" + pidfile
            + "'\n"
              "printf 'Content-Type: text/plain\\r\\n\\r\\ndone'\n",
        started);
    stdout_fd = started.stdout_fd;
    pid = wait_for_pid_file(pidfile);

    close_lifecycle_stdin(lifecycle, started);
    cleanup = finish_lifecycle_request(lifecycle, stdout_fd);
    cr_assert(process_is_gone(pid));
}
