/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 18:53:25 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/13 06:17:02 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <vector>
#include "webserv.hpp"

int32_t main(int32_t ac, char **av)
{
    if (ac < 1)
	{
		std::cerr << "Usage ./webserv <configuration>\n";
		return 1;
	}
	std::vector<Server> servers = parse_config(av[1]);
	for (size_t i = 0; i < servers.size(); i++)
	{
	
	}
	(void)av;
    return 0;
}
