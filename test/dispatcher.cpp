/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dispatcher.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/20 16:16:07 by mle-flem          #+#    #+#             */
/*   Updated: 2026/08/07 10:05:23 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "dispatcher.hpp"

#include <sys/stat.h>

#include <criterion/criterion.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include "test_helpers.hpp"

static Server make_dispatch_server(
    const std::string &root, bool autoindex = false,
    const std::string &index = "")
{
    Config config = { };
    Config cgi_config = { };
    Location cgi_location;
    std::vector<Location> locations;

    config.root = root;
    config.allowed_methods[http::methods::GET] = true;
    config.allowed_methods[http::methods::POST] = true;
    config.allowed_methods[http::methods::HEAD] = true;
    config.allowed_methods[http::methods::DELETE] = true;
    config.autoindex = autoindex;
    if (!index.empty())
        config.conf.index.push_back(index);
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

static void test_mkdir(const std::string &path)
{
    cr_assert_eq(mkdir(path.c_str(), 0700), 0, "mkdir() failed: %s",
        strerror(errno));
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

Test(dispatcher, static_get_returns_response_outcome)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root);
    http::request req = make_request(http::methods::GET, "/static.txt");

    test_write_file(root + "/static.txt", "static body\n");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 200 OK");
    cr_assert_neq(response.find("static body\n"), std::string::npos);
}

Test(dispatcher, allowed_static_non_get_still_returns_not_implemented)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root);
    http::request req = make_request(http::methods::POST, "/static.txt");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 501 Not Implemented");
}

Test(dispatcher, cgi_eligible_request_selects_cgi_config)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root);
    http::request req = make_request(http::methods::POST, "/cgi/app.sh");

    const Config &cfg = dispatcher::config_for(req, server);

    cr_assert_str_eq(cfg.cgi_pass.c_str(), "/bin/sh");
    cr_assert_eq(cfg.root + req.uri, root + req.uri);
}

Test(dispatcher, disallowed_cgi_method_returns_response_outcome)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root);
    http::request req = make_request(http::methods::DELETE, "/cgi/app.sh");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 405 Method Not Allowed");
}

Test(dispatcher, autoindex_escapes_and_encodes_directory_entries)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root, true);
    http::request req = make_request(http::methods::GET, "/dir/");

    test_mkdir(root + "/dir");
    test_write_file(root + "/dir/<script>.txt", "x\n");
    test_write_file(root + "/dir/what?#.txt", "x\n");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 200 OK");
    cr_assert_eq(response.find("<script>.txt"), std::string::npos);
    cr_assert_neq(response.find("&lt;script&gt;.txt"), std::string::npos);
    cr_assert_neq(response.find("href=\"what%3F%23.txt\""), std::string::npos);
}

Test(dispatcher, unreadable_index_returns_error_instead_of_autoindex)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root, true);
    http::request req = make_request(http::methods::GET, "/dir/");

    test_mkdir(root + "/dir");
    test_write_file(root + "/dir/index.html", "secret\n");
    cr_assert_eq(chmod((root + "/dir/index.html").c_str(), 0000), 0,
        "chmod() failed: %s", strerror(errno));

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 403 Forbidden");
}

Test(dispatcher, configured_index_is_served_before_autoindex)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root, true, "home.html");
    http::request req = make_request(http::methods::GET, "/dir/");

    test_mkdir(root + "/dir");
    test_write_file(root + "/dir/home.html", "home\n");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 200 OK");
    cr_assert_neq(response.find("home\n"), std::string::npos);
}

Test(dispatcher, unreadable_autoindex_directory_returns_forbidden)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root, true);
    http::request req = make_request(http::methods::GET, "/dir/");

    test_mkdir(root + "/dir");
    cr_assert_eq(chmod((root + "/dir").c_str(), 0100), 0,
        "chmod() failed: %s", strerror(errno));

    std::string response = dispatcher::handle(req, server);

    cr_assert_eq(chmod((root + "/dir").c_str(), 0700), 0,
        "chmod() failed: %s", strerror(errno));
    test_assert_status(response, "HTTP/1.1 403 Forbidden");
}

Test(dispatcher, too_long_path_returns_bad_request)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root);
    http::request req
        = make_request(http::methods::GET, "/" + std::string(5000, 'a'));

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 400 Bad Request");
}

Test(dispatcher, path_below_file_returns_not_found)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root);
    http::request req = make_request(http::methods::GET, "/file.txt/child");

    test_write_file(root + "/file.txt", "body\n");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 404 Not Found");
}
