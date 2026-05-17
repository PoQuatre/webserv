/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:44:31 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/17 03:38:10 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <netinet/in.h>
#include <sys/socket.h>

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "Server.hpp"
#include "logger.hpp"
#include "webserv.hpp"

namespace {

#define TOKEN_TYPE                                                             \
    X(BRACE_OPEN, '{')                                                         \
    X(BRACE_CLOSE, '}')                                                        \
    X(WORD, ' ')                                                               \
    X(END, ';')                                                                \
    X(EQUAL, '=')                                                              \
    X(COM, '#')

enum token_type {
#define X(name, _) name,
    TOKEN_TYPE
#undef X
};

enum node_type { ROOT, NODE, LEAF };

struct config_token {
    std::string value;
    token_type type;
    bool alive;
    uint32_t size;
    uint32_t line;
};

struct config_node {
    node_type type;
    std::string key;
    std::vector<std::string> vals;
    std::vector<config_node *> children;
    config_node *parent;
    bool strict;
    uint32_t line;
};

std::string get_token_type_string(const config_token &tok)
{
    switch (tok.type) {
#define X(name, _)                                                             \
    case name:                                                                 \
        return #name;
        TOKEN_TYPE
#undef X
    default:
        return "WORD";
    }
}

token_type config_get_token_type(config_token token)
{
    switch (token.value[0]) {
#define X(name, val)                                                           \
    case val:                                                                  \
        return name;
        TOKEN_TYPE
#undef X
    default:
        return WORD;
    }
}

bool config_is_special_char(char c)
{
    switch (c) {
#define X(_, val) case val:
        TOKEN_TYPE
#undef X
        return true;
    default:
        return false;
    }
}

bool tokenize_config_file(
    const std::string &path, std::vector<config_token> &tokens)
{
    std::ifstream in_file;
    std::string buf;
    std::size_t len = 0;
    std::size_t b_size;
    std::size_t line_i = 1;
    config_token tmp_token;

    in_file.open(path.c_str());
    if (!in_file.is_open()) {
        L_ERROR("Can't open config file {}", path);
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
                tokens.push_back(tmp_token);
                i += len;
            } else if (config_is_special_char(buf[i])) {
                tmp_token.value = buf[i];
                tmp_token.alive = 1;
                tmp_token.line = line_i;
                tmp_token.type = config_get_token_type(tmp_token);
                tokens.push_back(tmp_token);
                i++;
            }
        }
        line_i++;
    }
    in_file.close();
    return true;
}

void config_free_tree(config_node *root)
{
    if (!root)
        return;
    for (std::vector<config_node *>::iterator it = root->children.begin();
        it != root->children.end(); it++)
        config_free_tree(*it);
    root->children.clear();
    delete root;
}

void config_skip_line(std::vector<config_token> &tokens, const uint32_t &line)
{
    for (std::vector<config_token>::iterator it = tokens.begin();
        it != tokens.end(); it++) {
        if (line == it->line) {
            it->alive = 0;
        }
        if (line < it->line)
            break;
    }
}

bool config_see_next_token(
    std::vector<config_token> &tokens, config_token *&new_token, bool consume)
{
    for (std::vector<config_token>::iterator it = tokens.begin();
        it != tokens.end(); it++) {
        if (it->alive) {
            if (consume) {
                it->alive = 0;
            }
            *new_token = *it;
            return true;
        }
    }
    new_token = 0;
    return false;
}

bool create_location_node(config_node *&root, std::vector<config_token> &tokens,
    config_node *&og_root, bool &valid, config_token *&token,
    uint32_t &error_count)
{
    config_node *node;
    uint32_t line = token->line;

    try {
        node = new config_node();
    } catch (...) {
        config_free_tree(og_root);
        return false;
    }
    node->key = "location";
    node->strict = false;
    node->line = line;
    if (!config_see_next_token(tokens, token, true))
        return true;
    if (token->type == EQUAL) {
        node->strict = true;
        if (!config_see_next_token(tokens, token, true)) {
            delete node;
            return true;
        }
    }
    if (token->type != WORD && token->type != BRACE_OPEN) {
        L_ERROR("location need one parameter (line {})", line);
        error_count++;
        valid = false;
        config_skip_line(tokens, token->line);
        delete node;
        return true;
    }
    if (token->type == WORD) {
        node->vals.push_back(token->value);
        if (!config_see_next_token(tokens, token, true))
            return true;
    }
    if (token->type != BRACE_OPEN) {
        L_ERROR("special keyword 'location', need "
                "OPEN_BRACE after arg (line {})",
            line);
        config_skip_line(tokens, token->line);
        error_count++;
        delete node;
        return true;
    }
    if (!config_see_next_token(tokens, token, false)) {
        node->type = NODE;
        node->parent = root;
        root->children.push_back(node);
        root = node;
        return true;
    }
    if (token->type != WORD && token->line == line) {
        L_ERROR("unexpected '{}' (line {})", token->value, line);
        error_count++;
        config_skip_line(tokens, token->line);
        delete node;
        return true;
    }
    node->type = NODE;
    node->parent = root;
    root->children.push_back(node);
    root = node;
    return true;
}

bool create_node(config_node *&root, std::vector<config_token> &tokens,
    config_node *&og_root, bool &valid, config_token *&token,
    uint32_t &error_count)
{
    config_node *node;
    uint32_t line;

    try {
        node = new config_node();
    } catch (...) {
        config_free_tree(og_root);
        return false;
    }
    line = token->line;
    node->key = token->value;
    node->type = NODE;
    node->parent = root;
    node->line = line;
    if (!config_see_next_token(tokens, token, false)) {
        delete node;
        return true;
    }
    if (token->type != BRACE_OPEN) {
        L_ERROR("special keyword '{}', need "
                "OPEN_BRACE (line {})",
            node->key, line);
        config_skip_line(tokens, token->line);
        valid = false;
        delete node;
        error_count++;
        return true;
    }
    if (!config_see_next_token(tokens, token, true)) {
        delete node;
        return true;
    }
    if (!config_see_next_token(tokens, token, false)) {
        root->children.push_back(node);
        root = node;
        return true;
    }
    if (token->type != WORD) {
        L_ERROR("unexpected '{}' (line {})", token->value, line);
        error_count++;
        config_skip_line(tokens, token->line);
        delete node;
        return true;
    }
    root->children.push_back(node);
    root = node;
    return true;
}

bool recovery(std::vector<config_token> &tokens, config_token *&token,
    config_node *node, uint32_t line, uint32_t &error_count)
{
    uint32_t tmp_line = line;
    int32_t depth = 1;
    while (token) {
        if (!config_see_next_token(tokens, token, true)) {
            delete node;
            return true;
        }
        line = token->line;
        if (token->type == BRACE_OPEN) {
            depth++;
        } else if (token->type == BRACE_CLOSE) {
            depth--;
            if (depth == 0) {
                break;
                L_ERROR(
                    "undefined scope name used, ignoring '{}' scope from {} to "
                    "{}",
                    node->key, tmp_line, line);
                config_skip_line(tokens, line);
                delete node;
                error_count++;
                return true;
            }
        }
    }
    if (depth == 0) {
        L_WARN("undefined scope name used, ignoring '{}' scope from {} to {}",
            node->key, tmp_line, line);
        config_skip_line(tokens, line);
    } else {
        L_ERROR("ignoring '{}', because is not a valid scope (line {})",
            node->key, tmp_line);
        error_count++;
    }
    delete node;
    return true;
}

bool config_create_leaf(config_node *&root, std::vector<config_token> &tokens,
    bool &valid, config_token *&token, uint32_t &error_count)
{
    config_node *node;
    config_token tmp_token;
    uint32_t line = token->line;

    try {
        node = new config_node();
    } catch (...) {
        valid = false;
        return false;
    }
    node->key = token->value;
    if (!config_see_next_token(tokens, token, false)) {
        delete node;
        return true;
    }
    if (token->line != line) {
        L_ERROR("no instruction need arg and end ';' (line {})", line);
        error_count++;
        valid = false;
        delete node;
        return true;
    }
    if (!config_see_next_token(tokens, token, true)) {
        delete node;
        return true;
    }
    while (
        (token->type == EQUAL || token->type == WORD) && token->line == line) {
        node->vals.push_back(token->value);
        if (!config_see_next_token(tokens, token, false)) {
            delete node;
            return true;
        }
        if (token->line != line) {
            break;
        }
        if (!config_see_next_token(tokens, token, true)) {
            delete node;
            return true;
        }
    }
    if (token->type != END) {
        if (token->type == BRACE_OPEN && token->line == line) {
            return recovery(tokens, token, node, line, error_count);
        }
        L_ERROR("no instruction end ';' (line {})", line);
        error_count++;
        config_skip_line(tokens, line);
        valid = false;
        delete node;
        return true;
    }

    if (!config_see_next_token(tokens, token, false))
        return true;
    if (token->type != WORD && token->type != BRACE_CLOSE) {
        L_ERROR(
            "extra {} (line {})", get_token_type_string(*token), token->line);
        error_count++;
        config_skip_line(tokens, line);
        valid = false;
        delete node;
        return true;
    }

    root->children.push_back(node);
    return true;
}

bool iter_on_tokens(config_node *&root, std::vector<config_token> &tokens,
    bool &valid, config_token *&token, int &depth, uint32_t &error_count)
{
    config_node *og_root = root;

    if (!config_see_next_token(tokens, token, true))
        return true;
    uint32_t line = token->line;
    if (token->value == "location") {
        if (!create_location_node(
                root, tokens, og_root, valid, token, error_count))
            return false;
        depth++;
    } else if (token->value == "server" || token->value == "http") {
        if (!create_node(root, tokens, og_root, valid, token, error_count))
            return false;
        depth++;
    } else if (token->type == BRACE_CLOSE) {
        std::string old_token_value = token->value;
        if (!config_see_next_token(tokens, token, false))
            return true;
        if (!root->parent) {
            L_ERROR("unexpected '{}' (line {})", old_token_value, line);
            error_count++;
            config_skip_line(tokens, line);
            return true;
        }
        depth--;
        root = root->parent;
    } else if (token->type == BRACE_OPEN) {
        L_ERROR("opened an undefined scope (line {})", line);
        error_count++;
        config_skip_line(tokens, line);
        valid = false;
        return true;
    } else {
        if (!config_create_leaf(root, tokens, valid, token, error_count)) {
            config_free_tree(og_root);
            return false;
        }
    }
    return true;
}

bool token_to_tree(std::vector<config_token> &tokens, config_node *&tree,
    int &depth, uint32_t &error_count)
{
    config_node *root;
    try {
        root = new config_node();
    } catch (...) {
        return false;
    }
    config_node *og_root = root;
    config_token tok;
    config_token *token = &tok;
    bool valid = true;
    root->type = ROOT;
    root->key = "TOP LEVEL";
    root->parent = 0;

    if (!config_see_next_token(tokens, token, false))
        return false;
    while (token) {
        if (!iter_on_tokens(root, tokens, valid, token, depth, error_count))
            return false;
    }
    if (!valid) {
        config_free_tree(og_root);
        return valid;
    }
    if (depth != 1) {
        error_count++;
        if (root->vals.empty())
            L_ERROR("all statements need to be closed (in scope {}, line {})",
                root->key, root->line);
        else
            L_ERROR("all statements need to be closed (in scope "
                    "'{} {}', line {})",
                root->key, root->vals[0], root->line);
        config_free_tree(og_root);
        return false;
    }
    while (root->parent)
        root = root->parent;
    tree = root;
    return true;
}

#ifndef NDEBUG
void debug_tree(config_node *tree, int i)
{
    if (!tree)
        return;
    for (int j = 0; j < i; j++)
        std::cout << "	";
    if (tree->type == NODE || tree->type == ROOT) {
        i++;
    }
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
#endif

bool get_first_val(config_node *node, const std::string &key, std::string &val)
{
    for (std::vector<config_node *>::iterator it = node->children.begin();
        it != node->children.end(); it++) {
        if (key == (*it)->key) {
            val = *((*it)->vals.begin());
            return true;
        }
    }
    return false;
}

bool get_direct_child(
    config_node *node, const std::string &key, config_node *&node_to_set)
{
    for (std::vector<config_node *>::iterator it = node->children.begin();
        it != node->children.end(); it++) {
        if (key == (*it)->key) {
            node_to_set = *it;
            return true;
        }
    }
    return false;
}

bool create_servers(config_node *root, std::vector<Server> &servers)
{
    bool valid = false;
    std::vector<config_node *> node_stack = root->children;

    for (std::size_t i = 0; i < node_stack.size(); i++) {
        if (node_stack[i]->key == "server") {
            std::string root_str;
            std::string servername;
            std::string addr;
            config_node *location_node = 0;
            if (!get_first_val(node_stack[i], "server_name", servername)) {
                L_ERROR("no server name");
            }
            if (!get_first_val(node_stack[i], "listen", addr)) {
                L_ERROR("no listen");
            } else {
                Location location = { };
                if (get_direct_child(
                        node_stack[i], "location", location_node)) {
                    if (get_first_val(location_node, "root", root_str)) {
                        L_TRACE("config-parser: server create");
                        servers.push_back(
                            Server(root_str, location, servername, addr));
                        valid = true;
                    }
                } else {
                    Location location = { };
                    servers.push_back(
                        Server(root_str, location, servername, addr));
                    valid = true;
                }
            }
        } else if (node_stack[i]->type == NODE) {
            node_stack.insert(node_stack.end(), node_stack[i]->children.begin(),
                node_stack[i]->children.end());
        }
    }
    L_DEBUG("{} server from config file", servers.size());
    return valid;
}
}

bool parse_config(std::vector<Server> &servers, const std::string &path)
{

    Location location = { };
    std::vector<config_token> tokens;
    config_node *config_root = NULL;
    uint32_t error_count = 0;

    if (!tokenize_config_file(path, tokens))
        return false;

    int depth = 0;
    if (!token_to_tree(tokens, config_root, depth, error_count)) {
        L_ERROR("invalid configuration file {} error", error_count);
        return false;
    }

#ifndef NDEBUG
    debug_tree(config_root, 0);
#endif
    if (!create_servers(config_root, servers)) {
        L_ERROR("invalid configuration file no server scope");
        config_free_tree(config_root);
        return false;
    }

    if (servers.empty()) {
        L_ERROR("no server in configuration");
        return false;
    }

    config_free_tree(config_root);
    return true;
}
