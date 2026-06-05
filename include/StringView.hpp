/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StringView.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 02:37:38 by mle-flem          #+#    #+#             */
/*   Updated: 2026/06/05 03:53:12 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <cstddef>
#include <cstring>
#include <ostream>
#include <string>

class StringView {
public:
    StringView();
    StringView(const char *data, std::size_t size);

    const char *data() const;
    std::size_t size() const;
    bool empty() const;

    std::string str() const;

    int32_t compare(const StringView &other) const;
    int32_t compare(const char *str) const;
    int32_t compare(const std::string &str) const;
    int32_t compare(
        std::size_t pos, std::size_t count, const StringView &str) const;
    int32_t compare(std::size_t pos, std::size_t count, const char *str) const;
    int32_t compare(
        std::size_t pos, std::size_t count, const std::string &str) const;

    char operator[](std::size_t i) const;

    bool operator==(const StringView &other) const;
    bool operator!=(const StringView &other) const;

    bool operator==(const char *str) const;
    bool operator!=(const char *str) const;

    bool operator==(const std::string &str) const;
    bool operator!=(const std::string &str) const;

    bool operator<(const StringView &other) const;

private:
    const char *_data;
    std::size_t _size;
};

inline bool operator==(const char *s, const StringView &sv) { return sv == s; }

inline bool operator==(const std::string &s, const StringView &sv)
{
    return sv == s;
}

inline bool operator!=(const char *s, const StringView &sv) { return sv != s; }

inline bool operator!=(const std::string &s, const StringView &sv)
{
    return sv != s;
}

std::ostream &operator<<(std::ostream &os, const StringView &sv);
