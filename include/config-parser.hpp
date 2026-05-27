/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config-parser.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 07:53:07 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/27 04:42:38 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <cstring>
#include <string>
#include <vector>

#include "Server.hpp"
#include "config-parser-def.hpp"
#include "logger.hpp"

#define DEFAULT_ROOT "/"
#define DEFAULT_LISTEN "127.0.0.1:8080"
#define DEFAULT_SERVER_NAME ""

// #define CALL(macro, val) macro(val);

bool string_check(const std::string &string);
bool bool_check(const std::string &string);
bool time_check(const std::string &val);
bool int_check(const std::string &string);
bool size_check(const std::string &val);
std::size_t convert_time_to_size_t(const std::string &val);
std::size_t convert_string_to_size(const std::string &val);

class Parser {
public:
    Parser(const std::string &path);
    ~Parser();
    bool parse_config();
    bool create_all_servers();
    std::vector<Server> get_all_servers();

private:
    Parser();
    void skip_line();
    bool see_next_token();
    bool consume_next_token();
    void skip_scope(
        const std::string &scope_name, uint32_t line, bool print_err = true);
    bool create_location_node();
    bool create_node();
    bool create_leaf();
    bool create_tree();
    void config_set_alive_last_token();
    bool iter_on_tokens();
    bool token_to_tree();
    void free_tree(config_node *root);
    bool tokenize();
    void delete_tree(config_node *root);
    void create_one_server(const config_node &node,
        std::vector<Location> location_vector,
        std::map<keywords::type, std::vector<std::string> > &server_conf);
    bool check_controversal_directive(std::vector<config_node *> &node,
        const keywords::type &key, uint32_t line);

    const std::string _path;
    std::vector<config_token> _tokens;
    std::vector<Server> _servers;
    config_node *_root;
    config_token *_act_token;
    uint32_t _err_count;
    int32_t _depth;
    bool _valid;
    // #ifndef NDEBUG
    void debug_tree(config_node *tree, int i);
    // #endif
};
