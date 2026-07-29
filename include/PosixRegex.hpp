/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PosixRegex.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/29 19:37:08 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/29 20:16:59 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <regex.h>

#include <string>

class PosixRegex {
public:
    PosixRegex();
    PosixRegex(const PosixRegex &other);
    PosixRegex(const std::string &pattern, int flags);
    ~PosixRegex();

    PosixRegex &operator=(const PosixRegex &other);

    bool matches(const std::string &str, int flags = REG_STARTEND) const;

private:
    std::string _pattern;
    regex_t _regex;
    int _flags;
    bool _compiled;
};
