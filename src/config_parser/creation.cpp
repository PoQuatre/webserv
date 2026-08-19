/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   creation.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/17 18:57:11 by nlaporte          #+#    #+#             */
/*   Updated: 2026/08/19 21:55:49 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <regex.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <stack>
#include <string>
#include <vector>

#include "Server.hpp"
#include "config-parser-def.hpp"
#include "config-parser.hpp"
#include "http.hpp"
#include "logger.hpp"

namespace {

void add_value_to_config(const config_node &node, Config &conf)
{
    switch (node.keyword) {
#define X(type, name, ...)                                                     \
    case keywords::type:                                                       \
        conf.conf.name = node.vals;                                            \
        break;
#define X_SPECIAL(...)
        KEYWORDS
    default:
        break;
#undef X_SPECIAL
#undef X
    }
}

std::size_t convert_string_to_size(const std::string &val)
{
    int64_t i;
    char *p;

    i = std::strtol(val.c_str(), &p, 10);
    if (!*p)
        return i;
    if (*p == 'k' || *p == 'K')
        return (INT64_MAX / 1024 < i ? INT64_MAX : i * 1024);
    if (*p == 'm' || *p == 'M')
        return (INT64_MAX / 1048576 < i ? INT64_MAX : i * 1048576);
    if ((*p == 'g' || *p == 'G'))
        return (INT64_MAX / 1073741824 < i ? INT64_MAX : i * 1073741824);
    return 0;
}

std::size_t convert_time_to_seconds(const std::string &val)
{
    int64_t amount = 0;
    std::size_t total_seconds = 0;
    const char *cursor = val.c_str();
    char *end;

    while (1) {
        amount = std::strtol(cursor, &end, 10);
        if (amount < 0)
            return 0;
        cursor = end;
        if (!*cursor)
            return total_seconds + (amount / 1000);
        if (cursor[1] && cursor[0] == 'm' && cursor[1] == 's') {
            total_seconds += (amount / 1000);
            cursor += 2;
        } else {
            switch (*cursor) {
            case 's':
                total_seconds += amount;
                cursor++;
                break;
            case 'm':
                total_seconds += amount * 60;
                cursor++;
                break;
            case 'h':
                total_seconds += amount * 3600;
                cursor++;
                break;
            default:
                return total_seconds;
            }
        }
        if (!*end)
            return total_seconds;
    }
    return 0;
}
}

std::string ConfigParser::handle_relative_path(const std::string &path)
{
    if (path.empty() || path[0] == '/')
        return path;
    std::size_t pos = _path.rfind('/');
    if (pos == std::string::npos)
        return path;
    return _path.substr(0, pos + 1) + path;
}

void ConfigParser::push_configuration(const config_node &node,
    std::map<keywords::type, std::vector<std::string> > &conf)
{
    for (std::vector<config_node *>::const_iterator cit = node.children.begin();
        cit != node.children.end(); cit++) {

        if ((*cit)->type == LEAF) {
            if ((*cit)->keyword == keywords::ROOT
                || (*cit)->keyword == keywords::CGI_PASS
                || (*cit)->keyword == keywords::CLIENT_BODY_TEMP_PATH) {
                for (std::vector<std::string>::iterator cit2
                    = (*cit)->vals.begin();
                    cit2 != (*cit)->vals.end(); cit2++) {
                    *cit2 = handle_relative_path(*cit2);
                }
            }
            if ((*cit)->keyword == keywords::SERVER_NAME
                && conf.find((*cit)->keyword) != conf.end()) {
                conf[(*cit)->keyword].insert(conf[(*cit)->keyword].end(),
                    (*cit)->vals.begin(), (*cit)->vals.end());
            } else {
                conf[(*cit)->keyword] = (*cit)->vals;
            }
        }
    }
}

void ConfigParser::set_location_value(
    const config_node &node, Config &location_conf)
{
    switch (node.keyword) {
    case keywords::ROOT:
        location_conf.root = handle_relative_path(*(node.vals.begin()));
        break;
    case keywords::UPLOAD_PATH:
        location_conf.upload_path = handle_relative_path(*(node.vals.begin()));
        break;
    case keywords::AUTOINDEX:
        location_conf.autoindex = (*node.vals.begin()) == "on";
        break;
    case keywords::CLIENT_MAX_BODY_SIZE:
        location_conf.client_max_body_size
            = convert_string_to_size(*node.vals.begin());
        break;
    case keywords::CGI_PASS:
        location_conf.cgi_pass = handle_relative_path(*(node.vals.begin()));
        break;
    case keywords::CGI_TIMEOUT:
        location_conf.cgi_timeout = convert_time_to_seconds(*node.vals.begin());
        break;
    case keywords::CGI_OUTPUT_BUFFER_SIZE:
        location_conf.cgi_output_buffer_size
            = convert_string_to_size(*node.vals.begin());
        break;
    case keywords::RETURN:
        location_conf.redirect_status
            = std::strtol(node.vals[0].c_str(), NULL, 10);
        location_conf.redirect_target = node.vals[1];
        break;
    default:
        return;
    }
}

void ConfigParser::create_location(
    std::vector<config_node *>::const_iterator &node_it, Config &location_conf)
{
    char *p;

    // iterate on children location node
    for (std::vector<config_node *>::iterator it = (*node_it)->children.begin();
        it != (*node_it)->children.end(); it++) {

        add_value_to_config(**it, location_conf);

        if ((*it)->keyword == keywords::ERROR_PAGE) {
            // node->value ERROR_PAGE
            for (std::vector<std::string>::iterator it2 = (*it)->vals.begin();
                it2 != (*it)->vals.end(); it2++) {
                std::size_t code = std::strtol((*it2).c_str(), &p, 10);
                if (code < 512 && code > 1) {
                    location_conf.error_pages[code] = *((*it)->vals.end() - 1);
                }
            }
        }

        set_location_value(**it, location_conf);

        if ((*it)->keyword != keywords::LIMIT_EXCEPT)
            continue;
        std::memset(location_conf.allowed_methods, 0,
            sizeof(bool) * http::methods::COUNT);
        // node->values LIMIT_EXCEPT
        for (std::vector<std::string>::iterator it2 = (*it)->vals.begin();
            it2 != (*it)->vals.end(); it2++) {
            for (std::size_t i = 0; i < http::methods::COUNT; i++)
                if (*it2 == http::methods::strings[i]) {
                    location_conf.allowed_methods[i] = 1;
                }
        }
    }
}

void ConfigParser::create_all_location(const config_node &node,
    Config &inital_config, std::vector<Location> &location_vector)
{
    // Iter on server children node
    for (std::vector<config_node *>::const_iterator cit = node.children.begin();
        cit != node.children.end(); cit++) {

        // Create new location from server configuration
        if ((*cit)->keyword == keywords::LOCATION) {
            Config location_conf = inital_config;
            Location location_struct;

            location_struct.type = (*cit)->location_type;
            location_struct.regexp = (*cit)->location_regexp;
            if ((*cit)->vals.empty()) {
                location_struct.path = "/";
            } else {
                location_struct.path = *(*cit)->vals.begin();
            }
            location_conf.conf.root.clear();

            create_location(cit, location_conf);

            location_struct.config = location_conf;
            location_vector.push_back(location_struct);
        }
    }
}

namespace {
void initalize_server_config(
    std::map<keywords::type, std::vector<std::string> > &server_conf,
    Config &inital_config)
{
    if (server_conf.find(keywords::ROOT) != server_conf.end())
        inital_config.root = *server_conf.find(keywords::ROOT)->second.begin();

    if (server_conf.find(keywords::UPLOAD_PATH) != server_conf.end())
        inital_config.upload_path
            = *server_conf.find(keywords::UPLOAD_PATH)->second.begin();

    // Configure error_pages on server config
    if (server_conf.find(keywords::ERROR_PAGE) != server_conf.end()) {
        std::vector<std::string> error_page_vec
            = server_conf.find(keywords::ERROR_PAGE)->second;
        for (std::vector<std::string>::iterator it2 = error_page_vec.begin();
            it2 != error_page_vec.end(); it2++) {
            char *p;
            std::size_t code = std::strtol((*it2).c_str(), &p, 10);
            if (code < 512 && code > 1) {
                inital_config.error_pages[code] = *(error_page_vec.end() - 1);
            }
        }
    }

    if (server_conf.find(keywords::AUTOINDEX) != server_conf.end()) {
        inital_config.autoindex
            = *server_conf.find(keywords::AUTOINDEX)->second.begin() == "on";
    }

    if (server_conf.find(keywords::CLIENT_MAX_BODY_SIZE) != server_conf.end()) {
        std::vector<std::string> vals
            = server_conf.find(keywords::CLIENT_MAX_BODY_SIZE)->second;
        for (std::vector<std::string>::iterator it = vals.begin();
            it != vals.end(); it++) {
            if (convert_string_to_size(*it) > 0) {
                inital_config.client_max_body_size
                    += convert_string_to_size(*it);
            } else {
                inital_config.client_max_body_size = 0;
                break;
            }
        }
    }

    for (std::map<keywords::type, std::vector<std::string> >::iterator it
        = server_conf.begin();
        it != server_conf.end(); it++) {
        switch (it->first) {
#define X(type, name, ...)                                                     \
    case keywords::type:                                                       \
        inital_config.conf.name = it->second;                                  \
        break;
#define X_SPECIAL(...)
            KEYWORDS
        default:
            break;
#undef X_SPECIAL
#undef X
        }
    }
}
}

std::vector<Server> ConfigParser::get_all_servers(std::vector<Server> &servers)
{
    for (std::vector<Server>::iterator it = _servers.begin();
        it != _servers.end(); it++)
        servers.push_back(*it);
    return _servers;
}

void ConfigParser::create_one_server(const config_node &node,
    std::vector<Location> location_vector,
    std::map<keywords::type, std::vector<std::string> > &server_conf)
{
    Config inital_config = { };

    inital_config.root = "/";
    inital_config.cgi_timeout = DEFAULT_CGI_TIMEOUT;
    inital_config.cgi_output_buffer_size = DEFAULT_CGI_OUTPUT_BUFFER_SIZE;
    std::memset(
        inital_config.allowed_methods, 1, sizeof(bool) * http::methods::COUNT);

    initalize_server_config(server_conf, inital_config);

    create_all_location(node, inital_config, location_vector);

    if (server_conf.find(keywords::ROOT) == server_conf.end())
        L_WARN("No server root using '{}' as the default", DEFAULT_ROOT);
    if (server_conf.find(keywords::SERVER_NAME) == server_conf.end())
        L_WARN("No server name using '{}' as the default", DEFAULT_SERVER_NAME);
    if (server_conf.find(keywords::LISTEN) == server_conf.end())
        L_WARN("No listen address, using '{}' as the default ", DEFAULT_LISTEN);

    std::map<keywords::type, std::vector<std::string> >::const_iterator names
        = server_conf.find(keywords::SERVER_NAME);
    std::map<keywords::type, std::vector<std::string> >::const_iterator listen
        = server_conf.find(keywords::LISTEN);
    std::vector<std::string> server_names;
    if (names == server_conf.end())
        server_names.push_back(DEFAULT_SERVER_NAME);
    else
        server_names = names->second;

    Server n_server = Server(location_vector, server_names,
        listen != server_conf.end() ? *listen->second.begin() : DEFAULT_LISTEN,
        inital_config);

    _servers.push_back(n_server);
}

bool ConfigParser::create_all_servers()
{
    std::map<keywords::type, std::vector<std::string> > global_conf;
    std::map<keywords::type, std::vector<std::string> > http_conf;
    std::map<keywords::type, std::vector<std::string> > server_conf;
    std::vector<Location> location_vector;
    std::stack<config_node *> node_stack;
    config_node *node;

    node_stack.push(_root);
    while (!node_stack.empty()) {
        node = node_stack.top();
        node_stack.pop();

        if (node->type == NODE || node->type == ROOT) {
            for (std::vector<config_node *>::iterator it
                = node->children.begin();
                it != node->children.end(); it++) {
                if ((*it)->type == NODE) {
                    node_stack.push(*it);
                }
            }
        }

        switch (node->keyword) {
        case keywords::GLOBAL:
            push_configuration(*node, global_conf);
            break;
        case keywords::HTTP:
            http_conf = global_conf;
            push_configuration(*node, http_conf);
            break;
        case keywords::SERVER:
            server_conf = http_conf;
            push_configuration(*node, server_conf);
            create_one_server(*node, location_vector, server_conf);
            break;
        default:
            break;
        }
    }

    if (_servers.empty()) {
        L_ERROR(" no server configuration");
        return false;
    }
    L_TRACE("{} server(s) created", _servers.size());
    return true;
}
