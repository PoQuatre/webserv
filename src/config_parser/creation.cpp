/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   creation.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/17 18:57:11 by nlaporte          #+#    #+#             */
/*   Updated: 2026/06/18 20:06:29 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <regex.h>

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

#ifndef DTESTING
namespace {
#endif

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

void push_configuration(const config_node &node,
    std::map<keywords::type, std::vector<std::string> > &conf)
{
    for (std::vector<config_node *>::const_iterator cit = node.children.begin();
        cit != node.children.end(); cit++) {

        if ((*cit)->type == LEAF) {
            conf.insert(std::pair<keywords::type, std::vector<std::string> >(
                (*cit)->keyword, (*cit)->vals));
        }
    }
}

void set_location_value(const config_node &node, Config &location_conf)
{
    char *p;
    switch (node.keyword) {
    case keywords::ROOT:
        location_conf.root = *(node.vals.begin());
        break;
    case keywords::AUTOINDEX:
        location_conf.autoindex = (*node.vals.begin()) == "on";
        break;
    case keywords::CLIENT_MAX_BODY_SIZE:
        location_conf.client_max_body_size
            = std::strtol(node.vals.begin()->c_str(), &p, 10);
        // TODO: handle error
        break;
    default:
        return;
    }
}

void create_location(
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

        if ((*it)->keyword == keywords::LIMIT_EXCEPT) {
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
}

void create_all_location(const config_node &node, Config &inital_config,
    std::vector<Location> &location_vector)
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

            create_location(cit, location_conf);

            location_struct.config = location_conf;
            location_vector.push_back(location_struct);
        }
    }
}

void initalize_server_config(
    std::map<keywords::type, std::vector<std::string> > &server_conf,
    Config &inital_config)
{
    if (server_conf.find(keywords::ROOT) != server_conf.end())
        inital_config.root = *server_conf.find(keywords::ROOT)->second.begin();

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
    } else {
        inital_config.autoindex = false;
    }

    if (server_conf.find(keywords::CLIENT_MAX_BODY_SIZE) != server_conf.end()) {
        std::vector<std::string> vals
            = server_conf.find(keywords::CLIENT_MAX_BODY_SIZE)->second;
        inital_config.client_max_body_size = 0;
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
    } else {
        inital_config.client_max_body_size = 0;
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

#ifndef DTESTING
}
#endif

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
    Config inital_config;

    inital_config.root = "/";
    inital_config.autoindex = false;
    inital_config.client_max_body_size = 0;
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

    Server n_server = Server(location_vector,
        server_conf.find(keywords::SERVER_NAME) != server_conf.end()
            ? *server_conf.find(keywords::SERVER_NAME)->second.begin()
            : DEFAULT_SERVER_NAME,
        server_conf.find(keywords::LISTEN) != server_conf.end()
            ? *server_conf.find(keywords::LISTEN)->second.begin()
            : DEFAULT_LISTEN,
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
