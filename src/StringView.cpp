/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StringView.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 03:02:18 by mle-flem          #+#    #+#             */
/*   Updated: 2026/06/05 03:53:00 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "StringView.hpp"

StringView::StringView()
    : _data(NULL)
    , _size(0)
{
}

StringView::StringView(const char *data, std::size_t size)
    : _data(data)
    , _size(size)
{
}

const char *StringView::data() const { return _data; }

std::size_t StringView::size() const { return _size; }

bool StringView::empty() const { return _size == 0; }

std::string StringView::str() const
{
    return _data ? std::string(_data, _size) : std::string();
}

int32_t StringView::compare(const StringView &other) const
{
    int64_t size_diff
        = static_cast<int64_t>(_size) - static_cast<int64_t>(other._size);
    std::size_t n = std::min(_size, other._size);
    if (n == 0)
        return (size_diff > 0) - (size_diff < 0);
    int32_t cmp = std::char_traits<char>::compare(_data, other._data, n);
    if (cmp != 0)
        return (cmp > 0) - (cmp < 0);
    return (size_diff > 0) - (size_diff < 0);
}

int32_t StringView::compare(const char *str) const
{
    return compare(StringView(str, std::strlen(str)));
}

int32_t StringView::compare(const std::string &str) const
{
    return compare(StringView(str.data(), str.size()));
}

int32_t StringView::compare(
    std::size_t pos, std::size_t count, const StringView &str) const
{
    pos = std::min(pos, _size);
    count = std::min(count, _size - pos);
    return StringView(_data + pos, count).compare(str);
}

int32_t StringView::compare(
    std::size_t pos, std::size_t count, const char *str) const
{
    return compare(pos, count, StringView(str, std::strlen(str)));
}

int32_t StringView::compare(
    std::size_t pos, std::size_t count, const std::string &str) const
{
    return compare(pos, count, StringView(str.data(), str.size()));
}

char StringView::operator[](std::size_t i) const { return _data[i]; }

bool StringView::operator==(const StringView &other) const
{
    if (_size != other._size)
        return false;
    if (_data == other._data)
        return true;
    return std::char_traits<char>::compare(_data, other._data, _size) == 0;
}

bool StringView::operator!=(const StringView &other) const
{
    return !(*this == other);
}

bool StringView::operator==(const char *str) const
{
    if (_size != std::strlen(str))
        return false;
    return std::char_traits<char>::compare(_data, str, _size) == 0;
}

bool StringView::operator!=(const char *str) const { return !(*this == str); }

bool StringView::operator==(const std::string &str) const
{
    if (_size != str.size())
        return false;
    return std::char_traits<char>::compare(_data, str.data(), _size) == 0;
}

bool StringView::operator!=(const std::string &str) const
{
    return !(*this == str);
}

bool StringView::operator<(const StringView &other) const
{
    int32_t cmp = std::char_traits<char>::compare(
        _data, other._data, std::min(_size, other._size));
    if (cmp != 0)
        return cmp < 0;
    return _size < other._size;
}

std::ostream &operator<<(std::ostream &os, const StringView &sv)
{
    if (!sv.empty())
        os.write(sv.data(), static_cast<std::streamsize>(sv.size()));
    return os;
}
