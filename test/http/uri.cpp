/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   http_uri.cpp                                       :+:      :+:    :+:   */
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
// URI
// -----------------------------------------------------------------------------

Test(uri, root_path)
{
    HttpParser p = make_parser("GET / HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/");
}

Test(uri, simple_path)
{
    HttpParser p = make_parser("GET /index.html HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/index.html");
}

Test(uri, nested_path)
{
    HttpParser p = make_parser("GET /a/b/c HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/a/b/c");
}

Test(uri, path_with_query_splits_at_question_mark)
{
    HttpParser p = make_parser("GET /search?q=hello&page=2 HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/search");
}

Test(uri, path_without_query_no_query_map)
{
    HttpParser p = make_parser("GET /no-query HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/no-query");
    cr_assert(p.request().query.empty());
}

Test(uri, fragment_stripped)
{
    HttpParser p = make_parser("GET /page#section HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/page");
    cr_assert(p.request().query.empty());
}

Test(uri, fragment_after_query_stripped)
{
    HttpParser p = make_parser("GET /page?k=v#section HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/page");
    cr_assert_eq(p.request().query.at("k"), "v");
    cr_assert_eq(p.request().query.size(), 1);
}

Test(uri, percent_encoded_space)
{
    HttpParser p = make_parser("GET /hello%20world HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/hello world");
}

Test(uri, percent_encoded_slash)
{
    HttpParser p = make_parser("GET /a%2Fb HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/a/b");
}

Test(uri, percent_encoded_uppercase_hex)
{
    HttpParser p = make_parser("GET /caf%C3%A9 HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/caf\xc3\xa9");
}

Test(uri, percent_encoded_lowercase_hex)
{
    HttpParser p = make_parser("GET /caf%c3%a9 HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/caf\xc3\xa9");
}

Test(uri, invalid_percent_sequence_kept_literal)
{
    HttpParser p = make_parser("GET /a%GGb HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/a%GGb");
}

Test(uri, truncated_percent_sequence_kept_literal)
{
    HttpParser p = make_parser("GET /a%4 HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/a%4");
}

Test(uri, lone_percent_kept_literal)
{
    HttpParser p = make_parser("GET /a% HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/a%");
}

// -----------------------------------------------------------------------------
// URI canonicalization
// -----------------------------------------------------------------------------

Test(uri, dot_segment_removed)
{
    HttpParser p = make_parser("GET /a/./b HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/a/b");
}

Test(uri, dot_dot_goes_up)
{
    HttpParser p = make_parser("GET /a/b/../c HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/a/c");
}

Test(uri, dot_dot_at_end_adds_trailing_slash)
{
    HttpParser p = make_parser("GET /a/b/.. HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/a/");
}

Test(uri, dot_at_end_adds_trailing_slash)
{
    HttpParser p = make_parser("GET /a/b/. HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/a/b/");
}

Test(uri, traversal_above_root_clamped)
{
    HttpParser p = make_parser("GET /../../etc/passwd HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/etc/passwd");
}

Test(uri, traversal_to_root)
{
    HttpParser p = make_parser("GET /../.. HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/");
}

Test(uri, double_slashes_merged)
{
    HttpParser p = make_parser("GET //a//b HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/a/b");
}

Test(uri, trailing_slash_preserved)
{
    HttpParser p = make_parser("GET /a/b/ HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/a/b/");
}

Test(uri, encoded_dot_dot_resolved)
{
    HttpParser p = make_parser("GET /a/%2E%2E/b HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/b");
}

// -----------------------------------------------------------------------------
// URI validation
// -----------------------------------------------------------------------------

Test(uri_validation, empty_uri_is_error)
{
    HttpParser p = make_parser("GET # HTTP/1.0\r\n\r\n");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

Test(uri_validation, bare_word_no_slash_is_error)
{
    HttpParser p = make_parser("GET foo HTTP/1.0\r\n\r\n");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

Test(uri_validation, empty_scheme_is_error)
{
    HttpParser p = make_parser("GET ://host/path HTTP/1.0\r\n\r\n");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

Test(uri_validation, asterisk_form_valid)
{
    HttpParser p
        = make_parser("OPTIONS * HTTP/1.1\r\nHost: localhost\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "*");
}

Test(uri_validation, asterisk_with_extra_chars_is_error)
{
    HttpParser p = make_parser("GET *path HTTP/1.0\r\n\r\n");
    cr_assert(p.is_error());
    cr_assert_eq(p.error_code(), http::status::BAD_REQUEST);
}

Test(uri_validation, absolute_form_path_extracted)
{
    HttpParser p = make_parser(
        "GET http://example.com/path HTTP/1.1\r\nHost: example.com\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/path");
    cr_assert(p.request().query.empty());
}

Test(uri_validation, absolute_form_with_query)
{
    HttpParser p = make_parser("GET http://example.com/search?q=hello "
                               "HTTP/1.1\r\nHost: example.com\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/search");
    cr_assert_eq(p.request().query.at("q"), "hello");
}

Test(uri_validation, absolute_form_no_path_defaults_to_root)
{
    HttpParser p = make_parser(
        "GET http://example.com HTTP/1.1\r\nHost: example.com\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/");
}

Test(uri_validation, absolute_form_query_without_path)
{
    HttpParser p = make_parser(
        "GET http://example.com?q=1 HTTP/1.1\r\nHost: example.com\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/");
    cr_assert_eq(p.request().query.at("q"), "1");
}

Test(uri_validation, absolute_form_with_port)
{
    HttpParser p = make_parser("GET http://example.com:8080/path "
                               "HTTP/1.1\r\nHost: example.com\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/path");
}

Test(uri_validation, absolute_form_fragment_stripped)
{
    HttpParser p = make_parser("GET http://example.com/path#section "
                               "HTTP/1.1\r\nHost: example.com\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/path");
}

Test(uri_validation, https_scheme_accepted)
{
    HttpParser p = make_parser(
        "GET https://example.com/secure HTTP/1.1\r\nHost: example.com\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/secure");
}

// -----------------------------------------------------------------------------
// Query params
// -----------------------------------------------------------------------------

Test(query, single_key_value)
{
    HttpParser p = make_parser("GET /?key=value HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().query.count("key"), 1);
    cr_assert_eq(p.request().query.at("key"), "value");
}

Test(query, multiple_params)
{
    HttpParser p = make_parser("GET /?a=1&b=2&c=3 HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().query.at("a"), "1");
    cr_assert_eq(p.request().query.at("b"), "2");
    cr_assert_eq(p.request().query.at("c"), "3");
}

Test(query, key_without_value)
{
    HttpParser p = make_parser("GET /?flag HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().query.count("flag"), 1);
    cr_assert(p.request().query.at("flag").empty());
}

Test(query, key_with_empty_value)
{
    HttpParser p = make_parser("GET /?key= HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().query.count("key"), 1);
    cr_assert(p.request().query.at("key").empty());
}

Test(query, empty_query_string)
{
    HttpParser p = make_parser("GET /path? HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/path");
    cr_assert(p.request().query.empty());
}

Test(query, percent_encoded_value)
{
    HttpParser p = make_parser("GET /?msg=hello%20world HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().query.at("msg"), "hello world");
}

Test(query, percent_encoded_key)
{
    HttpParser p = make_parser("GET /?my%20key=val HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().query.count("my key"), 1);
    cr_assert_eq(p.request().query.at("my key"), "val");
}

Test(query, plus_decoded_as_space_in_value)
{
    HttpParser p = make_parser("GET /?q=hello+world HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().query.at("q"), "hello world");
}

Test(query, plus_decoded_as_space_in_key)
{
    HttpParser p = make_parser("GET /?my+key=val HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().query.count("my key"), 1);
}

Test(query, duplicate_key_last_wins)
{
    HttpParser p = make_parser("GET /?x=first&x=second HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().query.at("x"), "second");
}

Test(query, mixed_flags_and_values)
{
    HttpParser p = make_parser("GET /?verbose&limit=10&debug HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().query.count("verbose"), 1);
    cr_assert(p.request().query.at("verbose").empty());
    cr_assert_eq(p.request().query.at("limit"), "10");
    cr_assert_eq(p.request().query.count("debug"), 1);
    cr_assert(p.request().query.at("debug").empty());
}

Test(query, uri_and_query_together)
{
    HttpParser p = make_parser("GET /search?q=openai&lang=en HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().uri, "/search");
    cr_assert_eq(p.request().query.at("q"), "openai");
    cr_assert_eq(p.request().query.at("lang"), "en");
}

Test(query, percent_encoded_ampersand_in_value)
{
    HttpParser p = make_parser("GET /?a=1%262 HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().query.at("a"), "1&2");
    cr_assert_eq(p.request().query.size(), 1);
}

Test(query, percent_encoded_equals_in_value)
{
    HttpParser p = make_parser("GET /?a=x%3Dy HTTP/1.0\r\n\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().query.at("a"), "x=y");
}

Test(query, http09_with_query)
{
    HttpParser p = make_parser("GET /search?q=test\r\n");
    cr_assert(p.is_complete());
    cr_assert_eq(p.request().version, http::versions::HTTP09);
    cr_assert_eq(p.request().uri, "/search");
    cr_assert_eq(p.request().query.at("q"), "test");
}
