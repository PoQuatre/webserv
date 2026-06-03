/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   http_headers.cpp                                   :+:      :+:    :+:   */
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
// Headers
// -----------------------------------------------------------------------------

Test(headers, single_header)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\nHost: example.com\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.count("host"), 1);
}

Test(headers, multiple_headers)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\n"
                               "Host: example.com\r\n"
                               "Accept: text/html\r\n"
                               "Connection: keep-alive\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.count("host"), 1);
    cr_assert_eq(p.request().headers.count("accept"), 1);
    cr_assert_eq(p.request().headers.count("connection"), 1);
}

Test(headers, keys_lowercased)
{
    HttpParser p
        = make_parser("GET / HTTP/1.0\r\nContent-Type: text/html\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.count("content-type"), 1);
    cr_assert_eq(p.request().headers.count("Content-Type"), 0);
}

Test(headers, lf_only_terminator)
{
    HttpParser p = make_parser("GET / HTTP/1.0\nHost: h\n\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.count("host"), 1);
}

Test(headers, value_leading_space_trimmed)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\nX-Foo:   bar\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.at("x-foo"), "bar");
}

Test(headers, value_leading_tab_trimmed)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\nX-Foo:\tbar\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.at("x-foo"), "bar");
}

Test(headers, value_trailing_whitespace_trimmed)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\nX-Foo: bar   \r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.at("x-foo"), "bar");
}

Test(headers, key_trailing_space_trimmed)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\nX-Foo : bar\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.count("x-foo"), 1);
}

Test(headers, value_internal_spaces_preserved)
{
    HttpParser p = make_parser(
        "GET / HTTP/1.0\r\nContent-Type: text/html; charset=utf-8\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(
        p.request().headers.at("content-type"), "text/html; charset=utf-8");
}

Test(headers, value_internal_tab_preserved)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\nX-Foo: bar\tbaz\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.at("x-foo"), "bar\tbaz");
}

Test(headers, key_internal_space_preserved)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\nX My: value\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.count("x my"), 1);
}

// -----------------------------------------------------------------------------
// Header continuation lines (folded headers, RFC 2616 §2.2)
// -----------------------------------------------------------------------------

Test(headers, continuation_space)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\n"
                               "X-Foo: first\r\n"
                               " second\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.at("x-foo"), "first second");
}

Test(headers, continuation_tab)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\n"
                               "X-Foo: first\r\n"
                               "\tsecond\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.at("x-foo"), "first second");
}

Test(headers, continuation_multiple_lines)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\n"
                               "X-Foo: one\r\n"
                               " two\r\n"
                               " three\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.at("x-foo"), "one two three");
}

Test(headers, continuation_only_whitespace_skipped)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\n"
                               "X-Foo: bar\r\n"
                               "   \r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.at("x-foo"), "bar");
}

Test(headers, continuation_does_not_affect_next_header)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\n"
                               "X-Foo: first\r\n"
                               " cont\r\n"
                               "X-Bar: other\r\n"
                               "\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().headers.at("x-foo"), "first cont");
    cr_assert_eq(p.request().headers.at("x-bar"), "other");
}
