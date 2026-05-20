/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 02:44:31 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/25 21:06:47 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config.hpp"

#include <netinet/in.h>
#include <sys/socket.h>

#include <cctype>
#include <fstream>

#include "logger.hpp"
#include "webserv.hpp"

namespace {

tokens::type config_get_token_type(config_token token)
{
    switch (token.value[0]) {
#define X(name, val)                                                           \
    case val:                                                                  \
        return tokens::name;
        TOKENS
#undef X
    default:
        return tokens::WORD;
    }
}

keywords::type config_get_token_keyword(const config_token &token) // NOLINT
{
    if (token.value.empty())
        return keywords::UNKNOWN;
#define X(name, val, _, __, ___, ____, _____, _______)                         \
    if ((val) == token.value)                                                  \
        return keywords::name;
    KEYWORDS
#undef X
    return keywords::UNKNOWN;
}

uint32_t config_get_max_args(const config_token &token) // NOLINT
{
    if (token.value.empty())
        return 1;
    switch (token.keyword) {
#define X(name, _, arg, __, ___, ____, _____, _______)                         \
    case keywords::name:                                                       \
        return arg;
        KEYWORDS // NOLINT
#undef X
    }
    return 1;
}

bool config_directive_is_in_valide_scope( // NOLINT
    keywords::type root_keyword, keywords::type directive)
{

    if (root_keyword == keywords::ROOT && directive == keywords::HTTP)
        return true;
#define X(name, __, ___, ____, http, server, location, _______)                \
    if (root_keyword == keywords::HTTP && directive == keywords::name) {       \
        return (http);                                                         \
    }                                                                          \
    if (root_keyword == keywords::SERVER && directive == keywords::name) {     \
        return (server);                                                       \
    }                                                                          \
    if (root_keyword == keywords::LOCATION && directive == keywords::name) {   \
        return (location);                                                     \
    }
    KEYWORDS
#undef X
    return false;
}

bool config_is_a_valid_val(
    __attribute__((unused)) std::string &val, const keywords::type &type)
{
    switch (type) {
#define X(name, _, __, ___, ____, _____, ______, check)                        \
    case keywords::name:                                                       \
        return CALL(check, val);                                               \
        KEYWORDS
#undef X
    default:
        return true;
    }
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

#ifndef NDEBUG
void debug_tree(config_node *tree, int i)
{
    if (!tree)
        return;
    for (int j = 0; j < i; j++)
        std::cout << "\t";

    if (tree->type == NODE || tree->type == ROOT) {
        i++;
    }
    if (tree->key == "location" && tree->strict == 1)
        std::cout << tree->key << " = ";
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
#endif

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
        // TODO: MORE CPP STYLE
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
                tokens.push_back(tmp_token);
                i += len;
            } else if (config_is_special_char(buf[i])) {
                tmp_token.value = buf[i];
                tmp_token.alive = 1;
                tmp_token.line = line_i;
                tmp_token.type = config_get_token_type(tmp_token);
                tmp_token.keyword = config_get_token_keyword(tmp_token);
                tokens.push_back(tmp_token);
                i++;
            }
        }
        line_i++;
    }
    in_file.close();
    return true;
}

void delete_tree(config_node *root)
{
    if (!root)
        return;
    for (std::vector<config_node *>::iterator it = root->children.begin();
        it != root->children.end(); it++)
        delete_tree(*it);
    root->children.clear();
    delete root;
}

void config_free_tree(config_node *root)
{
    if (!root)
        return;
    while (root->parent)
        root = root->parent;
    delete_tree(root);
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

void config_set_alive_last_token(std::vector<config_token> &tokens)
{
    if (tokens.empty())
        return;
    for (std::vector<config_token>::iterator ite = tokens.end() - 1;
        ite != tokens.begin(); ite--) {
        if (!ite->alive) {
            ite->alive = 1;
            return;
        }
    }
}

bool config_node_already_present(
    std::vector<config_node *> &node, const keywords::type &key)
{
    if (key == keywords::ERROR_PAGE || key == keywords::SERVER_NAME)
        return false;
    for (std::vector<config_node *>::iterator it = node.begin();
        it != node.end(); it++) {
        if ((*it)->keyword == key)
            return true;
    }
    return false;
}

bool config_check_controversal_directive(
    std::vector<config_node *> &node, const keywords::type &key, uint32_t line)
{
    if (key == keywords::PROXY_PASS) {
        if (config_node_already_present(node, keywords::ROOT)) {
            L_ERROR("directive 'proxy_pass' conflicting with 'root' (line {})",
                line);
            return true;
        }
        if (config_node_already_present(node, keywords::TRY_FILES)) {
            L_ERROR(
                "directive 'proxy_pass' conflicting with 'try_files' (line {})",
                line);
            return true;
        }
        if (config_node_already_present(node, keywords::AUTOINDEX)) {
            L_ERROR(
                "directive 'proxy_pass' conflicting with 'autoindex' (line {})",
                line);
            return true;
        }
        if (config_node_already_present(node, keywords::INDEX)) {
            L_ERROR("directive 'proxy_pass' conflicting with 'index' (line {})",
                line);
            return true;
        }
    }
    if ((key == keywords::ROOT || key == keywords::TRY_FILES
            || key == keywords::AUTOINDEX || key == keywords::INDEX)
        && config_node_already_present(node, keywords::PROXY_PASS)) {
        L_ERROR("directive '{}' conflicting with 'proxy_pass' (line {})",
            stringss[key], line);
        return true;
    }
    return false;
}

// ignoring undefined scope
// scope_name is copy of token->str for prevent this value from being changed
void recovery(std::vector<config_token> &tokens, config_token *&token,
    std::string scope_name, uint32_t line, // NOLINT
    const std::string &root_key)
{
    int32_t depth = 1;

    // Init recovery
    while (token && token->type != tokens::BRACE_OPEN) {
        if (!config_see_next_token(tokens, token, true)) {
            return;
        }
    }

    while (token) {
        if (!config_see_next_token(tokens, token, true)) {
            return;
        }
        if (token->type == tokens::BRACE_OPEN) {
            depth++;
        } else if (token->type == tokens::BRACE_CLOSE) {
            depth--;
        }
        if (depth == 0) {
            L_WARN("can't open '{}' scope (line {}), inside '{}' scope, line "
                   "{} to {} "
                   "ignored",
                scope_name, line, root_key, line, token->line);
            config_skip_line(tokens, line);
            return;
        }
    }
}

// Create a location node, return false only on critical error, other error
// set valid value to false
bool create_location_node(config_node *&root, std::vector<config_token> &tokens,
    bool &valid, config_token *&token, uint32_t &error_count, int32_t &depth)
{
    config_node *node;
    uint32_t line = token->line;

    // check scope validity in scope
    if (!config_directive_is_in_valide_scope(root->keyword, token->keyword)) {
        recovery(tokens, token, token->value, line, root->key);
        return true;
    }

    // See next token without consume them
    if (!config_see_next_token(tokens, token, false))
        return true;

    // Initialize node
    try {
        node = new config_node();
        node->strict = false;
    } catch (...) {
        return false;
    }

    // Manage no arg location
    if (token->type == tokens::BRACE_OPEN) {
        config_see_next_token(tokens, token, true);
        node->key = "location";
        node->line = line;
        node->type = NODE;
        node->parent = root;
        node->keyword = keywords::LOCATION;
        root->children.push_back(node);
        root = node;
        depth++;
        return true;
    }

    //  Handle strict location
    if (token->type == tokens::EQUAL) {
        node->strict = true;
        config_see_next_token(tokens, token, true);
        // No other token, token_to_tree() handle scope error
        if (!config_see_next_token(tokens, token, false)) {
            delete node;
            return true;
        }
    }

    // if next token is bad, print an error and abort current line parsing
    if (token->type != tokens::WORD) {
        if (node->strict)
            L_ERROR("strict location need one parameter (line {})", line);
        else
            L_ERROR(
                "location need one parameter or BRACE_OPEN (line {})", line);
        error_count++;
        valid = false;
        config_skip_line(tokens, line);
        delete node;
        return true;
    }

    // push last child
    node->vals.push_back(token->value);
    config_see_next_token(tokens, token, true);
    if (!config_see_next_token(tokens, token, false)) {
        delete node;
        return true;
    }

    // verify is a new scope is opened
    if (token->type != tokens::BRACE_OPEN) {
        L_ERROR("special keyword 'location', need "
                "OPEN_BRACE after arg (line {})",
            line);
        config_set_alive_last_token(tokens);
        config_skip_line(tokens, line);
        error_count++;
        valid = false;
        delete node;
        return true;
    }

    // destroy BRACE_OPEN token
    config_see_next_token(tokens, token, true);
    depth++;
    node->key = "location";
    node->line = line;
    node->type = NODE;
    node->keyword = keywords::LOCATION;
    node->parent = root;

    root->children.push_back(node);
    root = node;
    return true;
}

// create http and server node
bool create_node(config_node *&root, std::vector<config_token> &tokens,
    bool &valid, config_token *&token, uint32_t &error_count, int32_t &depth)
{
    config_node *node;
    uint32_t line;

    line = token->line;

    // check scope validity in scope
    if (!config_directive_is_in_valide_scope(root->keyword, token->keyword)) {
        recovery(tokens, token, token->value, line, root->key);
        return true;
    }

    // Initialize node
    try {
        node = new config_node();
        node->key = token->value;
        node->type = NODE;
        node->parent = root;
        node->keyword = config_get_token_keyword(*token);
        node->line = line;
    } catch (...) {
        return false;
    }

    // http and server token must be followed directly by BRACE_OPEN
    if (!config_see_next_token(tokens, token, false)
        || token->type != tokens::BRACE_OPEN) {
        L_ERROR("special keyword '{}', need "
                "OPEN_BRACE (line {})",
            node->key, line);
        if (token) {
            config_skip_line(tokens, line);
        }
        valid = false;
        delete node;
        error_count++;
        return true;
    }

    // destroy BRACE_OPEN token
    config_see_next_token(tokens, token, true);
    depth++;
    root->children.push_back(node);
    root = node;
    return true;
}

bool config_create_leaf(config_node *&root, std::vector<config_token> &tokens,
    bool &valid, config_token *&token, uint32_t &error_count)
{
    config_node *node;
    config_token tmp_token;
    uint32_t line = token->line;

    // Directive need word token identified from keywords to start
    if (token->type != tokens::WORD || token->keyword == keywords::UNKNOWN) {
        tmp_token = *token;
        if (!config_see_next_token(tokens, token, false)
            || tmp_token.keyword != keywords::OPEN_SCOPE) {
            L_ERROR("unknown directive '{}' (line {}) for all directive {}",
                tmp_token.value, line, strings[tmp_token.keyword]);
            error_count++;
            config_skip_line(tokens, line);
            valid = false;
            return false;
        }
        recovery(tokens, token, tmp_token.value, tmp_token.line, root->key);
        valid = false;
        return true;
    }

    // Check for duplicate node
    if (config_node_already_present(root->children, token->keyword)) {
        L_WARN("directive '{}' (line {}) already present in node {} (line {}), "
               "ignoring",
            token->value, line, root->key, root->line);
        config_skip_line(tokens, line);
        return true;
    }

    if (config_check_controversal_directive(
            root->children, token->keyword, line)) {
        config_skip_line(tokens, line);
        valid = false;
        error_count++;
        return true;
    }

    // check  directive validity in scope
    if (!config_directive_is_in_valide_scope(root->keyword, token->keyword)) {
        L_ERROR("directive '{}' (line {}) is in a bad scope '{}' (line {}) {}",
            token->value, line, root->key, root->line, strings[token->keyword]);
        config_skip_line(tokens, line);
        valid = false;
        error_count++;
        return true;
    }

    // Initialize node
    try {
        node = new config_node();
        node->key = token->value;
        node->keyword = config_get_token_keyword(*token);
    } catch (...) {
        return false;
    }

    tmp_token = *token;
    if (!config_see_next_token(tokens, token, false)) {
        delete node;
        return true;
    }

    // add value(s) to leaf
    uint32_t ac = 0;
    uint32_t limit = config_get_max_args(tmp_token);

    while ((token->type == tokens::EQUAL || token->type == tokens::WORD)
        && ac < limit && line == token->line) {
        config_see_next_token(tokens, token, true);
        if (!config_is_a_valid_val(token->value, tmp_token.keyword)) {
            L_ERROR("'{}' is invalid value for '{}'", token->value,
                tmp_token.value);
            valid = false;
            config_skip_line(tokens, line);
            return true;
        }
        node->vals.push_back(token->value);
        ac++;
        if (!config_see_next_token(tokens, token, false)) {
            delete node;
            return true;
        }
    }

    // check directive end
    if (token->type != tokens::END) {
        if (token->type == tokens::BRACE_OPEN && token->line == line) {
            delete node;
            recovery(tokens, token, tmp_token.value, line, root->key);
            return true;
        }
        if (ac >= limit) {
            L_ERROR("directive '{}' can have up to {} argument(s) (line {}) {}",
                node->key, limit, line, strings[tmp_token.keyword]);
        } else {
            L_ERROR("no instruction end ';' (line {})", line);
        }
        error_count++;
        config_skip_line(tokens, line);
        valid = false;
        node->children.clear();
        delete node;
        return true;
    }

    // destroy END token
    config_see_next_token(tokens, token, true);

    // check directive value(s) is not empty
    if (node->vals.empty()) {
        L_WARN("directive '{}' (line {}) has no value, directive ignored",
            node->key, token->line);
        delete node;
        return true;
    }

    node->parent = root;
    root->children.push_back(node);
    return true;
}

bool iter_on_tokens(config_node *&root, std::vector<config_token> &tokens,
    bool &valid, config_token *&token, int32_t &depth, uint32_t &error_count)
{
    uint32_t line = token->line;

    if (!config_see_next_token(tokens, token, true))
        return true;

    // redirects the creation of a node to the corresponding type
    switch (token->keyword) {
    case keywords::LOCATION:
        return create_location_node(
            root, tokens, valid, token, error_count, depth);

    case keywords::HTTP:
    case keywords::SERVER:
        return create_node(root, tokens, valid, token, error_count, depth);

    case keywords::OPEN_SCOPE:
        recovery(tokens, token, token->value, line, root->key);
        return true;

    case keywords::CLOSE_SCOPE:
        if (!root->parent) {
            L_ERROR(
                "unexpected '}' all scopes are already closed (line {})", line);
            error_count++;
            valid = false;
            config_skip_line(tokens, line);
            return true;
        }
        depth--;
        root = root->parent;
        return true;

    default:
        if (!config_create_leaf(root, tokens, valid, token, error_count)) {
            config_free_tree(root);
            return false;
        }
    }
    return true;
}

bool token_to_tree(std::vector<config_token> &tokens, config_node *tree,
    int &depth, uint32_t &error_count)
{
    config_token tok;
    config_token *token = &tok;
    bool valid = true;

    if (!config_see_next_token(tokens, token, false) || tokens.empty()) {
        return false;
    }

    while (token) {
        if (!iter_on_tokens(tree, tokens, valid, token, depth, error_count))
            return false;
    }
    if (!valid) {
        config_free_tree(tree);
        return valid;
    }

    if (depth != 0) {
        error_count++;
        L_ERROR(
            "unexpected EOF missing '}' for close scope '{}' (opened line {})",
            tree->key, tree->line);
        config_free_tree(tree);
        return false;
    }

    return true;
}

bool get_first_val(
    const config_node &node, const std::string &key, std::string &val)
{
    for (std::vector<config_node *>::const_iterator it = node.children.begin();
        it != node.children.end(); it++) {
        if (key == (*it)->key) {
            val = *((*it)->vals.begin());
            return true;
        }
    }
    return false;
}

bool get_first_val_bool(
    const config_node &node, const std::string &key, bool &val)
{
    for (std::vector<config_node *>::const_iterator it = node.children.begin();
        it != node.children.end(); it++) {
        if (key == (*it)->key) {
            if (*((*it)->vals.begin()) == "on")
                val = true;
            if (*((*it)->vals.begin()) == "off")
                val = false;
            return true;
        }
    }
    return false;
}

bool create_server(const config_node &act, config_webserv srv_conf,
    std::vector<Server> &servers, const std::string &root_str,
    const std::string &servername, const std::string &addr)
{
    std::vector<Location> locations;
    bool has_location = false;

    for (std::vector<config_node *>::const_iterator it = act.children.begin();
        it != act.children.end(); it++) {
        if ((*it)->keyword == keywords::LOCATION) {
            has_location = true;
            get_first_val(**it, "index", srv_conf.index);
            get_first_val_bool(**it, "autoindex", srv_conf.autoindex);
            /*
            Config config = { srv_conf.root_path, srv_conf.index,
                srv_conf.client_max_body_size, srv_conf.autoindex };
            if ((*it)->vals.empty()) {
                locations.push_back((Location) { "/", config, (*it)->strict });
            } else {
                locations.push_back(
                    (Location) { ((*it)->vals[0]), config, (*it)->strict });
            }
            */
        }
    }
    //servers.push_back(Server(root_str, locations, servername, addr));
    return false;
    return has_location;
}

bool create_one_server(const config_node &start, std::vector<Server> &servers,
    bool &valid, config_webserv &conf)
{
    (void)conf;
    std::string root_str = conf.root_path;
    std::string servername;
    std::string addr;

    L_INFO("server find");
    if (!get_first_val(start, "server_name", servername)) {
        // L_DEBUG("no server_name in server (line {})", start.line);
        // return false;
    }
    if (!get_first_val(start, "listen", addr)) {
        L_DEBUG("no listen in server (line {})", start.line);
        return false;
    }
    (void)valid;
    create_server(start, conf, servers, root_str, servername, addr);
    return true;
}

void set_config_from_node(const config_node &node, config_webserv &conf)
{
    // TODO: allowed methods
    get_first_val(node, stringss[keywords::ERROR_PAGE], conf.error_page);
    get_first_val(node, stringss[keywords::ROOT], conf.root_path);
}

bool create_configuration(config_node *root, std::vector<Server> &servers)
{
    if (root->children.empty())
        return false;
    bool valid = false;
    std::vector<config_node *> node_stack = root->children;

    config_webserv main_conf = { };
    set_config_from_node(*node_stack[0], main_conf);
    config_webserv http_conf = main_conf;
    config_webserv server_conf = { };

    for (std::size_t i = 0; i < node_stack.size(); i++) {
        if (node_stack[i]->keyword == keywords::HTTP) {
            http_conf = main_conf;
            set_config_from_node(*node_stack[i], http_conf);
        }

        if (node_stack[i]->keyword == keywords::SERVER) {
            server_conf = http_conf;
            set_config_from_node(*node_stack[i], server_conf);
            create_one_server(*node_stack[i], servers, valid, server_conf);
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
    config_node *config_tree;
    uint32_t error_count = 0;
    int32_t depth = 0;

    if (!tokenize_config_file(path, tokens)) {
        return false;
    }

    if (tokens.empty()) {
        L_ERROR("empty configuration file", error_count);
        return false;
    }

    try {
        config_tree = new config_node();
        config_tree->key = "GLOBAL";
        config_tree->type = ROOT;
        config_tree->keyword = keywords::ROOT;
    } catch (...) {
        return false;
    }

    if (!token_to_tree(tokens, config_tree, depth, error_count)) {
        L_ERROR("invalid configuration file {} error(s)", error_count);
        return false;
    }

#ifndef NDEBUG
    debug_tree(config_tree, 0);
#endif

    (void)create_configuration;
    // create_configuration(config_tree, servers);

    if (servers.empty()) {
        L_ERROR("no server in configuration");
        config_free_tree(config_tree);
        return false;
    }

    config_free_tree(config_tree);
    return true;
}
