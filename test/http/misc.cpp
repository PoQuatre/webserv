/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   http_misc.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 00:00:00 by mle-flem          #+#    #+#             */
/*   Updated: 2026/06/03 00:00:00 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

#include <string>

#include "HttpParser.hpp"
#include "http.hpp"

static HttpParser make_parser(const std::string &raw)
{
    HttpParser p;
    p.feed(raw.c_str(), raw.size());
    return p;
}

static HttpParser make_parser_eof(const std::string &raw)
{
    HttpParser p;
    p.feed(raw.c_str(), raw.size());
    p.set_eof();
    return p;
}

// -----------------------------------------------------------------------------
// Reset
// -----------------------------------------------------------------------------

Test(reset, second_request_after_reset)
{
    HttpParser p;
    const std::string req1 = "GET /first HTTP/1.0\r\n\r\n";
    p.feed(req1.c_str(), req1.size());
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/first");

    p.reset();
    const std::string req2
        = "POST /second HTTP/1.0\r\nContent-Length: 3\r\n\r\nabc";
    p.feed(req2.c_str(), req2.size());
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::POST);
    cr_assert_eq(p.request().uri, "/second");
    cr_assert_eq(p.request().body, "abc");
}

// -----------------------------------------------------------------------------
// EOF-terminated requests (write side closed without final newlines)
// The server should accept these rather than treating them as errors.
// -----------------------------------------------------------------------------

Test(eof_terminated, request_line_no_crlf)
{
    HttpParser p = make_parser_eof("GET /path HTTP/1.1");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::GET);
    cr_assert_eq(p.request().uri, "/path");
    cr_assert_eq(p.request().version, http::versions::HTTP11);
}

Test(eof_terminated, last_header_no_crlf)
{
    HttpParser p = make_parser_eof("GET / HTTP/1.0\r\nHost: localhost");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.count("host"), 1);
}

Test(eof_terminated, headers_no_blank_line)
{
    HttpParser p = make_parser_eof("GET / HTTP/1.0\r\nHost: localhost\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.count("host"), 1);
}

Test(eof_terminated, multiple_headers_no_blank_line)
{
    HttpParser p
        = make_parser_eof("GET / HTTP/1.0\r\nHost: h\r\nAccept: text/html\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.count("host"), 1);
    cr_assert_eq(p.request().headers.count("accept"), 1);
}

// -----------------------------------------------------------------------------
// HTTP/1.1 specific behavior
// -----------------------------------------------------------------------------

Test(http11, get_without_host_accepted)
{
    // NOTE: parser does not enforce the Host requirement from RFC 7230 5.4
    HttpParser p = make_parser("GET / HTTP/1.1\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().version, http::versions::HTTP11);
}

Test(http11, get_with_host)
{
    HttpParser p
        = make_parser("GET /index.html HTTP/1.1\r\nHost: example.com\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().version, http::versions::HTTP11);
    cr_assert_eq(p.request().headers.count("host"), 1);
}

Test(http11, get_with_body_via_content_length)
{
    HttpParser p = make_parser("GET /search HTTP/1.1\r\n"
                               "Host: localhost\r\n"
                               "Content-Length: 5\r\n"
                               "\r\n"
                               "query");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().body, "query");
}

Test(http11, post_with_content_length)
{
    HttpParser p = make_parser("POST /data HTTP/1.1\r\n"
                               "Host: localhost\r\n"
                               "Content-Length: 4\r\n"
                               "\r\n"
                               "data");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::POST);
    cr_assert_eq(p.request().body, "data");
}

Test(http11, post_without_body_framing)
{
    HttpParser p = make_parser("POST /data HTTP/1.1\r\n"
                               "Host: localhost\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::POST);
    cr_assert_eq(p.request().uri, "/data");
}

Test(http11, put_without_body_framing)
{
    HttpParser p = make_parser("PUT /res HTTP/1.1\r\n"
                               "Host: localhost\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::PUT);
    cr_assert_eq(p.request().uri, "/res");
}

Test(http11, delete_without_body_framing)
{
    HttpParser p = make_parser("DELETE /res HTTP/1.1\r\n"
                               "Host: localhost\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::DELETE);
    cr_assert_eq(p.request().uri, "/res");
}
