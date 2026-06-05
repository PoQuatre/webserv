/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HeaderMap.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 06:12:44 by mle-flem          #+#    #+#             */
/*   Updated: 2026/06/05 03:55:43 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <cstddef>

#include "StringView.hpp"

#define HEADER_MAP_MAX_HEADERS 128

class HeaderMap {
public:
    struct Entry {
        StringView name;
        StringView val;
    };

    HeaderMap();

    void clear();
    bool empty() const;
    std::size_t size() const;

    bool insert(StringView name, StringView val);
    const StringView *find(const char *name) const;
    std::size_t count(const char *name) const;
    StringView at(const char *name) const;

    const Entry *begin() const;
    const Entry *end() const;

private:
    Entry _entries[HEADER_MAP_MAX_HEADERS];
    std::size_t _count;
};
