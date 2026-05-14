/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:44:31 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/15 14:02:21 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Server.hpp"
#include "logger.hpp"
#include "webserv.hpp"

namespace {

enum token_type { BRACE_OPEN, BRACE_CLOSE, WORD };
enum node_type { ROOT, NODE, LEAF };

struct config_token {
    std::string value;
    uint32_t size;
    token_type type;
};

struct config_node {
    node_type type;
    std::string key;
    std::string location;
    std::vector<std::string> vals;
    std::vector<config_node *> children;
    config_node *parent;
    bool strict;
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

bool token_check_syn(config_token token)
{
    bool brace_flag = false;
    std::size_t i;

    for (i = 0; i < token.value.size(); i++) {
        if (token.value[i] == '}' || token.value[i] == '{')
            brace_flag = true;
    }
    if (i > 1)
        return !brace_flag;
    return true;
}

bool key_check_syn(config_token token)
{
    std::size_t i;

    for (i = 0; i < token.value.size(); i++) {
        if (!std::isalnum(token.value[i]) && token.value[i] != '_')
            return false;
    }
    return true;
}

bool config_check_line(std::string str)
{
    if (str.size() <= 0)
        return true;
    char c = str[str.size() - 1];
    if (c == '}' || c == '{' || c == ';')
        return true;
    for (int i = 0; str[i]; i++)
        if (!std::isspace(str[i]))
            return false;
    return true;
}

bool tokenize_config_file(
    const std::string &path, std::vector<config_token> &tokens)
{
    std::ifstream in_file;
    std::string buf;
    std::size_t len = 0;
    std::size_t b_size;
    std::size_t line_i = 0;
    config_token tmp_token;

    in_file.exceptions(std::ifstream::failbit);
    try {
        in_file.open(path.c_str());
    } catch (...) {
        logger::error("Can't open config file", "");
        return false;
    }
    in_file.exceptions(std::ifstream::goodbit);
    while (std::getline(in_file, buf)) {
        if (!config_check_line(buf)) {
            logger::error("'{}' is a bad syntaxe", buf);
            tokens.clear();
            in_file.close();
            return false;
        }
        line_i++;
        b_size = buf.size();
        for (std::size_t i = 0; b_size > i; i++) {
            len = 0;
            while (b_size > i && std::isspace(buf[i]))
                i++;
            while (b_size > i + len && !std::isspace(buf[i + len])
                && buf[i + len] != '#')
                len++;
            if (len > 0) {
                tmp_token.value = buf.substr(i, len);
                if (!token_check_syn(tmp_token)) {
                    logger::error("Invalid syntax (line {}) : '{}'", line_i,
                        tmp_token.value);
                    in_file.close();
                    tokens.clear();
                    return false;
                }
                tokens.push_back(tmp_token);
                i += len;
            }
        }
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

bool token_to_tree(std::vector<config_token> tokens, config_node **tree)
{
    config_node *root = new config_node();
    config_node *og_root = root;
    config_node *node;
    root->type = ROOT;
    root->key = "TOP LEVEL";
    root->parent = 0;

    for (std::vector<config_token>::iterator token = tokens.begin();
        token != tokens.end();) {
        if (token->value == "server" || token->value == "location"
            || token->value == "http") {
            if (token + 1 == tokens.end()) {
                config_free_tree(og_root);
                return false;
            }
            if (token->value == "location") {
                token++;
                try {
                    node = new config_node();
                } catch (...) {
                    config_free_tree(og_root);
                    return false;
                }
                node->key = "location";
                node->strict = false;
                if (token == tokens.end()) {
                    config_free_tree(og_root);
                    return false;
                }
                while (token != tokens.end() && !token->value.empty()
                    && token->value[token->value.size() - 1] != '{') {
                    if (token->value[0] == '=')
                        node->strict = true;
                    node->vals.push_back(token->value);
                    token++;
                }
                if (token == tokens.end() || token->value[0] != '{'
                    || token->value.size() > 1) {
                    config_free_tree(og_root);
                    if (token != tokens.end())
                        logger::error("'{}' Invalid syntax", token->value);
                    return false;
                }

                node->type = NODE;
                node->parent = root;
                root->children.push_back(node);
                root = node;
            } else if (token->value == "server" || token->value == "http") {
                try {
                    node = new config_node();
                } catch (...) {
                    config_free_tree(og_root);
                    return false;
                }
                node->key = token->value;
                token++;
                node->type = NODE;
                node->parent = root;
                root->children.push_back(node);
                root = node;
            }
            if (token == tokens.end() || token->value[0] != '{') {

                logger::error(" no end", token->value);
                config_free_tree(og_root);
                return false;
            }
            token++;
        } else if (token->value[0] == '}') {
            if (!root->parent) {
                logger::error("Extra }", "");
                config_free_tree(og_root);
                return false;
            }
            root = root->parent;
            token++;
        } else {
            node = new config_node();
            node->parent = root;
            node->type = LEAF;
            if (!key_check_syn(*token)) {
                logger::error(" '{}' Bad syntax", token->value);
                config_free_tree(og_root);
                delete node;
                return false;
            }
            node->key = token->value;
            token++;
            while (token != tokens.end() && !token->value.empty()
                && token->value[token->value.size() - 1] != ';') {
                node->vals.push_back(token->value);
                token++;
            }
            if (token == tokens.end()) {
                logger::error(" No end", "");
                config_free_tree(og_root);
                delete node;
                return false;
            }
            token->value.erase(token->value.length() - 1);
            node->vals.push_back(token->value);
            root->children.push_back(node);
            token++;
        }
    }
    while (root->parent)
        root = root->parent;
    *tree = root;
    return true;
}

void debug_tree(config_node *tree, int i)
{
    if (!tree)
        return;
    if (tree->type == NODE || tree->type == ROOT)
        i++;
    for (int j = 0; j < i; j++)
        std::cout << "	";
    std::cout << tree->key << " ";
    for (std::vector<std::string>::iterator ite = tree->vals.begin();
        ite != tree->vals.end(); ite++)
        std::cout << *ite << " ";
    std::cout << "\n";
    for (std::vector<config_node *>::iterator ite = tree->children.begin();
        ite != tree->children.end(); ite++) {
        config_node *tmp = *ite.base();
        debug_tree(tmp, i);
    }
    if (tree->type == NODE || tree->type == ROOT)
        i--;
}

}

std::vector<Server> parse_config(const std::string &path)
{
    L_DEBUG("Parsing configs");

    std::vector<Server> servers;
    Location location = { };
    std::vector<config_token> tokens;
    config_node *config_root = NULL;

    if (!tokenize_config_file(path, tokens)) { }
    if (!token_to_tree(tokens, &config_root)) {
    } else
        debug_tree(config_root, 0);
    const std::string root = dirpart(path);

    servers.push_back(Server(root, location, "example.com", "0.0.0.0:8080"));

    L_DEBUG("Successfully parsed configs");
    config_free_tree(config_root);
    return servers;
}
