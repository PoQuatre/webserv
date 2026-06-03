/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   http_body.cpp                                      :+:      :+:    :+:   */
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

// -----------------------------------------------------------------------------
// Body
// -----------------------------------------------------------------------------

Test(body, content_length_body_present)
{
    HttpParser p = make_parser("POST /upload HTTP/1.0\r\n"
                               "Content-Length: 5\r\n"
                               "\r\n"
                               "hello");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().body, "hello");
}

Test(body, content_length_zero)
{
    HttpParser p
        = make_parser("POST /x HTTP/1.0\r\nContent-Length: 0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert(p.request().body.empty());
}

Test(body, no_content_length_no_body)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert(p.request().body.empty());
}

Test(body, content_length_non_numeric)
{
    HttpParser p
        = make_parser("POST /x HTTP/1.0\r\nContent-Length: abc\r\n\r\nbody");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

Test(body, content_length_trailing_garbage)
{
    HttpParser p
        = make_parser("POST /x HTTP/1.0\r\nContent-Length: 5abc\r\n\r\nhello");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

Test(body, content_length_empty_value)
{
    HttpParser p = make_parser("POST /x HTTP/1.0\r\nContent-Length: \r\n\r\n");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

Test(body, content_length_overflow)
{
    HttpParser p = make_parser(
        "POST /x HTTP/1.0\r\nContent-Length: 99999999999999999999999\r\n\r\n");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

Test(body, content_length_negative)
{
    HttpParser p
        = make_parser("POST /x HTTP/1.0\r\nContent-Length: -1\r\n\r\nbody");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

// -----------------------------------------------------------------------------
// No headers — content_length defaults to 0, body must be empty
// -----------------------------------------------------------------------------

Test(no_headers, get_no_headers)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert(p.request().headers.empty());
    cr_assert_eq(p.request().body.size(), 0);
}

Test(no_headers, post_no_headers_no_body)
{
    HttpParser p = make_parser("POST /submit HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert(p.request().headers.empty());
    cr_assert_eq(p.request().body.size(), 0);
}

Test(no_headers, delete_no_headers)
{
    HttpParser p = make_parser("DELETE /res HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert(p.request().headers.empty());
    cr_assert_eq(p.request().body.size(), 0);
}

// -----------------------------------------------------------------------------
// Chunked Transfer-Encoding (HTTP/1.1)
// -----------------------------------------------------------------------------

Test(chunked, single_chunk)
{
    HttpParser p = make_parser("POST /upload HTTP/1.1\r\n"
                               "Host: localhost\r\n"
                               "Transfer-Encoding: chunked\r\n"
                               "\r\n"
                               "5\r\n"
                               "hello\r\n"
                               "0\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().body, "hello");
}

Test(chunked, multiple_chunks)
{
    HttpParser p = make_parser("POST /upload HTTP/1.1\r\n"
                               "Host: localhost\r\n"
                               "Transfer-Encoding: chunked\r\n"
                               "\r\n"
                               "5\r\n"
                               "hello\r\n"
                               "6\r\n"
                               " world\r\n"
                               "0\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().body, "hello world");
}

Test(chunked, empty_body)
{
    HttpParser p = make_parser("POST /upload HTTP/1.1\r\n"
                               "Host: localhost\r\n"
                               "Transfer-Encoding: chunked\r\n"
                               "\r\n"
                               "0\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert(p.request().body.empty());
}

Test(chunked, hex_chunk_size)
{
    HttpParser p = make_parser("POST /upload HTTP/1.1\r\n"
                               "Host: localhost\r\n"
                               "Transfer-Encoding: chunked\r\n"
                               "\r\n"
                               "a\r\n"
                               "0123456789\r\n"
                               "0\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().body, "0123456789");
}

Test(chunked, not_applied_to_http10)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\n"
                               "Transfer-Encoding: chunked\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert(p.request().body.empty());
}
