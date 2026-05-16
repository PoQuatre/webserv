/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:44:31 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/16 19:34:05 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <cctype>
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

#include "Server.hpp"
#include "logger.hpp"
#include "webserv.hpp"

namespace {

enum token_type {
    BRACE_OPEN = 1,
    BRACE_CLOSE = 2,
    WORD = 4,
    END = 8,
    COM = 16,
    EQUAL = 32
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

std::string dirpart(const std::string &path)
{
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return "."; // no separator → current dir
    if (pos == 0)
        return "/"; // root
    return path.substr(0, pos);
}

std::string tkt(const config_token &tok)
{
    if (tok.type == BRACE_OPEN)
        return "BRACE OPEN";
    if (tok.type == BRACE_CLOSE)
        return "BRACE CLOSE";
    if (tok.type == END)
        return "SEMICOLON";
    if (tok.type == COM)
        return "";
    if (tok.type == EQUAL)
        return "EQUAL";
    return "WORD";
}

bool key_check_syn(config_token token)
{
    std::size_t i;
    std::string config_ok_special_char = "_-/\\$.";

    for (i = 0; i < token.value.size(); i++) {
        if (!std::isalnum(token.value[i])
            && config_ok_special_char.find(token.value[i]) == std::string::npos)
            return false;
    }
    return true;
}

token_type config_get_token_type(config_token token)
{
    std::size_t len = token.value.size();
    if (len == 1) {
        char val = *token.value.begin();
        if (val == ';')
            return END;
        if (val == '{')
            return BRACE_OPEN;
        if (val == '}')
            return BRACE_CLOSE;
        if (val == '#')
            return COM;
        if (val == '=')
            return EQUAL;
    }
    return WORD;
}

bool config_is_special_char(char c)
{
    return (c == '#' || c == '{' || c == '}' || c == ';' || c == '=');
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
    for (std::vector<config_node *>::iterator ite = root->children.begin();
        ite != root->children.end(); ite++)
        config_free_tree(*ite);
    root->children.clear();
    delete root;
}

void config_skip_line(std::vector<config_token> &tokens, const uint32_t &line)
{
    for (std::vector<config_token>::iterator ite = tokens.begin();
        ite != tokens.end(); ite++) {
        if (line == ite->line) {
            ite->alive = 0;
        }
        if (line < ite->line)
            break;
    }
}

bool config_see_next_token(
    std::vector<config_token> &tokens, config_token **new_token, bool consume)
{
    for (std::vector<config_token>::iterator ite = tokens.begin();
        ite != tokens.end(); ite++) {
        if (ite->alive) {
            if (consume) {
                ite->alive = 0;
            }
            **new_token = *ite;
            return true;
        }
    }
    (void)new_token;
    *new_token = 0;
    return false;
}

bool create_location_node(config_node **root, std::vector<config_token> &tokens,
    config_node **og_root, bool &valid, config_token **token,
    uint32_t &error_count)
{
    config_node *node;
    uint32_t line = (*token)->line;

    try {
        node = new config_node();
    } catch (...) {
        config_free_tree(*og_root);
        return false;
    }
    node->key = "location";
    node->strict = false;
    node->line = line;
    if (!config_see_next_token(tokens, token, true))
        return true;
    if ((*token)->type == EQUAL) {
        node->strict = true;
        if (!config_see_next_token(tokens, token, true)) {
            delete node;
            return true;
        }
    }
    if ((*token)->type != WORD && (*token)->type != BRACE_OPEN) {
        L_ERROR("location need one parameter (line {})", line);
        error_count++;
        valid = false;
        config_skip_line(tokens, (*token)->line);
        delete node;
        return true;
    }
    if ((*token)->type == WORD) {
        node->vals.push_back((*token)->value);
        if (!config_see_next_token(tokens, token, true))
            return true;
    }
    if ((*token)->type != BRACE_OPEN) {
        L_ERROR("special keyword 'location', need "
                "OPEN_BRACE after arg (line {})",
            line);
        config_skip_line(tokens, (*token)->line);
        valid = false;
        error_count++;
        delete node;
        return true;
    }
    if (!config_see_next_token(tokens, token, false)) {
        node->type = NODE;
        node->parent = *root;
        (*root)->children.push_back(node);
        *root = node;
        return true;
    }
    if ((*token)->type != WORD && (*token)->line == line) {
        L_ERROR("unexpected '{}' (line {})", (*token)->value, line);
        error_count++;
        config_skip_line(tokens, (*token)->line);
        delete node;
        return true;
    }
    node->type = NODE;
    node->parent = *root;
    (*root)->children.push_back(node);
    *root = node;
    return true;
}

bool create_node(config_node **root, std::vector<config_token> &tokens,
    config_node **og_root, bool &valid, config_token **token,
    uint32_t &error_count)
{
    config_node *node;
    uint32_t line;

    try {
        node = new config_node();
    } catch (...) {
        config_free_tree(*og_root);
        return false;
    }
    line = (*token)->line;
    node->key = (*token)->value;
    node->type = NODE;
    node->parent = *root;
    node->line = line;
    if (!config_see_next_token(tokens, token, false)) {
        delete node;
        return true;
    }
    if ((*token)->type != BRACE_OPEN) {
        L_ERROR("special keyword '{}', need "
                "OPEN_BRACE (line {})",
            node->key, line);
        config_skip_line(tokens, (*token)->line);
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
        (*root)->children.push_back(node);
        *root = node;
        return true;
    }
    if ((*token)->type != WORD) {
        L_ERROR("unexpected '{}' (line {})", (*token)->value, line);
        error_count++;
        config_skip_line(tokens, (*token)->line);
        delete node;
        return true;
    }
    (*root)->children.push_back(node);
    *root = node;
    return true;
}

bool jsp(std::vector<config_token> &tokens, config_token **token,
    config_node *node, bool recovery, uint32_t line, uint32_t &error_count)
{
    uint32_t tmp_line = line;
    int32_t prof = 1;
    while (recovery && (*token)) {
        if (!config_see_next_token(tokens, token, true)) {
            delete node;
            return true;
        }
        line = (*token)->line;
        if ((*token)->type == BRACE_OPEN) {
            prof++;
        } else if ((*token)->type == BRACE_CLOSE) {
            prof--;
            if (prof == 0) {
                break;
                L_ERROR("undefine scope name used, ignoring '{}' scope "
                        "from "
                        "{} to {}",
                    node->key, tmp_line, line);
                config_skip_line(tokens, line);
                delete node;
                error_count++;
                return true;
            }
        }
    }
    if (prof == 0) {
        L_WARN("undefine scope name used, ignoring '{}' scope from {} to {}",
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

bool config_create_leaf(config_node **root, std::vector<config_token> &tokens,
    bool &valid, config_token **token, bool recovery, uint32_t &error_count)
{
    config_node *node;
    config_token tmp_token;
    uint32_t line = (*token)->line;

    try {
        node = new config_node();
    } catch (...) {
        return false;
    }
    node->parent = *root;
    node->type = LEAF;
    (void)key_check_syn;
    node->key = (*token)->value;
    if (!config_see_next_token(tokens, token, false)) {
        delete node;
        return true;
    }
    if ((*token)->line != line) {
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
    while (((*token)->type == EQUAL || (*token)->type == WORD)
        && (*token)->line == line) {
        node->vals.push_back((*token)->value);
        if (!config_see_next_token(tokens, token, false)) {
            delete node;
            return true;
        }
        if ((*token)->line != line) {
            break;
        }
        if (!config_see_next_token(tokens, token, true)) {
            delete node;
            return true;
        }
    }
    if ((*token)->type != END) {
        if ((*token)->type == BRACE_OPEN && (*token)->line == line) {
            return jsp(tokens, token, node, recovery, line, error_count);
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
    if ((*token)->type != WORD && (*token)->type != BRACE_CLOSE) {
        L_ERROR("extra {} (line {})", tkt(**token), (*token)->line);
        error_count++;
        config_skip_line(tokens, line);
        valid = false;
        delete node;
        return true;
    }

    (*root)->children.push_back(node);
    return true;
}

bool iter_on_tokens(config_node **root, std::vector<config_token> &tokens,
    bool &valid, config_token **token, bool recovery, int &deep,
    uint32_t &error_count)
{
    config_node *og_root = *root;

    if (!config_see_next_token(tokens, token, true))
        return true;
    uint32_t line = (*token)->line;
    if ((*token)->value == "location") {
        if (!create_location_node(
                root, tokens, &og_root, valid, token, error_count))
            return false;
        deep++;
    } else if ((*token)->value == "server" || (*token)->value == "http") {
        if (!create_node(root, tokens, &og_root, valid, token, error_count))
            return false;
        deep++;
    } else if ((*token)->type == BRACE_CLOSE) {
        std::string old_token_value = (*token)->value;
        if (!config_see_next_token(tokens, token, false))
            return true;
        if (!(*root)->parent) {
            L_ERROR("unexpected '{}' (line {})", old_token_value, line);
            error_count++;
            config_skip_line(tokens, line);
            return true;
        }
        deep--;
        *root = (*root)->parent;
    } else if ((*token)->type == BRACE_OPEN) {
        L_ERROR("open an undefine scope (line {})", line);
        error_count++;
        config_skip_line(tokens, line);
        valid = false;
        return true;
    } else {
        if (!config_create_leaf(
                root, tokens, valid, token, recovery, error_count)) {
            config_free_tree(og_root);
            return false;
        }
    }
    return true;
}

bool token_to_tree(std::vector<config_token> &tokens, config_node **tree,
    bool recovery, int &deep, uint32_t &error_count)
{
    config_node *root = new config_node();
    config_node *og_root = root;
    config_token tok;
    config_token *token = &tok;
    bool valid = true;
    root->type = ROOT;
    root->key = "TOP LEVEL";
    root->parent = 0;

    if (!config_see_next_token(tokens, &token, false))
        return false;
    while (token) {
        if (!iter_on_tokens(
                &root, tokens, valid, &token, recovery, deep, error_count))
            return false;
    }
    if (!valid) {
        config_free_tree(og_root);
        return valid;
    }
    if (deep != 1) {
        error_count++;
        if (root->vals.empty())
            L_ERROR("all statement need to be close (act scope "
                    "{}, start {})",
                root->key, root->line);
        else
            L_ERROR("all statement need to be close (act scope "
                    "'{} {}', start line {})",
                root->key, *root->vals.begin(), root->line);
        config_free_tree(og_root);
        return false;
    }
    while (root->parent)
        root = root->parent;
    *tree = root;
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
    for (std::vector<std::string>::iterator ite = tree->vals.begin();
        ite != tree->vals.end(); ite++)
        std::cout << *ite << " ";
    std::cout << "\n";

    for (std::vector<config_node *>::iterator ite = tree->children.begin();
        ite != tree->children.end(); ite++) {
        config_node *tmp = *ite;
        debug_tree(tmp, i);
    }
    if (tree->type == NODE || tree->type == ROOT) {
        i--;
    }
}
#endif

bool get_first_val(config_node *node, const std::string &key, std::string &val)
{
    for (std::vector<config_node *>::iterator ite = node->children.begin();
        ite != node->children.end(); ite++) {
        if (key == (*ite)->key) {
            val = *((*ite)->vals.begin());
            return true;
        }
    }
    return false;
}

bool get_direct_child(
    config_node *node, const std::string &key, config_node *&node_to_set)
{
    for (std::vector<config_node *>::iterator ite = node->children.begin();
        ite != node->children.end(); ite++) {
        if (key == (*ite)->key) {
            node_to_set = *ite;
            return true;
        }
    }
    return false;
}

/*
bool get_all_location(config_node *node, Location res)
{
    std::cout << "c reparti node parent " << node->key << "\n";
    for (std::vector<config_node *>::iterator ite = node->children.begin();
        ite != node->children.end(); ite++) {
        std::cout << "value " << (*ite)->key << "\n";
        if ((*ite)->key == "location") {
            Location tmp_loc = { };
            tmp_loc.exact = (*ite)->strict;
            if ((*ite)->vals.size() > 0) {
                // Location children
            }
            res.push_back(tmp_loc);
        }
    }
    return (!res.empty());
}

bool create_one_server(std::vector<Server> &servers, config_node *srv_node,
    const std::string &servername, const std::string &addr)
{
Location tmp_loc = { };
if (!get_all_location(srv_node, tmp))
return false;
(void)servers;
(void)srv_node;
(void)servername;
(void)addr;
return true;
}
*/

bool create_servers(config_node *root, std::vector<Server> &servers, int deep)
{
    Location location = { };
    bool valid = false;
    std::vector<config_node *> node_stack = root->children;

    for (std::size_t i = 0; i < node_stack.size(); i++) {
        if (node_stack[i]->key == "server") {
            std::string root_str;
            std::string servername;
            std::string addr;
            config_node *location_node = 0;
            if (!get_first_val(node_stack[i], "server_name", servername)
                || !get_first_val(node_stack[i], "listen", addr)
                || !get_direct_child(
                    node_stack[i], "location", location_node)) {
            } else {
                // create_one_server(servers, node_stack[i], servername, addr);
                if (get_first_val(location_node, "root", root_str)) {
                    L_DEBUG("config-parser: server create");
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
    (void)deep;
    return valid;
}
}

bool parse_config(
    std::vector<Server> &servers, const std::string &path, bool recovery)
{

    Location location = { };
    std::vector<config_token> tokens;
    config_node *config_root = NULL;
    uint32_t error_count = 0;

    if (!tokenize_config_file(path, tokens))
        return false;

    for (std::vector<config_token>::iterator ite = tokens.begin();
        ite != tokens.end(); ite++) { }
    int deep = 0;
    if (!token_to_tree(tokens, &config_root, recovery, deep, error_count)) {
        L_INFO("Invalid configuration file {} error", error_count);
        return false;
    }

    const std::string root = dirpart(path);

#ifndef NDEBUG
    debug_tree(config_root, 0);
#endif // !
    if (!create_servers(config_root, servers, 3)) {
        L_ERROR("Invalid configuration file no server scope");
        config_free_tree(config_root);
        return false;
    }

    if (servers.empty()) {
        servers.push_back(
            Server(root, location, "example.com", "0.0.0.0:8080"));
    }
    config_free_tree(config_root);
    return true;
}
