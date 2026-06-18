/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_parsing.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/23 00:10:43 by nlaporte          #+#    #+#             */
/*   Updated: 2026/06/18 20:16:35 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <vector>

#include "config-parser.hpp"
#include "logger.hpp"

// Test parsing utils

bool size_check(const std::string &val)
{
    std::size_t i;
    for (i = 0; val[i]; i++) {
        if (!std::isdigit(val[i]))
            break;
    }
    if (!val[i])
        return true;
    if ((val[i] == 'k' || val[i] == 'K') && i + 1 == (val.size()))
        return true;
    if ((val[i] == 'm' || val[i] == 'M') && i + 1 == (val.size()))
        return true;
    if ((val[i] == 'g' || val[i] == 'G') && i + 1 == (val.length()))
        return true;
    return (std::isdigit(val[i]));
}

std::size_t convert_string_to_size(const std::string &val)
{
    int64_t i;
    char *p;

    i = std::strtol(val.c_str(), &p, 10);
    if (!*p)
        return i;
    if (*p == 'k' || *p == 'K')
        return (INT64_MAX / 1024 < i ? INT64_MAX : i * 1024);
    if (*p == 'm' || *p == 'M')
        return (INT64_MAX / 1048576 < i ? INT64_MAX : i * 1048576);
    if ((*p == 'g' || *p == 'G'))
        return (INT64_MAX / 1073741824 < i ? INT64_MAX : i * 1073741824);
    return 0;
}

std::size_t convert_time_to_size_t(const std::string &val)
{
    int64_t i = 0;
    std::size_t final_size = 0;
    const char *str = val.c_str();
    char *p;

    while (1) {
        i = std::strtol(str, &p, 10);
        if (i < 0)
            return 9;
        str = p;
        if (!*str)
            return final_size + (i / 1000);
        if (str[1] && str[0] == 'm' && str[1] == 's') {
            final_size += (i / 1000);
            str += 2;
        } else {
            switch (*str) {
            case 's':
                final_size += i;
                str++;
                break;
            case 'm':
                final_size += i * 60;
                str++;
                break;
            case 'h':
                final_size += i * 3600;
                str++;
                break;
                /*
        case 'd':
        case 'w':
        case 'M':
        case 'y':
                */
            default:
                return final_size;
            }
        }
        if (!*p)
            return final_size;
    }
    return 0;
}

bool time_check(const std::string &val)
{
    bool valid = false;
    uint32_t i = 0;

    while (1) {
        while (val[i] && std::isdigit(val[i])) {
            i++;
        }
        if (i + 1 < val.length() && val[i] == 'm' && (val[i + 1]) == 's') {
            valid = true;
            i += 2;
        } else {
            switch (val[i]) {
            case 's':
            case 'm':
            case 'h':
                /*
            case 'd':
            case 'w':
            case 'M':
            case 'y':
*/
                i++;
                valid = true;
                break;
            default:
                return (!val[i]);
            }
        }
        if (i == val.length())
            return valid;
    }
    return valid;
}

Test(config_parsing, time_utils)
{
    std::string str = "1400";

    cr_assert_eq(time_check(str), 1);
    cr_assert_eq(convert_time_to_size_t(str), 1);

    str = "13s";
    cr_assert_eq(time_check(str), 1);
    cr_assert_eq(convert_time_to_size_t(str), 13);

    str = "12345678999999999999s";
    cr_assert_eq(time_check(str), 1);
    cr_assert_eq(convert_time_to_size_t(str), INT64_MAX);

    str = "-12345678999999999999s";
    cr_assert_eq(time_check(str), 0);

    str = "1s1m1h";
    cr_assert_eq(time_check(str), 1);
    cr_assert_eq(convert_time_to_size_t(str), 3661);

    str = "1s1m3h";
    cr_assert_eq(time_check(str), 1);
    cr_assert_eq(convert_time_to_size_t(str), (3600 * 3) + 61);

    str = "1s1m3h1200ms";
    cr_assert_eq(time_check(str), 1);
    cr_assert_eq(convert_time_to_size_t(str), (3600 * 3) + 62);

    str = "1m1s3h1200";
    cr_assert_eq(time_check(str), 1);
    cr_assert_eq(convert_time_to_size_t(str), (3600 * 3) + 62);

    str = "lol";
    cr_assert_eq(time_check(str), 0);
}

Test(config_parsing, size_utils)
{
    std::size_t wanted;
    std::string str = "1000";

    cr_assert_eq(size_check(str), 1);
    cr_assert_eq(convert_string_to_size(str), 1000);

    str = "1000k";
    wanted = 1024000;
    cr_assert_eq(size_check(str), 1);
    cr_assert_eq(convert_string_to_size(str), wanted);

    str = "1m";
    wanted = 1048576;
    cr_assert_eq(size_check(str), 1);
    cr_assert_eq(convert_string_to_size(str), wanted);

    str = "3M";
    wanted = 3145728;
    cr_assert_eq(size_check(str), 1);
    cr_assert_eq(convert_string_to_size(str), wanted);

    str = "1G";
    wanted = 1073741824;
    cr_assert_eq(size_check(str), 1);
    cr_assert_eq(convert_string_to_size(str), wanted);

    str = "3G";
    wanted = 3221225472;
    cr_assert_eq(size_check(str), 1);
    cr_assert_eq(convert_string_to_size(str), wanted);

    str = "30000000000000000000000000000000000000000000000G";
    cr_assert_eq(size_check(str), 1);
    cr_assert_eq(convert_string_to_size(str), INT64_MAX);

    str = "-30000000000000000000000000000000000000000000000G";
    cr_assert_eq(size_check(str), 0);

    str = "30m1G";
    cr_assert_eq(size_check(str), 0);

    str = "3m1G";
    cr_assert_eq(size_check(str), 0);

    str = "3mG";
    cr_assert_eq(size_check(str), 0);

    str = "lol";
    cr_assert_eq(size_check(str), 0);
}

Test(config_parsing, good_simple)
{
    std::vector<Server> servers;

    std::string path = "test/data/conf-0.conf";
    cr_assert_eq(ConfigParser::parse_config(path, servers), true);
    cr_assert_eq(servers.size(), 1);
    servers.clear();

    path = "test/data/conf-1.conf";
    cr_assert_eq(ConfigParser::parse_config(path, servers), true);
    cr_assert_eq(servers.size(), 2);
    servers.clear();

    path = "test/data/conf-3.conf";
    cr_assert_eq(ConfigParser::parse_config(path, servers), false);
    servers.clear();
}
