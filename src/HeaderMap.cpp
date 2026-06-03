/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HeaderMap.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 06:30:15 by mle-flem          #+#    #+#             */
/*   Updated: 2026/06/03 06:31:26 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HeaderMap.hpp"

HeaderMap::HeaderMap()
    : _count(0)
{
}

void HeaderMap::clear() { _count = 0; }

bool HeaderMap::empty() const { return _count == 0; }

std::size_t HeaderMap::size() const { return _count; }

bool HeaderMap::insert(StringView name, StringView val)
{
    if (_count >= HEADER_MAP_MAX_HEADERS)
        return false;
    _entries[_count].name = name;
    _entries[_count].val = val;
    ++_count;
    return true;
}

const StringView *HeaderMap::find(const char *name) const
{
    for (std::size_t i = 0; i < _count; ++i)
        if (_entries[i].name == name)
            return &_entries[i].val;
    return NULL;
}

std::size_t HeaderMap::count(const char *name) const
{
    std::size_t cnt = 0;
    for (std::size_t i = 0; i < _count; ++i)
        if (_entries[i].name == name)
            ++cnt;
    return cnt;
}

const HeaderMap::Entry *HeaderMap::begin() const { return _entries; }

const HeaderMap::Entry *HeaderMap::end() const { return _entries + _count; }
