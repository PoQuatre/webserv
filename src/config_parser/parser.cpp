/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parser.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 10:20:45 by nlaporte          #+#    #+#             */
/*   Updated: 2026/06/18 19:50:53 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <regex.h>

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Server.hpp"
#include "config-parser-def.hpp"
#include "config-parser.hpp"
#include "logger.hpp"

/*   ____ ___  _   _ _____ ___ ____   ____   _    ____  ____  _____ ____   */
/*  / ___/ _ \| \ | |  ___|_ _/ ___| |  _ \ / \  |  _ \/ ___|| ____|  _ \  */
/* | |  | | | |  \| | |_   | | |  _  | |_) / _ \ | |_) \___ \|  _| | |_) | */
/* | |__| |_| | |\  |  _|  | | |_| | |  __/ ___ \|  _ < ___) | |___|  _ <  */
/*  \____\___/|_| \_|_|   |___\____| |_| /_/   \_\_| \_\____/|_____|_| \_\ */

ConfigParser::ConfigParser(const std::string &path)
    : _path(path)
    , _root(NULL)
    , _act_token(NULL)
    , _err_count(0)
    , _depth(0)
    , _valid(true)
{
    L_TRACE("configuration file: {}", _path);
}

ConfigParser::~ConfigParser() { free_tree(_root); }

bool ConfigParser::start()
{
    if (!tokenize()) {
        return false;
    }

    if (_tokens.empty()) {
        L_ERROR("empty configuration file");
        return false;
    }

    try {
        _root = new config_node();
        _root->key = "GLOBAL";
        _root->type = ROOT;
        _root->keyword = keywords::GLOBAL;
    } catch (...) {
        L_ERROR("Can't alloc inital node");
        return false;
    }

    if (!token_to_tree()) {
        L_ERROR("invalid configuration file {} error(s)", _err_count);
        return false;
    }
#ifndef TESTING
    if (logger::log_level() == logger::levels::TRACE)
        debug_tree(_root, 0);
#endif

    if (!create_all_servers()) {
        return false;
    }
    return _valid;
}

bool ConfigParser::parse_config(
    const std::string &path, std::vector<Server> &servers)
{
    Location location = { };
    ConfigParser parser(path);
    if (!parser.start())
        return false;
    parser.get_all_servers(servers);
    return true;
}
