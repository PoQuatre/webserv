/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   http_request_line.cpp                              :+:      :+:    :+:   */
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
// Request line: valid methods
// -----------------------------------------------------------------------------

Test(request_line, get_http11)
{
    HttpParser p
        = make_parser("GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::GET);
    cr_assert_eq(p.request().uri, "/index.html");
    cr_assert_eq(p.request().version, http::versions::HTTP11);
}

Test(request_line, post_http10)
{
    HttpParser p
        = make_parser("POST /submit HTTP/1.0\r\nContent-Length: 0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::POST);
    cr_assert_eq(p.request().uri, "/submit");
}

Test(request_line, delete_http10)
{
    HttpParser p = make_parser("DELETE /res HTTP/1.0\r\nHost: localhost\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::DELETE);
    cr_assert_eq(p.request().version, http::versions::HTTP10);
}

Test(request_line, put_method)
{
    HttpParser p = make_parser("PUT /res HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::PUT);
}

Test(request_line, patch_method)
{
    HttpParser p = make_parser("PATCH /res HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::PATCH);
}

Test(request_line, head_method)
{
    HttpParser p = make_parser("HEAD / HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::HEAD);
}

Test(request_line, options_method)
{
    HttpParser p = make_parser("OPTIONS * HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::OPTIONS);
}

Test(request_line, trace_method)
{
    HttpParser p = make_parser("TRACE / HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::TRACE);
}

Test(request_line, lowercase_method_normalized)
{
    HttpParser p = make_parser("get / HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::GET);
}

Test(request_line, mixed_case_method_normalized)
{
    HttpParser p = make_parser("Post /x HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().method, http::methods::POST);
}

// -----------------------------------------------------------------------------
// HTTP/0.9: GET without a version field
// -----------------------------------------------------------------------------

Test(request_line, get_http09_no_version)
{
    HttpParser p = make_parser("GET /simple\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().version, http::versions::HTTP09);
    cr_assert_eq(p.request().uri, "/simple");
}

Test(request_line, get_http09_lf_only)
{
    HttpParser p = make_parser("GET /simple\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().version, http::versions::HTTP09);
    cr_assert_eq(p.request().uri, "/simple");
}

// -----------------------------------------------------------------------------
// Request line: error cases
// -----------------------------------------------------------------------------

Test(request_line, unknown_method)
{
    HttpParser p = make_parser("BREW /coffee HTTP/1.0\r\n\r\n");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::NOT_IMPLEMENTED);
}

Test(request_line, empty_line)
{
    HttpParser p = make_parser("\r\n");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

Test(request_line, method_only)
{
    HttpParser p = make_parser("GET\r\n");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

Test(request_line, non_get_missing_version)
{
    HttpParser p = make_parser("POST /x\r\n\r\n");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

Test(request_line, invalid_version)
{
    HttpParser p = make_parser("GET / HTTP/2.0\r\n\r\n");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

// -----------------------------------------------------------------------------
// Line endings
// -----------------------------------------------------------------------------

Test(request_line, lf_only_line_ending)
{
    HttpParser p = make_parser("GET /path HTTP/1.0\nHost: localhost\n\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/path");
}
