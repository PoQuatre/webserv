/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticMap.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/10 03:52:13 by mle-flem          #+#    #+#             */
/*   Updated: 2026/06/10 04:03:40 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <cstddef>

#include "StringView.hpp"

template <std::size_t MaxEntries = 128>
class StaticMap {
public:
    struct Entry {
        StringView key;
        StringView val;
    };

    StaticMap()
        : _count(0)
    {
    }

    void clear() { _count = 0; }
    bool empty() const { return _count == 0; }
    std::size_t size() const { return _count; }

    bool insert(StringView key, StringView val)
    {
        if (_count >= MaxEntries)
            return false;
        _entries[_count].key = key;
        _entries[_count].val = val;
        ++_count;
        return true;
    }

    const StringView *find(const char *key) const
    {
        for (std::size_t i = 0; i < _count; ++i)
            if (_entries[i].key == key)
                return &_entries[i].val;
        return NULL;
    }

    std::size_t count(const char *key) const
    {
        std::size_t cnt = 0;
        for (std::size_t i = 0; i < _count; ++i)
            if (_entries[i].key == key)
                ++cnt;
        return cnt;
    }

    StringView at(const char *key) const
    {
        const StringView *v = find(key);
        return v ? *v : StringView();
    }

    const Entry *begin() const { return _entries; }
    const Entry *end() const { return _entries + _count; }

private:
    Entry _entries[MaxEntries];
    std::size_t _count;
};
