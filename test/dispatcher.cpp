/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dispatcher.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/20 16:16:07 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/26 21:35:58 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "dispatcher.hpp"

#include <criterion/criterion.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "test_helpers.hpp"

static Server make_dispatch_server(const std::string &root)
{
    Config config = { };
    Config cgi_config = config;
    Location cgi_location;
    std::vector<Location> locations;

    config.root = root;
    config.allowed_methods[http::methods::GET] = true;
    config.allowed_methods[http::methods::POST] = true;
    config.allowed_methods[http::methods::HEAD] = true;
    config.allowed_methods[http::methods::DELETE] = true;
    cgi_config = config;
    cgi_config.allowed_methods[http::methods::DELETE] = false;
    cgi_config.cgi_pass = "/bin/sh";
    cgi_config.cgi_timeout = DEFAULT_CGI_TIMEOUT;
    cgi_config.cgi_output_buffer_size = DEFAULT_CGI_OUTPUT_BUFFER_SIZE;

    cgi_location.path = "/cgi";
    cgi_location.config = cgi_config;
    cgi_location.type = location::CLASSIC;
    locations.push_back(cgi_location);

    return Server(locations, "test", "127.0.0.1:0", config);
}

static http::request make_request(
    http::methods::type method, const std::string &uri)
{
    http::request req = { };

    req.method = method;
    req.uri = uri;
    req.version = http::versions::HTTP11;
    req.keep_alive = true;
    return req;
}

static void assert_status(const std::string &response, const char *status)
{
    cr_assert_neq(response.find(status), std::string::npos,
        "missing status '%s' in response:\n%s", status, response.c_str());
}

Test(dispatcher, static_get_returns_response_outcome)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root);
    http::request req = make_request(http::methods::GET, "/static.txt");

    test_write_file(root + "/static.txt", "static body\n");

    dispatcher::Outcome outcome = dispatcher::handle(req, server);

    cr_assert_eq(outcome.type, dispatcher::Outcome::RESPONSE_NOW);
    assert_status(outcome.response, "HTTP/1.1 200 OK");
    cr_assert_neq(outcome.response.find("static body\n"), std::string::npos);
}

Test(dispatcher, allowed_static_non_get_still_returns_not_implemented)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root);
    http::request req = make_request(http::methods::POST, "/static.txt");

    dispatcher::Outcome outcome = dispatcher::handle(req, server);

    cr_assert_eq(outcome.type, dispatcher::Outcome::RESPONSE_NOW);
    assert_status(outcome.response, "HTTP/1.1 501 Not Implemented");
}

Test(dispatcher, cgi_eligible_request_returns_cgi_outcome)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root);
    http::request req = make_request(http::methods::POST, "/cgi/app.sh");

    dispatcher::Outcome outcome = dispatcher::handle(req, server);

    cr_assert_eq(outcome.type, dispatcher::Outcome::CGI_REQUIRED);
    cr_assert_not_null(outcome.config);
    cr_assert_str_eq(outcome.config->cgi_pass.c_str(), "/bin/sh");
    cr_assert_eq(outcome.filesystem_path, root + req.uri);
}

Test(dispatcher, disallowed_cgi_method_returns_response_outcome)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root);
    http::request req = make_request(http::methods::DELETE, "/cgi/app.sh");

    dispatcher::Outcome outcome = dispatcher::handle(req, server);

    cr_assert_eq(outcome.type, dispatcher::Outcome::RESPONSE_NOW);
    assert_status(outcome.response, "HTTP/1.1 405 Method Not Allowed");
}
