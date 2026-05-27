/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config-parser.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 10:20:45 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/27 09:16:22 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config-parser.hpp"

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <stack>
#include <string>
#include <vector>

#include "Server.hpp"
#include "config-parser-def.hpp"
#include "http.hpp"
#include "logger.hpp"

#ifndef TESTING
namespace {
#endif

const char *strings[] = {
#define X(_, __, ___, text, ____, _____, ______, _______, ________) #text,
    KEYWORDS
#undef X
};

const char *keywords_strs[] = {
#define X(_, text, ___, __, ____, _____, ______, _______, ________) text,
    KEYWORDS
#undef X
};

const int keywords_scope_rules[] = {
#define X(_, _____, ___, __, http, server, location, _______, ________)        \
    ((http) + ((server) << 1) + ((location) << 2)),
    KEYWORDS
#undef X
};

const std::size_t COUNT = sizeof(keywords_strs) / sizeof(*keywords_strs);

tokens::type config_get_token_type(config_token token)
{
    switch (token.value[0]) {
#define X(name, val)                                                           \
    case val:                                                                  \
        return tokens::name;
        TOKENS // NOLINT (bugprone-branch-clone) switch 2 identical branches
#undef X
            default : return tokens::WORD;
    }
}

keywords::type config_get_token_keyword(const config_token &token)
{
    for (std::size_t i = 0; i < COUNT; i++) {
        if (keywords_strs[i] == token.value)
            return static_cast<keywords::type>(i);
    }
    return keywords::UNKNOWN;
}

location::type config_get_location_type(const std::string &t_str)
{
#define X(val, str)                                                            \
    if (t_str == (str)) {                                                      \
        return location::val;                                                  \
    }
    LOCATION_TYPE
#undef X
    return location::CLASSIC;
}

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

bool bool_check(const std::string &string)
{
    return (string == "on" || string == "off");
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

bool config_is_special_char(char c)
{
    switch (c) {
#define X(_, val) case val:
        TOKENS
#undef X
        return true;
    default:
        return false;
    }
}

void push_configuration(const config_node &node,
    std::map<keywords::type, std::vector<std::string> > &conf)
{
    for (std::vector<config_node *>::const_iterator it = node.children.begin();
        it != node.children.end(); it++) {

        if ((*it)->type == LEAF) {
            conf.insert(std::pair<keywords::type, std::vector<std::string> >(
                (*it)->keyword, (*it)->vals));
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
    for (std::vector<config_node *>::const_iterator it = node.children.begin();
        it != node.children.end(); it++) {

        // Create new location from server configuration
        if ((*it)->keyword == keywords::LOCATION) {
            Config location_conf = inital_config;
            Location location_struct;

            location_struct.exact = (*it)->location_type;
            if ((*it)->vals.empty()) {
                location_struct.path = "/";
            } else {
                location_struct.path = *(*it)->vals.begin();
            }

            create_location(it, location_conf);

            location_struct.config = location_conf;
            location_vector.push_back(location_struct);
        }
    }
}

bool node_already_present(std::vector<config_node *> &node,
    const keywords::type &key, bool delete_node = false)
{
    if (key == keywords::ERROR_PAGE || key == keywords::SERVER_NAME)
        return false;
    for (std::vector<config_node *>::iterator it = node.begin();
        it != node.end(); it++) {
        if ((*it)->keyword == key) {
            if (delete_node)
                node.erase(it);
            return true;
        }
    }
    return false;
}

void initalize_server_config(
    std::map<keywords::type, std::vector<std::string> > &server_conf,
    Config &inital_config)
{
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
}

bool directive_is_in_valide_scope(
    keywords::type root_keyword, keywords::type directive)
{
    if (directive == keywords::HTTP && keywords::GLOBAL == root_keyword)
        return true;
    switch (root_keyword) {
    case keywords::HTTP:
        return (1 & keywords_scope_rules[directive]);

    case keywords::SERVER:
        return (2 & keywords_scope_rules[directive]);

    case keywords::LOCATION:
        return (4 & keywords_scope_rules[directive]);

    default:
        return false;
    }
}

uint32_t get_max_args(const config_token &token)
{
    if (token.value.empty())
        return 1;
    switch (token.keyword) {
#define X(name, _, arg, __, ___, ____, _____, _______, ______)                 \
    case keywords::name:                                                       \
        return arg;
        KEYWORDS // NOLINT (bugprone-branch-clone) switch 2 identical branches
#undef X
    }
    return 1;
}

bool is_a_valid_val(std::string &val, const config_token &token)
{
    (void)val;
    switch (token.keyword) {
#define X(name, _, _______, __, ___, ____, _____, check, ______)               \
    case keywords::name:                                                       \
        return check(val);
        KEYWORDS // NOLINT (bugprone-branch-clone) switch 2 identical branches
#undef X
            default : return true;
    }
}

#ifndef TESTING
}
#endif

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

void Parser::skip_line()
{
    while (_act_token->type != tokens::END
        && _act_token->type != tokens::BRACE_CLOSE
        && _act_token->type != tokens::BRACE_OPEN) {
        if (!consume_next_token()) {
            break;
        }
        if (!see_next_token())
            break;
    }
    if (_act_token->type == tokens::END)
        consume_next_token();
}

bool Parser::see_next_token()
{
    for (std::vector<config_token>::iterator it = _tokens.begin();
        it != _tokens.end(); it++) {
        if (it->alive) {
            _act_token = &(*it);
            return true;
        }
    }
    return false;
}

bool Parser::consume_next_token()
{
    for (std::vector<config_token>::iterator it = _tokens.begin();
        it != _tokens.end(); it++) {
        if (it->alive) {
            it->alive = 0;
            _act_token = &(*it);
            return true;
        }
    }
    return false;
}

void Parser::config_set_alive_last_token()
{
    if (_tokens.empty())
        return;
    for (std::vector<config_token>::iterator ite = _tokens.end() - 1;
        ite != _tokens.begin(); ite--) {
        if (!ite->alive) {
            ite->alive = 1;
            return;
        }
    }
}

// ignoring undefined scope
// scope_name is copy of token->str for prevent this value from being changed
void Parser::skip_scope(
    const std::string &scope_name, uint32_t line, bool print_err)
{
    int depth = 0;
    bool has_start = false;

    while (consume_next_token()) {
        if (_act_token->type == tokens::BRACE_OPEN) {
            has_start = true;
            depth++;
        } else if (_act_token->type == tokens::BRACE_CLOSE) {
            depth--;
        }
        if (depth == 0 && has_start) {
            if (print_err)
                L_ERROR("invalid scope '{}' from line {}, to line {}, has "
                        "been ignored",
                    scope_name, line, _act_token->line);
            _err_count++;
            _valid = false;
            return;
        }
    }
}

bool Parser::create_location_node()
{
    config_node *node;
    uint32_t line = _act_token->line;

    // check scope validity in scope
    if (!directive_is_in_valide_scope(_root->keyword, _act_token->keyword)) {
        L_ERROR("scope '{}' (line {}) is in a bad scope '{}' (line {}) {}",
            _act_token->value, line, _root->key, _root->line);
        skip_scope(_act_token->value, line);
        return true;
    }

    // See next token without consume them
    if (!see_next_token())
        return true;

    // Manage no arg location
    if (_act_token->type == tokens::BRACE_OPEN) {
        L_ERROR("location directive requires a path (line {}) {}", line,
            strings[keywords::LOCATION]);
        skip_scope("location", line, true);
        return true;
    }

    // Initialize node
    try {
        node = new config_node();
        node->key = "location";
        node->line = line;
        node->type = NODE;
        node->parent = _root;
        node->keyword = keywords::LOCATION;
    } catch (...) {
        return false;
    }

    //  Handle strict location
    // if (_act_token->value == "=" || _act_token->value == "~") {

    node->location_type = config_get_location_type(_act_token->value);
    if (node->location_type != location::CLASSIC) {
        consume_next_token();
        // No other token, token_to_tree() handle scope error
        if (!see_next_token()) {
            delete node;
            return true;
        }
    }

    // if next token is bad, print an error and abort current line parsing
    if (_act_token->type != tokens::WORD) {
        if (node->location_type != location::CLASSIC)
            L_ERROR("{} location need one parameter (line {})",
                location::strings[node->location_type], line);
        else
            L_ERROR(
                "location need one parameter or BRACE_OPEN (line {})", line);
        _err_count++;
        _valid = false;
        skip_scope(_act_token->value, line);
        delete node;
        return true;
    }

    // push last child
    node->vals.push_back(_act_token->value);
    consume_next_token();
    if (!see_next_token()) {
        delete node;
        return true;
    }

    // verify is a new scope is opened
    if (_act_token->type != tokens::BRACE_OPEN) {
        L_ERROR("special keyword 'location', need "
                "OPEN_BRACE after arg (line {})",
            line);
        config_set_alive_last_token();
        skip_scope(_act_token->value, line);
        _err_count++;
        _valid = false;
        delete node;
        return true;
    }

    // destroy BRACE_OPEN token
    consume_next_token();
    _depth++;
    node->key = "location";
    node->line = line;
    node->type = NODE;
    node->keyword = keywords::LOCATION;
    node->parent = _root;

    _root->children.push_back(node);
    _root = node;
    return true;
}

bool Parser::check_controversal_directive(
    std::vector<config_node *> &node, const keywords::type &key, uint32_t line)
{
    if (key == keywords::PROXY_PASS) {
        if (node_already_present(node, keywords::ROOT)) {
            L_ERROR("directive 'proxy_pass' conflicting with 'root' (line {})",
                line);
            _err_count++;
            return true;
        }
        if (node_already_present(node, keywords::TRY_FILES)) {
            L_ERROR(
                "directive 'proxy_pass' conflicting with 'try_files' (line {})",
                line);
            _err_count++;
            return true;
        }
        if (node_already_present(node, keywords::AUTOINDEX)) {
            L_ERROR(
                "directive 'proxy_pass' conflicting with 'autoindex' (line {})",
                line);
            _err_count++;
            return true;
        }
        if (node_already_present(node, keywords::INDEX)) {
            L_ERROR("directive 'proxy_pass' conflicting with 'index' (line {})",
                line);
            _err_count++;
            return true;
        }
    }
    if ((key == keywords::ROOT || key == keywords::TRY_FILES
            || key == keywords::AUTOINDEX || key == keywords::INDEX)
        && node_already_present(node, keywords::PROXY_PASS)) {
        L_ERROR("directive '{}' conflicting with 'proxy_pass' (line {})",
            keywords_strs[key], line);
        _err_count++;
        return true;
    }
    return false;
}

bool Parser::create_node()
{
    config_node *node;
    uint32_t line;

    line = _act_token->line;

    // check scope validity in scope
    if (!directive_is_in_valide_scope(_root->keyword, _act_token->keyword)) {
        L_ERROR("scope '{}' (line {}) is in a bad scope '{}' (line {}) {}",
            _act_token->value, line, _root->key, _root->line);
        skip_scope(_act_token->value, line, true);
        return true;
    }

    // Initialize node
    try {
        node = new config_node();
        node->key = _act_token->value;
        node->type = NODE;
        node->parent = _root;
        node->keyword = _act_token->keyword;
        node->line = line;
    } catch (...) {
        return false;
    }

    // http and server token must be followed directly by BRACE_OPEN
    if (!see_next_token() || _act_token->type != tokens::BRACE_OPEN) {
        L_ERROR("special keyword '{}', need "
                "OPEN_BRACE (line {})",
            node->key, line);
        skip_line();
        _valid = false;
        _err_count++;
        delete node;
        return true;
    }

    // destroy BRACE_OPEN token
    consume_next_token();
    _depth++;
    _root->children.push_back(node);
    _root = node;
    return true;
}

bool Parser::create_leaf()
{
    config_node *node;
    config_token tmp_token;
    uint32_t line = _act_token->line;

    // Directive need word token identified from keywords to start
    if (_act_token->type != tokens::WORD
        || _act_token->keyword == keywords::UNKNOWN) {
        tmp_token = *_act_token;
        if (!see_next_token() || _act_token->keyword != keywords::OPEN_SCOPE) {
            L_ERROR("unknown directive '{}' (line {})", tmp_token.value, line);
            skip_line();
            _err_count++;
            _valid = false;
            return true;
        }
        skip_scope(tmp_token.value, tmp_token.line, true);
        _valid = false;
        return true;
    }

    // Check for duplicate key
    if (node_already_present(_root->children, _act_token->keyword, true)) {
        L_WARN("directive '{}' (line {}) already present in node {} (line {}), "
               "rewrite directive",
            _act_token->value, line, _root->key, _root->line);
    }

    if (check_controversal_directive(
            _root->children, _act_token->keyword, line)) {
        skip_line();
        _valid = false;
        _err_count++;
        return true;
    }

    // check  directive validity in scope
    if (!directive_is_in_valide_scope(_root->keyword, _act_token->keyword)) {
        L_ERROR("directive '{}' (line {}) is in a bad scope '{}' (line {}) {}",
            _act_token->value, line, _root->key, _root->line,
            strings[_act_token->keyword]);
        skip_line();
        _valid = false;
        _err_count++;
        return true;
    }

    // Initialize node
    try {
        node = new config_node();
        node->key = _act_token->value;
        node->keyword = _act_token->keyword;
        node->type = LEAF;
    } catch (...) {
        return false;
    }

    tmp_token = *_act_token;
    if (!see_next_token()) {
        delete node;
        return true;
    }

    // add value(s) to leaf
    uint32_t ac = 0;
    uint32_t limit = get_max_args(tmp_token);

    while (_act_token->type == tokens::WORD && ac < limit) {
        consume_next_token();
        if (!is_a_valid_val(_act_token->value, tmp_token)) {
            L_ERROR("'{}' is invalid value for '{}'", _act_token->value,
                tmp_token.value);
            _valid = false;
            skip_line();
            _err_count++;
            return true;
        }
        node->vals.push_back(_act_token->value);
        ac++;
        if (!see_next_token()) {
            delete node;
            return true;
        }
    }

    if (ac >= limit && _act_token->type != tokens::END) {
        L_ERROR("directive '{}' can have up to {} argument(s) (line {}) {}",
            node->key, limit, line, strings[tmp_token.keyword]);
        skip_line();
        _err_count++;
        _valid = false;
        node->children.clear();
        delete node;
        return true;
    }

    // check directive end
    if (_act_token->type != tokens::END) {
        if (_act_token->type == tokens::BRACE_OPEN
            && _act_token->line == line) {
            skip_scope(tmp_token.value, line);
            delete node;
            return true;
        }
        L_ERROR("no instruction end ';' (line {})", line);
        skip_line();
        _err_count++;
        _valid = false;
        node->children.clear();
        delete node;
        return true;
    }

    // destroy END token
    consume_next_token();

    // check directive value(s) is not empty
    if (node->vals.empty()) {
        L_WARN("directive '{}' (line {}) has no value, directive ignored",
            node->key, _act_token->line);
        delete node;
        return true;
    }
    node->parent = _root;
    _root->children.push_back(node);
    return true;
}

bool Parser::iter_on_tokens()
{
    uint32_t line;

    if (!consume_next_token())
        return false;

    line = _act_token->line;
    // redirects the creation of a node to the corresponding type
    switch (_act_token->keyword) {
    case keywords::LOCATION:
        return create_location_node();

    case keywords::HTTP:
    case keywords::SERVER:
        return create_node();

    case keywords::OPEN_SCOPE:
        config_set_alive_last_token();
        skip_scope(_act_token->value, line, true);
        return true;

    case keywords::CLOSE_SCOPE:
        if (!_root->parent) {
            L_ERROR(
                "unexpected '}' all scopes are already closed (line {})", line);
            _err_count++;
            _valid = false;
            skip_line();
            return true;
        }
        _depth--;
        _root = _root->parent;
        return true;

    default:
        if (!create_leaf()) {
            return false;
        }
    }
    return true;
}

bool Parser::token_to_tree()
{
    L_DEBUG("TOKEN TO TREE tokens size {}", _tokens.size());
    if (_tokens.empty()) {
        return false;
    }

    while (iter_on_tokens()) { }

    if (!_valid) {
        // free_tree(tree);
        return _valid;
    }

    if (_depth != 0) {
        _err_count++;
        L_ERROR("unexpected EOF missing '}' for close scope '{}' (opened "
                "line {})",
            _root->key, _root->line);
        // free_tree(tree);
        return false;
    }

    return true;
}

void Parser::delete_tree(config_node *root)
{
    if (!root)
        return;
    for (std::vector<config_node *>::iterator it = root->children.begin();
        it != root->children.end(); it++)
        delete_tree(*it);
    root->children.clear();
    delete root;
}

void Parser::free_tree(config_node *root)
{
    if (!root)
        return;
    while (root->parent)
        root = root->parent;
    delete_tree(root);
}

void Parser::debug_tree(config_node *tree, int i)
{
    if (!tree)
        return;
    for (int j = 0; j < i; j++)
        std::cout << "\t";

    if (tree->type == NODE || tree->type == ROOT) {
        i++;
    }
    if (tree->key == "location")
        std::cout << tree->key << " " << location::strings[tree->location_type];
    else
        std::cout << tree->key << " ";
    for (std::vector<std::string>::iterator it = tree->vals.begin();
        it != tree->vals.end(); it++)
        std::cout << *it << " ";
    std::cout << "\n";

    for (std::vector<config_node *>::iterator it = tree->children.begin();
        it != tree->children.end(); it++) {
        config_node *tmp = *it;
        debug_tree(tmp, i);
    }
    if (tree->type == NODE || tree->type == ROOT) {
        i--;
    }
}

bool Parser::tokenize()
{
    std::ifstream in_file;
    std::string buf;
    std::size_t len = 0;
    std::size_t b_size;
    std::size_t line_i = 1;
    config_token tmp_token;

    in_file.open(_path.c_str());
    if (!in_file.is_open()) {
        L_ERROR("Can't open config file {}", _path);
        return false;
    }
    while (std::getline(in_file, buf)) {
        b_size = buf.size();
        for (std::size_t i = 0; b_size > i;) {
            len = 0;
            while (b_size > i && std::isspace(buf[i]))
                i++;
            while (b_size > i + len && !std::isspace(buf[i + len])
                && !config_is_special_char(buf[i + len]))
                len++;
            if (buf[i] == '#') {
                i = b_size;
            } else if (len > 0) {
                tmp_token.value = buf.substr(i, len);
                tmp_token.alive = 1;
                tmp_token.line = line_i;
                tmp_token.type = config_get_token_type(tmp_token);
                tmp_token.keyword = config_get_token_keyword(tmp_token);
                _tokens.push_back(tmp_token);
                i += len;
            } else if (config_is_special_char(buf[i])) {
                tmp_token.value = buf[i];
                tmp_token.alive = 1;
                tmp_token.line = line_i;
                tmp_token.type = config_get_token_type(tmp_token);
                tmp_token.keyword = config_get_token_keyword(tmp_token);
                _tokens.push_back(tmp_token);
                i++;
            }
        }
        line_i++;
    }
    in_file.close();
    return true;
}

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

std::vector<Server> Parser::get_all_servers() { return _servers; }

void Parser::create_one_server(const config_node &node,
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

    Server n_server
        = Server(server_conf.find(keywords::ROOT) != server_conf.end()
                ? *server_conf.find(keywords::ROOT)->second.begin()
                : DEFAULT_ROOT,
            location_vector,
            server_conf.find(keywords::SERVER_NAME) != server_conf.end()
                ? *server_conf.find(keywords::SERVER_NAME)->second.begin()
                : DEFAULT_SERVER_NAME,
            server_conf.find(keywords::LISTEN) != server_conf.end()
                ? *server_conf.find(keywords::LISTEN)->second.begin()
                : DEFAULT_LISTEN);

    _servers.push_back(n_server);
}

bool Parser::create_all_servers()
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
