/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server_socket.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 18:34:27 by mle-flem          #+#    #+#             */
/*   Updated: 2026/07/20 13:40:11 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cerrno>

#include "Server.hpp"
#include "logger.hpp"

Test(server_socket, init_and_shutdown_without_epoll)
{
    logger::log_level() = logger::levels::NOTHING;

    Server server(std::vector<Location>(), "test", "127.0.0.1:0", Config(), 1);

    cr_assert(server.init_socket());

    int32_t fd = server.get_sockfd();
    cr_assert_geq(fd, 0);

    sockaddr_in addr = { };
    socklen_t addrlen = sizeof(addr);
    cr_assert_eq(
        getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &addrlen), 0);
    cr_assert_neq(ntohs(addr.sin_port), 0);

    server.shutdown_socket();

    errno = 0;
    cr_assert_eq(fcntl(fd, F_GETFD), -1);
    cr_assert_eq(errno, EBADF);
}
