/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   http_parsing.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 09:56:28 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/22 09:09:09 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

#include <sys/socket.h>
#include <unistd.h>

#include <string>

#include "Connection.hpp"
#include "http.hpp"
#include "logger.hpp"

// Feed raw bytes into a Connection via a socketpair and return it after one
// on_readable() call (sufficient for requests that fit in a single RECV_CHUNK).
static Connection make_conn(const std::string &raw)
{
    logger::log_level() = logger::levels::NOTHING;
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
        return Connection();
    write(fds[1], raw.c_str(), raw.size());
    Connection conn(fds[0]);
    while (!conn.is_parse_complete() && !conn.is_parse_error()
        && conn.on_readable())
        ;
    close(fds[0]);
    close(fds[1]);
    return conn;
}

// Same as make_conn but closes the write end before reading so the connection
// sees EOF after the data runs out.  Drives on_readable() until the parse
// reaches a terminal state or the EOF signal stops further progress.
static Connection make_conn_eof(const std::string &raw)
{
    logger::log_level() = logger::levels::NOTHING;
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
        return Connection();
    write(fds[1], raw.c_str(), raw.size());
    close(fds[1]);
    Connection conn(fds[0]);
    while (!conn.is_parse_complete() && !conn.is_parse_error()
        && conn.on_readable())
        ;
    close(fds[0]);
    return conn;
}

// -----------------------------------------------------------------------------
// Request line: valid methods
// -----------------------------------------------------------------------------

Test(request_line, get_http11)
{
    Connection c
        = make_conn("GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::GET);
    cr_assert_eq(c.request().uri, "/index.html");
    cr_assert_eq(c.request().version, http::versions::HTTP11);
}

Test(request_line, post_http10)
{
    Connection c
        = make_conn("POST /submit HTTP/1.0\r\nContent-Length: 0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::POST);
    cr_assert_eq(c.request().uri, "/submit");
}

Test(request_line, delete_http10)
{
    Connection c = make_conn("DELETE /res HTTP/1.0\r\nHost: localhost\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::DELETE);
    cr_assert_eq(c.request().version, http::versions::HTTP10);
}

Test(request_line, put_method)
{
    Connection c = make_conn("PUT /res HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::PUT);
}

Test(request_line, patch_method)
{
    Connection c = make_conn("PATCH /res HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::PATCH);
}

Test(request_line, head_method)
{
    Connection c = make_conn("HEAD / HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::HEAD);
}

Test(request_line, options_method)
{
    Connection c = make_conn("OPTIONS * HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::OPTIONS);
}

Test(request_line, trace_method)
{
    Connection c = make_conn("TRACE / HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::TRACE);
}

Test(request_line, lowercase_method_normalized)
{
    Connection c = make_conn("get / HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::GET);
}

Test(request_line, mixed_case_method_normalized)
{
    Connection c = make_conn("Post /x HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::POST);
}

// -----------------------------------------------------------------------------
// HTTP/0.9: GET without a version field
// -----------------------------------------------------------------------------

Test(request_line, get_http09_no_version)
{
    Connection c = make_conn("GET /simple\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().version, http::versions::HTTP09);
    cr_assert_eq(c.request().uri, "/simple");
}

Test(request_line, get_http09_lf_only)
{
    Connection c = make_conn("GET /simple\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().version, http::versions::HTTP09);
    cr_assert_eq(c.request().uri, "/simple");
}

// -----------------------------------------------------------------------------
// Request line: error cases
// -----------------------------------------------------------------------------

Test(request_line, unknown_method)
{
    Connection c = make_conn("BREW /coffee HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_error());
}

Test(request_line, empty_line)
{
    Connection c = make_conn("\r\n");
    cr_assert(c.is_parse_error());
}

Test(request_line, method_only)
{
    Connection c = make_conn("GET\r\n");
    cr_assert(c.is_parse_error());
}

Test(request_line, non_get_missing_version)
{
    Connection c = make_conn("POST /x\r\n\r\n");
    cr_assert(c.is_parse_error());
}

Test(request_line, invalid_version)
{
    Connection c = make_conn("GET / HTTP/2.0\r\n\r\n");
    cr_assert(c.is_parse_error());
}

// -----------------------------------------------------------------------------
// Line endings
// -----------------------------------------------------------------------------

Test(request_line, lf_only_line_ending)
{
    Connection c = make_conn("GET /path HTTP/1.0\nHost: localhost\n\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().uri, "/path");
}

// -----------------------------------------------------------------------------
// URI
// -----------------------------------------------------------------------------

Test(uri, root_path)
{
    Connection c = make_conn("GET / HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().uri, "/");
}

Test(uri, path_with_query)
{
    Connection c = make_conn("GET /search?q=hello&page=2 HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().uri, "/search?q=hello&page=2");
}

Test(uri, path_with_fragment)
{
    Connection c = make_conn("GET /page#section HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().uri, "/page#section");
}

// -----------------------------------------------------------------------------
// Headers
// -----------------------------------------------------------------------------

Test(headers, single_header)
{
    Connection c = make_conn("GET / HTTP/1.0\r\nHost: example.com\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.count("host"), 1);
}

Test(headers, multiple_headers)
{
    Connection c = make_conn("GET / HTTP/1.0\r\n"
                             "Host: example.com\r\n"
                             "Accept: text/html\r\n"
                             "Connection: keep-alive\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.count("host"), 1);
    cr_assert_eq(c.request().headers.count("accept"), 1);
    cr_assert_eq(c.request().headers.count("connection"), 1);
}

Test(headers, keys_lowercased)
{
    Connection c
        = make_conn("GET / HTTP/1.0\r\nContent-Type: text/html\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.count("content-type"), 1);
    cr_assert_eq(c.request().headers.count("Content-Type"), 0);
}

Test(headers, lf_only_terminator)
{
    Connection c = make_conn("GET / HTTP/1.0\nHost: h\n\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.count("host"), 1);
}

Test(headers, value_leading_space_trimmed)
{
    Connection c = make_conn("GET / HTTP/1.0\r\nX-Foo:   bar\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.at("x-foo"), "bar");
}

Test(headers, value_leading_tab_trimmed)
{
    Connection c = make_conn("GET / HTTP/1.0\r\nX-Foo:\tbar\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.at("x-foo"), "bar");
}

Test(headers, value_trailing_whitespace_trimmed)
{
    Connection c = make_conn("GET / HTTP/1.0\r\nX-Foo: bar   \r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.at("x-foo"), "bar");
}

Test(headers, key_trailing_space_trimmed)
{
    Connection c = make_conn("GET / HTTP/1.0\r\nX-Foo : bar\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.count("x-foo"), 1);
}

Test(headers, value_internal_spaces_preserved)
{
    Connection c = make_conn(
        "GET / HTTP/1.0\r\nContent-Type: text/html; charset=utf-8\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(
        c.request().headers.at("content-type"), "text/html; charset=utf-8");
}

Test(headers, value_internal_tab_preserved)
{
    Connection c = make_conn("GET / HTTP/1.0\r\nX-Foo: bar\tbaz\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.at("x-foo"), "bar\tbaz");
}

Test(headers, key_internal_space_preserved)
{
    Connection c = make_conn("GET / HTTP/1.0\r\nX My: value\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.count("x my"), 1);
}

// -----------------------------------------------------------------------------
// Header continuation lines (folded headers, RFC 2616 §2.2)
// -----------------------------------------------------------------------------

Test(headers, continuation_space)
{
    Connection c = make_conn("GET / HTTP/1.0\r\n"
                             "X-Foo: first\r\n"
                             " second\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.at("x-foo"), "first second");
}

Test(headers, continuation_tab)
{
    Connection c = make_conn("GET / HTTP/1.0\r\n"
                             "X-Foo: first\r\n"
                             "\tsecond\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.at("x-foo"), "first second");
}

Test(headers, continuation_multiple_lines)
{
    Connection c = make_conn("GET / HTTP/1.0\r\n"
                             "X-Foo: one\r\n"
                             " two\r\n"
                             " three\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.at("x-foo"), "one two three");
}

Test(headers, continuation_only_whitespace_skipped)
{
    Connection c = make_conn("GET / HTTP/1.0\r\n"
                             "X-Foo: bar\r\n"
                             "   \r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.at("x-foo"), "bar");
}

Test(headers, continuation_does_not_affect_next_header)
{
    Connection c = make_conn("GET / HTTP/1.0\r\n"
                             "X-Foo: first\r\n"
                             " cont\r\n"
                             "X-Bar: other\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.at("x-foo"), "first cont");
    cr_assert_eq(c.request().headers.at("x-bar"), "other");
}

// -----------------------------------------------------------------------------
// Body
// -----------------------------------------------------------------------------

Test(body, content_length_body_present)
{
    Connection c = make_conn("POST /upload HTTP/1.0\r\n"
                             "Content-Length: 5\r\n"
                             "\r\n"
                             "hello");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().body, "hello");
}

Test(body, content_length_zero)
{
    Connection c = make_conn("POST /x HTTP/1.0\r\nContent-Length: 0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert(c.request().body.empty());
}

Test(body, no_content_length_no_body)
{
    Connection c = make_conn("GET / HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert(c.request().body.empty());
}

// -----------------------------------------------------------------------------
// No headers — content_length defaults to 0, body must be empty
// -----------------------------------------------------------------------------

Test(no_headers, get_no_headers)
{
    Connection c = make_conn("GET / HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert(c.request().headers.empty());
    cr_assert_eq(c.request().body.size(), 0);
}

Test(no_headers, post_no_headers_no_body)
{
    Connection c = make_conn("POST /submit HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert(c.request().headers.empty());
    cr_assert_eq(c.request().body.size(), 0);
}

Test(no_headers, delete_no_headers)
{
    Connection c = make_conn("DELETE /res HTTP/1.0\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert(c.request().headers.empty());
    cr_assert_eq(c.request().body.size(), 0);
}

// -----------------------------------------------------------------------------
// Reset
// -----------------------------------------------------------------------------

Test(reset, second_request_after_reset)
{
    logger::log_level() = logger::levels::NOTHING;
    int fds[2];
    cr_assert_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const std::string req1 = "GET /first HTTP/1.0\r\n\r\n";
    const std::string req2
        = "POST /second HTTP/1.0\r\nContent-Length: 3\r\n\r\nabc";

    write(fds[1], req1.c_str(), req1.size());
    Connection conn(fds[0]);
    conn.on_readable();
    cr_assert(conn.is_parse_complete());
    cr_assert_eq(conn.request().uri, "/first");

    conn.reset();
    write(fds[1], req2.c_str(), req2.size());
    conn.on_readable();
    cr_assert(conn.is_parse_complete());
    cr_assert_eq(conn.request().method, http::methods::POST);
    cr_assert_eq(conn.request().uri, "/second");
    cr_assert_eq(conn.request().body, "abc");

    close(fds[0]);
    close(fds[1]);
}

// -----------------------------------------------------------------------------
// EOF-terminated requests (write side closed without final newlines)
// The server should accept these rather than treating them as errors.
// -----------------------------------------------------------------------------

Test(eof_terminated, request_line_no_crlf)
{
    Connection c = make_conn_eof("GET /path HTTP/1.1");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::GET);
    cr_assert_eq(c.request().uri, "/path");
    cr_assert_eq(c.request().version, http::versions::HTTP11);
}

Test(eof_terminated, last_header_no_crlf)
{
    Connection c = make_conn_eof("GET / HTTP/1.0\r\nHost: localhost");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.count("host"), 1);
}

Test(eof_terminated, headers_no_blank_line)
{
    Connection c = make_conn_eof("GET / HTTP/1.0\r\nHost: localhost\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.count("host"), 1);
}

Test(eof_terminated, multiple_headers_no_blank_line)
{
    Connection c
        = make_conn_eof("GET / HTTP/1.0\r\nHost: h\r\nAccept: text/html\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().headers.count("host"), 1);
    cr_assert_eq(c.request().headers.count("accept"), 1);
}

// -----------------------------------------------------------------------------
// HTTP/1.1 specific behavior
// -----------------------------------------------------------------------------

Test(http11, get_without_host_accepted)
{
    // NOTE: parser does not enforce the Host requirement from RFC 7230 5.4
    Connection c = make_conn("GET / HTTP/1.1\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().version, http::versions::HTTP11);
}

Test(http11, get_with_host)
{
    Connection c
        = make_conn("GET /index.html HTTP/1.1\r\nHost: example.com\r\n\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().version, http::versions::HTTP11);
    cr_assert_eq(c.request().headers.count("host"), 1);
}

Test(http11, get_with_body_via_content_length)
{
    Connection c = make_conn("GET /search HTTP/1.1\r\n"
                             "Host: localhost\r\n"
                             "Content-Length: 5\r\n"
                             "\r\n"
                             "query");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().body, "query");
}

Test(http11, post_with_content_length)
{
    Connection c = make_conn("POST /data HTTP/1.1\r\n"
                             "Host: localhost\r\n"
                             "Content-Length: 4\r\n"
                             "\r\n"
                             "data");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::POST);
    cr_assert_eq(c.request().body, "data");
}

Test(http11, post_without_body_framing)
{
    Connection c = make_conn("POST /data HTTP/1.1\r\n"
                             "Host: localhost\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::POST);
    cr_assert_eq(c.request().uri, "/data");
}

Test(http11, put_without_body_framing)
{
    Connection c = make_conn("PUT /res HTTP/1.1\r\n"
                             "Host: localhost\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::PUT);
    cr_assert_eq(c.request().uri, "/res");
}

Test(http11, delete_without_body_framing)
{
    Connection c = make_conn("DELETE /res HTTP/1.1\r\n"
                             "Host: localhost\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().method, http::methods::DELETE);
    cr_assert_eq(c.request().uri, "/res");
}

// -----------------------------------------------------------------------------
// Chunked Transfer-Encoding (HTTP/1.1)
// -----------------------------------------------------------------------------

Test(chunked, single_chunk)
{
    Connection c = make_conn("POST /upload HTTP/1.1\r\n"
                             "Host: localhost\r\n"
                             "Transfer-Encoding: chunked\r\n"
                             "\r\n"
                             "5\r\n"
                             "hello\r\n"
                             "0\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().body, "hello");
}

Test(chunked, multiple_chunks)
{
    Connection c = make_conn("POST /upload HTTP/1.1\r\n"
                             "Host: localhost\r\n"
                             "Transfer-Encoding: chunked\r\n"
                             "\r\n"
                             "5\r\n"
                             "hello\r\n"
                             "6\r\n"
                             " world\r\n"
                             "0\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().body, "hello world");
}

Test(chunked, empty_body)
{
    Connection c = make_conn("POST /upload HTTP/1.1\r\n"
                             "Host: localhost\r\n"
                             "Transfer-Encoding: chunked\r\n"
                             "\r\n"
                             "0\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert(c.request().body.empty());
}

Test(chunked, hex_chunk_size)
{
    Connection c = make_conn("POST /upload HTTP/1.1\r\n"
                             "Host: localhost\r\n"
                             "Transfer-Encoding: chunked\r\n"
                             "\r\n"
                             "a\r\n"
                             "0123456789\r\n"
                             "0\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert_eq(c.request().body, "0123456789");
}

Test(chunked, not_applied_to_http10)
{
    Connection c = make_conn("GET / HTTP/1.0\r\n"
                             "Transfer-Encoding: chunked\r\n"
                             "\r\n");
    cr_assert(c.is_parse_complete());
    cr_assert(c.request().body.empty());
}
