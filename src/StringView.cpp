/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StringView.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 03:02:18 by mle-flem          #+#    #+#             */
/*   Updated: 2026/06/03 03:02:19 by mle-flem         ###   ########.fr       */
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
