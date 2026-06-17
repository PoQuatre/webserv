/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parser.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 10:20:45 by nlaporte          #+#    #+#             */
/*   Updated: 2026/06/17 19:05:01 by nlaporte         ###   ########.fr       */
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

bool string_check(const std::string &val)
{
    (void)val;
    return true;
}

bool int_check(const std::string &val)
{
    for (int i = 0; val[i]; i++) {
        if (!std::isdigit(val[i]))
            return false;
    }
    return true;
}

bool bool_check(const std::string &val)
{
    return (val == "on" || val == "off");
}

bool time_check(const std::string &val)
{
    bool valid = false;
    uint32_t i = 0;

    while (1) {
        while (val[i] && std::isdigit(val[i])) {
            i++;
        }
        if (i + 1 < val.length() && val[i] == 'm' && (val[i + 1]) == 's') {
            valid = true;
            i += 2;
        } else {
            switch (val[i]) {
            case 's':
            case 'm':
            case 'h':
                /*
            case 'd':
            case 'w':
            case 'M':
            case 'y':
*/
                i++;
                valid = true;
                break;
            default:
                return (!val[i]);
            }
        }
        if (i == val.length())
            return valid;
    }
    return valid;
}

std::size_t convert_time_to_size_t(const std::string &val)
{
    int64_t i = 0;
    std::size_t final_size = 0;
    const char *str = val.c_str();
    char *p;

    while (1) {
        i = std::strtol(str, &p, 10);
        if (i < 0)
            return 9;
        str = p;
        if (!*str)
            return final_size + (i / 1000);
        if (str[1] && str[0] == 'm' && str[1] == 's') {
            final_size += (i / 1000);
            str += 2;
        } else {
            switch (*str) {
            case 's':
                final_size += i;
                str++;
                break;
            case 'm':
                final_size += i * 60;
                str++;
                break;
            case 'h':
                final_size += i * 3600;
                str++;
                break;
                /*
        case 'd':
        case 'w':
        case 'M':
        case 'y':
                */
            default:
                return final_size;
            }
        }
        if (!*p)
            return final_size;
    }
    return 0;
}

bool size_check(const std::string &val)
{
    (void)convert_time_to_size_t;
    std::size_t i;
    for (i = 0; val[i]; i++) {
        if (!std::isdigit(val[i]))
            break;
    }
    if (!val[i])
        return true;
    if ((val[i] == 'k' || val[i] == 'K') && i + 1 == (val.size()))
        return true;
    if ((val[i] == 'm' || val[i] == 'M') && i + 1 == (val.size()))
        return true;
    if ((val[i] == 'g' || val[i] == 'G') && i + 1 == (val.length()))
        return true;
    return (std::isdigit(val[i]));
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

Parser::Parser(const std::string &path)
    : _path(path)
    , _root(NULL)
    , _act_token(NULL)
    , _err_count(0)
    , _depth(0)
    , _valid(true)
{
    L_TRACE("configuration file: {}", _path);
}

Parser::~Parser() { free_tree(_root); }

bool Parser::parse_config()
{
    Location location = { };

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
    return _valid;
}
