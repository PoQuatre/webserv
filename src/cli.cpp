/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cli.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade <uanglade@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 18:01:28 by uanglade          #+#    #+#             */
/*   Updated: 2026/05/17 23:00:39 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cli.hpp"

#include <cstring>
#include <iostream>

#include "logger.hpp"

namespace {

void print_help()
{

    std::cout << "Usage: ./webserv [options] config-file\n";
    std::cout << "Options:\n"
                 "\t-h, --help  show the help message\n"
                 "\t-t, --test  only validate the config\n"
                 "\t-v, --verbose make the server more talkative\n"
                 "\t-s, --silent make the server silent\n";
}

bool parse_flags(cli::ParsedArgs &args, const std::string &argument)
{
    std::string flags_triggers[3];

    if (argument == "--help") {
        print_help();
        args.should_quit = true;
        return false;
    }

    if (argument[1] == '-') {
        bool found_flag = false;
        for (uint32_t i = 0; i < cli::flags::COUNT; i++) {
            if (cli::flags::long_flags[i] == argument.substr(2)) {
                args.flags[i] = true;
                found_flag = true;
            }
        }
        if (!found_flag) {
            L_ERROR("Incorrect or malformed argument: {}", argument);
            args.should_quit = true;
            return false;
        }
    } else {
        std::string::const_iterator cite = argument.end();
        for (std::string::const_iterator cit = ++argument.begin(); cit != cite;
            ++cit) {
            bool found_flag = false;
            if (*cit == 'h') {
                print_help();
                args.should_quit = true;
                return false;
            }
            for (uint32_t j = 0; j < cli::flags::COUNT; j++) {

                if (*cit == cli::flags::short_flags[j]) {
                    args.flags[j] = true;
                    found_flag = true;
                }
            }

            if (!found_flag) {
                L_ERROR("Incorrect or malformed argument: {}", argument);
                args.should_quit = true;
                return false;
            }
        }
    }
    return true;
}
}

namespace cli {

ParsedArgs parse_arguments(uint32_t ac, char **av)
{
    ParsedArgs args = { { false, false, false }, false, "" };

    L_DEBUG("Parsing cli arguments");
    if (ac < 2) {
        std::cerr << "Usage ./webserv <configuration>\n";
        args.should_quit = true;
        return args;
    }

    bool path_found = false;

    for (uint32_t i = 1; i < ac; i++) {
        char *found = std::strchr(av[i], '-');
        if (!found) {
            if (path_found) {
                L_ERROR("Got two path: {}, {}", args.config_path, av[i]);
                args.should_quit = true;
                return args;
            }
            args.config_path = av[i];
            path_found = true;
        } else if (found == &av[i][0]) {
            if (!parse_flags(args, av[i]))
                return args;

        } else {
            L_ERROR("incorrect argument: {}", av[i]);
            args.should_quit = true;
            return args;
        }
    }

    return args;
}

}
