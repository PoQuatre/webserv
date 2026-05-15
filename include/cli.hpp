/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cli.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade <uanglade@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 17:57:29 by uanglade          #+#    #+#             */
/*   Updated: 2026/05/15 18:30:45 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <string>

namespace cli {

namespace flags {

#define CLI_FLAGS                                                              \
    X(TEST)                                                                    \
    X(SILENT)                                                                  \
    X(VERBOSE)

enum type {
#define X(name) name,
    CLI_FLAGS
#undef X
};

static const char *strings[] = {
#define X(name) #name,
    CLI_FLAGS
#undef X
};

static const std::size_t COUNT = sizeof(strings) / sizeof(*strings);

#undef CLI_FLAGS

}

struct ParsedArgs {
    bool flags[flags::COUNT];
    bool should_quit;
    std::string config_path;
};

ParsedArgs parse_arguments(uint32_t ac, char **av);
}
