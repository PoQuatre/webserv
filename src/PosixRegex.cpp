/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PosixRegex.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/29 19:41:13 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/29 20:07:00 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "PosixRegex.hpp"

#include <stdexcept>
#include <vector>

namespace {

std::string error_message(int code, const regex_t *regex)
{
    std::size_t len = regerror(code, regex, 0, 0);

    if (len == 0)
        return std::string();

    std::vector<char> buffer(len);
    regerror(code, regex, &buffer[0], buffer.size());

    return std::string(&buffer[0]);
}

}

PosixRegex::PosixRegex(const PosixRegex &other)
    : _pattern(other._pattern)
    , _flags(other._flags)
    , _compiled(false)
{
    int code = regcomp(&_regex, _pattern.c_str(), _flags);
    if (code != 0)
        throw std::runtime_error(error_message(code, &_regex));

    _compiled = true;
}

PosixRegex::PosixRegex(const std::string &pattern, int flags)
    : _pattern(pattern)
    , _flags(flags)
    , _compiled(false)
{
    int code = regcomp(&_regex, _pattern.c_str(), _flags);
    if (code != 0)
        throw std::runtime_error(error_message(code, &_regex));

    _compiled = true;
}

PosixRegex::~PosixRegex()
{
    if (_compiled)
        regfree(&_regex);
}

PosixRegex &PosixRegex::operator=(const PosixRegex &other)
{
    if (this == &other)
        return *this;

    std::string new_pattern(other._pattern);
    const int new_flags = other._flags;

    if (_compiled) {
        regfree(&_regex);
        _compiled = false;
    }

    _pattern.swap(new_pattern);
    _flags = new_flags;

    int code = regcomp(&_regex, _pattern.c_str(), _flags);
    if (code != 0)
        throw std::runtime_error(error_message(code, &_regex));

    _compiled = true;
    return *this;
}

bool PosixRegex::matches(const std::string &str) const
{
    if (!_compiled)
        return false;
    return regexec(&_regex, str.c_str(), 0, 0, 0) == 0;
}
