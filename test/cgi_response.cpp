/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi_response.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 06:06:28 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/18 06:06:28 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

#include <cstring>
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
