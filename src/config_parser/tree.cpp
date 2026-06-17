/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tree.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/17 19:01:36 by nlaporte          #+#    #+#             */
/*   Updated: 2026/06/17 21:16:24 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <regex.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include "config-parser-def.hpp"
#include "config-parser.hpp"
#include "logger.hpp"

namespace {
const char *strings[] = {
#define X(_, __, ___, text, ...) #text,
#define X_SPECIAL(_, __, ___, text, ...) text,
    KEYWORDS
#undef X_SPECIAL
#undef X
};

const char *keywords_strs[] = {
#define X(_, text, ...) #text,
#define X_SPECIAL(_, text, ...) text,
    KEYWORDS
#undef X_SPECIAL
#undef X
};

// clang-format off
const int keywords_scope_rules[] = {
#define X(_, _____, ___, __, http, server, location, ...) ((http) + ((server) << 1) + ((location) << 2)),
#define X_SPECIAL(_, _____, ___, __, http, server, location, ...) ((http) + ((server) << 1) + ((location) << 2)),
    KEYWORDS
#undef X_SPECIAL
#undef X
};
//clang-format on



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

bool directive_is_in_valid_scope(
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
#define X(name, _, arg, ...)                                                   \
    case keywords::name:                                                       \
        return arg;
#define X_SPECIAL(name, _, arg, ...)                                           \
    case keywords::name:                                                       \
        return arg;
        KEYWORDS // NOLINT (bugprone-branch-clone) switch 2 identical branches
#undef X_SPECIAL
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
#define X_SPECIAL(name, _, _______, __, ___, ____, _____, check, ______)       \
    case keywords::name:                                                       \
        return check(val);
        KEYWORDS // NOLINT (bugprone-branch-clone) switch 2 identical branches
#undef X_SPECIAL
#undef X
            default : return true;
    }
}
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

void Parser::delete_tree(config_node *root)
{
    if (!root)
        return;
    for (std::vector<config_node *>::iterator it = root->children.begin();
        it != root->children.end(); it++)
        delete_tree(*it);
    if (root->location_type == location::CASE_INSENSITIVE
        || root->location_type == location::CASE_SENSITIVE)
        regfree(&root->location_regexp);
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
    if (!directive_is_in_valid_scope(_root->keyword, _act_token->keyword)) {
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

    //  Handle location type
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

    // Create regexp
    int code;
    switch (node->location_type) {
    case location::CASE_INSENSITIVE:
        code = regcomp(&node->location_regexp, _act_token->value.c_str(),
            REG_EXTENDED | REG_ICASE);
        break;
    case location::CASE_SENSITIVE:
        code = regcomp(
            &node->location_regexp, _act_token->value.c_str(), REG_EXTENDED);
        break;
    default:
        code = 0;
        break;
    }
    if (code != 0) {
        L_ERROR("location node invalid regexp (line {}) {}", node->line,
            strerror(errno));
        delete node;
        return false;
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
    if (!directive_is_in_valid_scope(_root->keyword, _act_token->keyword)) {
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
    if (!directive_is_in_valid_scope(_root->keyword, _act_token->keyword)) {
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
