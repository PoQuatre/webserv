/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_parsing.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/23 00:10:43 by nlaporte          #+#    #+#             */
/*   Updated: 2026/05/23 02:10:17 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

#include <unistd.h>

#include <cstdint>

#include "config-parser.hpp"

// Test parsing utils

Test(config_parsing, time_utils)
{
    std::string str = "1400";

    cr_assert(time_check(str) == 1);
    cr_assert(convert_time_to_size_t(str) == 1);

    str = "13s";
    cr_assert(time_check(str) == 1);
    cr_assert(convert_time_to_size_t(str) == 13);

    str = "12345678999999999999s";
    cr_assert(time_check(str) == 1);
    cr_assert(convert_time_to_size_t(str) == INT64_MAX);

    str = "-12345678999999999999s";
    cr_assert(time_check(str) == 0);

    str = "1s1m1h";
    cr_assert(time_check(str) == 1);
    cr_assert(convert_time_to_size_t(str) == 3661);

    str = "1s1m3h";
    cr_assert(time_check(str) == 1);
    cr_assert(convert_time_to_size_t(str) == (3600 * 3) + 61);

    str = "1s1m3h1200ms";
    cr_assert(time_check(str) == 1);
    cr_assert(convert_time_to_size_t(str) == (3600 * 3) + 62);

    str = "1m1s3h1200";
    cr_assert(time_check(str) == 1);
    cr_assert(convert_time_to_size_t(str) == (3600 * 3) + 62);

    str = "lol";
    cr_assert(time_check(str) == 0);
}

Test(config_parsing, size_utils)
{
    std::string str = "1000";
    cr_assert(size_check(str) == 1);
    cr_assert(convert_string_to_size(str) == 0);

    str = "1000k";
    cr_assert(size_check(str) == 1);
    cr_assert(convert_string_to_size(str) == 1000);

    str = "1m";
    cr_assert(size_check(str) == 1);
    cr_assert(convert_string_to_size(str) == 1024);

    str = "3M";
    cr_assert(size_check(str) == 1);
    cr_assert(
        convert_string_to_size(str) == static_cast<std::size_t>(1024 * 3));

    str = "1G";
    cr_assert(size_check(str) == 1);
    cr_assert(convert_string_to_size(str) == static_cast<std::size_t>(1048576));

    str = "3G";
    cr_assert(size_check(str) == 1);
    cr_assert(
        convert_string_to_size(str) == static_cast<std::size_t>(1048576 * 3));

    str = "30000000000000000000000000000000000000000000000G";
    cr_assert(size_check(str) == 1);
    cr_assert(convert_string_to_size(str) == INT64_MAX);

    str = "-30000000000000000000000000000000000000000000000G";
    cr_assert(size_check(str) == 0);

    str = "30m1G";
    cr_assert(size_check(str) == 0);

    str = "3m1G";
    cr_assert(size_check(str) == 0);

    str = "3mG";
    cr_assert(size_check(str) == 0);

    str = "lol";
    cr_assert(size_check(str) == 0);
}

Test(config_parsing, good_simple)
{
    std::string path = "config_test/good/easy-0.conf";
    Parser parser(path);
    cr_assert(parser.parse_config() == 1);
    cr_assert(parser.create_all_servers() == 1);
    cr_assert(parser.get_all_servers().size() == 1);

    path = "config_test/good/easy-1.conf";
    Parser parser2(path);
    cr_assert(parser2.parse_config() == 1);
    cr_assert(parser2.create_all_servers() == 1);
    cr_assert(parser2.get_all_servers().size() == 2);

    path = "config_test/good/easy-3.conf";
    Parser parser3(path);
    cr_assert(parser3.parse_config() == 1);
    cr_assert(parser3.create_all_servers() == 1);
    cr_assert(parser3.get_all_servers().size() == 1);
}
