/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi_response.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 06:06:28 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/18 07:38:28 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

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

Test(cgi_response, status_header_controls_status_line)
{
    std::string out = cgi::translate_output(
        "Status: 201 Created\r\nContent-Type: text/plain\r\n\r\nmade",
        make_req(http::methods::GET));

    cr_assert(strncmp(out.c_str(), "HTTP/1.1 201 Created\r\n", 22) == 0);
    cr_assert_eq(out.find("Status:"), std::string::npos);
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

Test(cgi_response, nph_output_is_passed_through_after_validation)
{
    std::string raw = "HTTP/1.1 204 No Content\r\nX-NPH: yes\r\n\r\n";
    std::string out = cgi::translate_output(raw, make_req(http::methods::GET));

    cr_assert_str_eq(out.c_str(), raw.c_str());
}

Test(cgi_response, headerless_output_becomes_html_response)
{
    std::string out = cgi::translate_output(
        "<h1>Hello</h1>\n", make_req(http::methods::GET));

    cr_assert_str_eq(out.c_str(),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 15\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        "<h1>Hello</h1>\n");
}

Test(cgi_response, malformed_output_becomes_bad_gateway)
{
    std::string out = cgi::translate_output(
        "Content-Type text/plain\r\n\r\nhello", make_req(http::methods::GET));

    cr_assert(strncmp(out.c_str(), "HTTP/1.1 502 Bad Gateway\r\n", 26) == 0);
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
        "printf 'GATEWAY_INTERFACE=%s\\n' \"$GATEWAY_INTERFACE\"\n"
        "printf 'REQUEST_METHOD=%s\\n' \"$REQUEST_METHOD\"\n"
        "printf 'QUERY_STRING=%s\\n' \"$QUERY_STRING\"\n"
        "printf 'SCRIPT_NAME=%s\\n' \"$SCRIPT_NAME\"\n"
        "printf 'SCRIPT_FILENAME=%s\\n' \"$SCRIPT_FILENAME\"\n"
        "printf 'REMOTE_ADDR=%s\\n' \"$REMOTE_ADDR\"\n"
        "printf 'SERVER_NAME=%s\\n' \"$SERVER_NAME\"\n"
        "printf 'SERVER_PORT=%s\\n' \"$SERVER_PORT\"\n"
        "printf 'SERVER_PROTOCOL=%s\\n' \"$SERVER_PROTOCOL\"\n"
        "printf 'SERVER_SOFTWARE=%s\\n' \"$SERVER_SOFTWARE\"\n"
        "printf 'CONTENT_LENGTH=%s\\n' \"$CONTENT_LENGTH\"\n"
        "printf 'CONTENT_TYPE=%s\\n' \"$CONTENT_TYPE\"\n"
        "printf 'HTTP_ACCEPT=%s\\n' \"$HTTP_ACCEPT\"\n"
        "printf 'HTTP_HOST=%s\\n' \"$HTTP_HOST\"\n"
        "printf 'HTTP_X_CUSTOM_HEADER=%s\\n' \"$HTTP_X_CUSTOM_HEADER\"\n"
        "printf 'PATH=%s\\n' \"$PATH\"\n"
        "env\n");
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
    cr_assert(cgi::start_get(req, cfg, script, process));
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
    assert_contains(output, "SERVER_NAME=example.test\n");
    assert_contains(output, "SERVER_PORT=8080\n");
    assert_contains(output, "SERVER_PROTOCOL=HTTP/1.1\n");
    assert_contains(output, "SERVER_SOFTWARE=webserv\n");
    assert_contains(output, "CONTENT_LENGTH=123\n");
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
