/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_parsing.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/23 00:10:43 by nlaporte          #+#    #+#             */
/*   Updated: 2026/06/17 21:10:00 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

#include <unistd.h>

#include <cstdint>
#include <vector>

#include "config-parser.hpp"
#include "logger.hpp"

// Test parsing utils

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
    cr_assert_eq(Parser::parse_config(path, servers), true);
    cr_assert_eq(servers.size(), 1);
    servers.clear();

    path = "test/data/conf-1.conf";
    cr_assert_eq(Parser::parse_config(path, servers), true);
    cr_assert_eq(servers.size(), 2);
    servers.clear();

    path = "test/data/conf-3.conf";
    cr_assert_eq(Parser::parse_config(path, servers), false);
    servers.clear();
}
