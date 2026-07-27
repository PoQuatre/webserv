/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test_helpers.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/26 21:35:58 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/27 18:30:53 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <criterion/criterion.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

inline std::string test_tmpdir(const char *prefix)
{
    std::string tmpl = std::string("/tmp/") + prefix + "-XXXXXX";
    std::vector<char> buffer(tmpl.begin(), tmpl.end());

    buffer.push_back('\0');
    char *dir = mkdtemp(buffer.data());
    cr_assert_not_null(dir, "mkdtemp() failed: %s", strerror(errno));
    return dir;
}

inline void test_write_file(const std::string &path, const std::string &content)
{
    std::ofstream out(path.c_str(), std::ios::binary);

    cr_assert(out.is_open(), "failed to open %s", path.c_str());
    out << content;
    cr_assert(!out.fail(), "failed to write %s", path.c_str());
}

inline void test_assert_status(const std::string &response, const char *status)
{
    cr_assert_neq(response.find(status), std::string::npos,
        "missing status '%s' in response:\n%s", status, response.c_str());
}
