/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PosixRegex.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/29 19:37:08 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/29 19:57:05 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <regex.h>

#include <string>

class PosixRegex {
public:
    PosixRegex(const PosixRegex &other);
    PosixRegex(const std::string &pattern, int flags);
    ~PosixRegex();

    PosixRegex &operator=(const PosixRegex &other);

    bool matches(const std::string &str) const;

private:
    std::string _pattern;
    regex_t _regex;
    int _flags;
    bool _compiled;
};
