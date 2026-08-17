/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 18:53:25 by mle-flem          #+#    #+#             */
/*   Updated: 2026/08/14 05:08:15 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdint.h>

#include <vector>

#include "EventLoop.hpp"
#include "cli.hpp"
#include "config-parser.hpp"
#include "logger.hpp"

int32_t main(int32_t ac, char **av)
{
    try {
        logger::print_date() = false;
        std::vector<Server> servers;

        cli::ParsedArgs args = cli::parse_arguments(ac, av);
        if (args.should_quit)
            return 1;

        if (args.flags[cli::flags::SILENT])
            logger::log_level() = logger::levels::NOTHING;
        if (args.flags[cli::flags::VERBOSE])
            logger::log_level() = logger::levels::TRACE;

        if (!ConfigParser::parse_config(args.config_path, servers))
            return 1;

        logger::print_date() = true;
        EventLoop loop(servers);
        return loop.run() ? 0 : 1;
    } catch (const std::exception &e) {
        std::cerr << "An exception occurred: " << e.what() << '\n';
        return 1;
    }
}
