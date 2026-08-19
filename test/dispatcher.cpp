/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dispatcher.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/20 16:16:07 by mle-flem          #+#    #+#             */
/*   Updated: 2026/08/19 21:45:34 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "dispatcher.hpp"

#include <criterion/criterion.h>

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "test_helpers.hpp"

static Server make_dispatch_server(const std::string &root,
    bool autoindex = false, const std::string &index = "",
    std::size_t client_max_body_size = 0)
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
    config.client_max_body_size = client_max_body_size;
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
    cr_assert_eq(
        mkdir(path.c_str(), 0700), 0, "mkdir() failed: %s", strerror(errno));
}

static Server make_upload_server(const std::string &root,
    const std::string &upload_path, std::size_t client_max_body_size = 0,
    bool autoindex = false, bool allow_delete = true)
{
    Config config = { };
    std::vector<Location> locations;

    config.root = root;
    config.upload_path = upload_path;
    config.client_max_body_size = client_max_body_size;
    config.autoindex = autoindex;
    config.allowed_methods[http::methods::GET] = true;
    config.allowed_methods[http::methods::POST] = true;
    config.allowed_methods[http::methods::DELETE] = allow_delete;
    return Server(locations, "test", "127.0.0.1:0", config);
}

static std::string test_read_file(const std::string &path)
{
    std::ifstream in(path.c_str(), std::ios::binary);

    cr_assert(in.is_open(), "failed to open %s", path.c_str());
    return std::string(
        std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
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

Test(dispatcher, location_root_maps_uri_suffix_under_root)
{
    std::string server_root = test_tmpdir("webserv-dispatcher-test");
    std::string location_root = test_tmpdir("webserv-location-root-test");
    Config config = { };
    Location location;
    std::vector<Location> locations;
    http::request req = make_request(http::methods::GET, "/kapouet/pouic");

    config.root = server_root;
    config.allowed_methods[http::methods::GET] = true;
    location.path = "/kapouet";
    location.config = config;
    location.config.root = location_root;
    location.config.conf.root = std::vector<std::string>(1, location_root);
    location.type = location::CLASSIC;
    locations.push_back(location);
    Server server(locations, "test", "127.0.0.1:0", config);
    test_write_file(location_root + "/pouic", "location body\n");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 200 OK");
    cr_assert_neq(response.find("location body\n"), std::string::npos);
}

Test(dispatcher, disallowed_cgi_method_returns_response_outcome)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root);
    http::request req = make_request(http::methods::DELETE, "/cgi/app.sh");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 405 Method Not Allowed");
}

Test(dispatcher, location_return_redirects_before_method_checks)
{
    Config config = { };
    Location location;
    std::vector<Location> locations;
    http::request req = make_request(http::methods::DELETE, "/old/path");

    location.path = "/old";
    location.config = config;
    location.config.redirect_status = 307;
    location.config.redirect_target = "https://example.test/new";
    location.type = location::CLASSIC;
    locations.push_back(location);
    Server server(locations, "test", "127.0.0.1:0", config);

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 307 Temporary Redirect");
    cr_assert_neq(response.find("Location: https://example.test/new\r\n"),
        std::string::npos);
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
    cr_assert_eq(response.find("deleteEntry("), std::string::npos);
}

Test(dispatcher, autoindex_upload_location_shows_delete_buttons_for_files)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Config config = { };
    Location location;
    std::vector<Location> locations;
    http::request req = make_request(http::methods::GET, "/upload/");

    test_mkdir(root + "/dir");
    config.root = root;
    location.path = "/upload";
    location.config = config;
    location.config.root = root + "/dir/";
    location.config.upload_path = root + "/dir/";
    location.config.autoindex = true;
    location.config.allowed_methods[http::methods::GET] = true;
    location.config.allowed_methods[http::methods::DELETE] = true;
    location.config.conf.root.push_back(root + "/dir/");
    location.type = location::CLASSIC;
    locations.push_back(location);
    Server server(locations, "test", "127.0.0.1:0", config);
    test_mkdir(root + "/dir/subdir");
    test_write_file(root + "/dir/file.txt", "x\n");
    cr_assert_eq(symlink("file.txt", (root + "/dir/link.txt").c_str()), 0,
        "symlink() failed: %s", strerror(errno));

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 200 OK");
    cr_assert_neq(response.find("deleteEntry('file.txt')"), std::string::npos);
    cr_assert_eq(response.find("deleteEntry('link.txt')"), std::string::npos);
    cr_assert_eq(response.find("deleteEntry('subdir')"), std::string::npos);
}

Test(dispatcher, autoindex_upload_location_hides_delete_if_delete_disallowed)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    http::request req = make_request(http::methods::GET, "/dir/");

    test_mkdir(root + "/dir");
    Server server = make_upload_server(root, root + "/dir", 0, true, false);
    test_write_file(root + "/dir/file.txt", "x\n");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 200 OK");
    cr_assert_eq(response.find("deleteEntry("), std::string::npos);
}

Test(dispatcher, autoindex_upload_location_hides_delete_outside_upload_path)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    http::request req = make_request(http::methods::GET, "/dir/");

    test_mkdir(root + "/dir");
    test_mkdir(root + "/uploads");
    Server server = make_upload_server(root, root + "/uploads", 0, true);
    test_write_file(root + "/dir/file.txt", "x\n");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 200 OK");
    cr_assert_eq(response.find("deleteEntry("), std::string::npos);
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
    cr_assert_eq(chmod((root + "/dir").c_str(), 0100), 0, "chmod() failed: %s",
        strerror(errno));

    std::string response = dispatcher::handle(req, server);

    cr_assert_eq(chmod((root + "/dir").c_str(), 0700), 0, "chmod() failed: %s",
        strerror(errno));
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

Test(dispatcher, raw_upload_writes_uri_basename_to_upload_path)
{
    std::string root = test_tmpdir("webserv-upload-test");
    std::string upload = root + "/uploads";
    cr_assert_eq(mkdir(upload.c_str(), 0700), 0);
    Server server = make_upload_server(root, upload);
    http::request req = make_request(http::methods::POST, "/api/photo.bin");

    req.body = std::string("raw\0body", 8);

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 201 Created");
    cr_assert_eq(test_read_file(upload + "/photo.bin"), req.body);
}

Test(dispatcher, multipart_upload_writes_file_parts_and_ignores_fields)
{
    std::string root = test_tmpdir("webserv-upload-test");
    std::string upload = root + "/uploads";
    cr_assert_eq(mkdir(upload.c_str(), 0700), 0);
    Server server = make_upload_server(root, upload);
    http::request req = make_request(http::methods::POST, "/upload");

    req.headers["content-type"] = "multipart/form-data; boundary=abc123";
    req.body = "--abc123\r\n"
               "Content-Disposition: form-data; name=\"title\"\r\n"
               "\r\n"
               "ignored\r\n"
               "--abc123\r\n"
               "Content-Disposition: form-data; name=\"file\"; "
               "filename=\"hello.txt\"\r\n"
               "Content-Type: text/plain\r\n"
               "\r\n"
               "hello upload\r\n"
               "--abc123--\r\n";

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 201 Created");
    cr_assert_eq(test_read_file(upload + "/hello.txt"), "hello upload");
}

Test(dispatcher, multipart_upload_rejects_unsafe_filename)
{
    std::string root = test_tmpdir("webserv-upload-test");
    std::string upload = root + "/uploads";
    cr_assert_eq(mkdir(upload.c_str(), 0700), 0);
    Server server = make_upload_server(root, upload);
    http::request req = make_request(http::methods::POST, "/upload");

    req.headers["content-type"] = "multipart/form-data; boundary=abc123";
    req.body = "--abc123\r\n"
               "Content-Disposition: form-data; name=\"file\"; "
               "filename=\"../bad.txt\"\r\n"
               "\r\n"
               "bad\r\n"
               "--abc123--\r\n";

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 400 Bad Request");
}

Test(dispatcher, upload_rejects_body_larger_than_configured_limit)
{
    std::string root = test_tmpdir("webserv-upload-test");
    std::string upload = root + "/uploads";
    cr_assert_eq(mkdir(upload.c_str(), 0700), 0);
    Server server = make_upload_server(root, upload, 3);
    http::request req = make_request(http::methods::POST, "/file.txt");

    req.body = "toolong";

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 413 Payload Too Large");
}

Test(dispatcher, delete_upload_removes_uri_basename_from_upload_path)
{
    std::string root = test_tmpdir("webserv-upload-test");
    std::string upload = root + "/uploads";
    cr_assert_eq(mkdir(upload.c_str(), 0700), 0);
    Server server = make_upload_server(root, upload);
    http::request req = make_request(http::methods::DELETE, "/api/photo.bin");

    test_write_file(upload + "/photo.bin", "old\n");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 204 No Content");
    cr_assert_eq(access((upload + "/photo.bin").c_str(), F_OK), -1);
}

Test(dispatcher, delete_upload_missing_file_returns_not_found)
{
    std::string root = test_tmpdir("webserv-upload-test");
    std::string upload = root + "/uploads";
    cr_assert_eq(mkdir(upload.c_str(), 0700), 0);
    Server server = make_upload_server(root, upload);
    http::request req = make_request(http::methods::DELETE, "/missing.txt");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 404 Not Found");
}

Test(dispatcher, delete_upload_rejects_directory_target)
{
    std::string root = test_tmpdir("webserv-upload-test");
    std::string upload = root + "/uploads";
    cr_assert_eq(mkdir(upload.c_str(), 0700), 0);
    cr_assert_eq(mkdir((upload + "/dir").c_str(), 0700), 0);
    Server server = make_upload_server(root, upload);
    http::request req = make_request(http::methods::DELETE, "/dir");

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 403 Forbidden");
}

Test(dispatcher, non_upload_request_uses_configured_body_limit)
{
    std::string root = test_tmpdir("webserv-dispatcher-test");
    Server server = make_dispatch_server(root, false, "", 3);
    http::request req = make_request(http::methods::POST, "/static.txt");

    req.body = "toolong";

    std::string response = dispatcher::handle(req, server);

    test_assert_status(response, "HTTP/1.1 413 Payload Too Large");
}
