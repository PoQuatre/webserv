/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tokens.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/17 18:16:14 by nlaporte          #+#    #+#             */
/*   Updated: 2026/06/17 18:37:03 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <regex.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#include "config-parser-def.hpp"
#include "config-parser.hpp"
#include "logger.hpp"

namespace {

const char *strings[] = {
#define X(_, text, ...) #text,
#define X_SPECIAL(_, text, ...) text,
    KEYWORDS
#undef X_SPECIAL
#undef X
};

const std::size_t COUNT = sizeof(strings) / sizeof(*strings);

/*  _____ ___  _  _______ _   _   _   _ _____ ___ _     ____   */
/* |_   _/ _ \| |/ / ____| \ | | | | | |_   _|_ _| |   / ___|  */
/*   | || | | | ' /|  _| |  \| | | | | | | |  | || |   \___ \  */
/*   | || |_| | . \| |___| |\  | | |_| | | |  | || |___ ___) | */
/*   |_| \___/|_|\_\_____|_| \_|  \___/  |_| |___|_____|____/  */

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
        if (strings[i] == token.value)
            return static_cast<keywords::type>(i);
    }
    return keywords::UNKNOWN;
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
}

/*  ____   _    ____  ____  _____ ____    _____ ___  _  _______ _   _ ____   */
/* |  _ \ / \  |  _ \/ ___|| ____|  _ \  |_   _/ _ \| |/ / ____| \ | / ___|  */
/* | |_) / _ \ | |_) \___ \|  _| | |_) |   | || | | | ' /|  _| |  \| \___ \  */
/* |  __/ ___ \|  _ < ___) | |___|  _ <    | || |_| | . \| |___| |\  |___) | */
/* |_| /_/   \_\_|_\_\____/|_____|_|_\_\___|_|_\___/|_|\_\_____|_| \_|____/  */
/*           |  ___| | | | \ | |/ ___|_   _|_ _/ _ \| \ | / ___|             */
/*           | |_  | | | |  \| | |     | |  | | | | |  \| \___ \             */
/*           |  _| | |_| | |\  | |___  | |  | | |_| | |\  |___) |            */
/*           |_|    \___/|_| \_|\____| |_| |___\___/|_| \_|____/             */

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

int32_t Parser::create_token(std::string &buf, const std::size_t &i,
    std::size_t len, std::size_t &line_i)
{
    config_token tmp_token;
    char *p;
    std::size_t size = buf.length();

    tmp_token.alive = 1;
    tmp_token.line = line_i;
    if (buf[i] == '\'' || buf[i] == '"') {
        p = std::find(&buf[i + 1], &buf[size], buf[i]);
        if (!p) {
            L_ERROR("unclosed quote line {}", line_i);
            return -1;
        }
        len = p - &buf[i];
#if ALLOW_MULTILINE_STRING == 0
        char *p2;
        p2 = std::find(&buf[i + 1], &buf[size], '\n');
        if (p > p2) {
            std::size_t old_line = line_i;
            while (p2 && *p2 == '\n') {
                p2 = std::find(p2 + 1, p, '\n');
                line_i++;
            }
            L_WARN("newline encountered in string literal (line {}), "
                   "recovered at line {}",
                old_line, line_i);
            return static_cast<int32_t>(len) + 1;
        }
#endif
        if (len - 1 > 0) {
            tmp_token.value = buf.substr(i + 1, len - 1);
            tmp_token.type = tokens::WORD;
            tmp_token.keyword = keywords::UNKNOWN;
            _tokens.push_back(tmp_token);
            for (std::size_t j = i + 1; j < i + len - 1; j++) {
                if (buf[j] == '\n')
                    line_i++;
            }
            return static_cast<int32_t>(len) + 1;
        }
#if ALLOW_EMPTY_STRING == 1
        tmp_token.value = "";
        tmp_token.type = tokens::WORD;
        tmp_token.keyword = keywords::UNKNOWN;
        _tokens.push_back(tmp_token);
#else
        L_WARN("empty string at line {} ignored", line_i);
#endif
        return 2;
    }
    if (len > 0) {
        tmp_token.value = buf.substr(i, len);
        tmp_token.type = config_get_token_type(tmp_token);
        tmp_token.keyword = config_get_token_keyword(tmp_token);
        _tokens.push_back(tmp_token);
        return static_cast<int32_t>(len);
    }
    if (config_is_special_char(buf[i])) {
        tmp_token.value = buf[i];
        tmp_token.type = config_get_token_type(tmp_token);
        tmp_token.keyword = config_get_token_keyword(tmp_token);
        _tokens.push_back(tmp_token);
        return 1;
    }
    return 0;
}

bool Parser::tokenize()
{
    std::ifstream in_file;
    std::stringstream ss;
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

    ss << in_file.rdbuf();
    buf = ss.str();
    b_size = buf.size();
    for (std::size_t i = 0; b_size > i;) {
        len = 0;

        while (b_size > i && std::isspace(buf[i])) {
            if (buf[i] == '\n')
                line_i++;
            i++;
        }
        while (b_size > i + len && !std::isspace(buf[i + len])
            && !config_is_special_char(buf[i + len])) {
            len++;
        }
        if (buf[i] == '#') {
            while (buf[i] && buf[i] != '\n')
                i++;
        } else {
            int32_t len_copied;
            len_copied = create_token(buf, i, len, line_i);
            if (len_copied == -1)
                return false;
            i += len_copied;
        }
    }
    in_file.close();
    return true;
}
