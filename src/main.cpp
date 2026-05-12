/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 18:53:25 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/12 18:57:52 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdint.h>

#include <iostream>

int32_t main(int32_t ac, char **av)
{
    std::cout << "Hello world\n\n";
    std::cout << "Got " << ac - 1 << " args:\n";
    if (ac > 1)
        for (int32_t i = 1; i < ac; ++i)
            std::cout << "  - '" << av[i] << "'\n";
    else
        std::cout << "  No args :(\n";
    return 0;
}
