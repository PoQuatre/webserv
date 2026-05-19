/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   http.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/19 06:24:34 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/19 06:26:31 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http.hpp"

#include <ostream>

std::ostream &operator<<(std::ostream &os, const http::request &req)
{
    os << http::methods::strings[req.method] << ' ' << req.uri << ' '
       << http::versions::strings[req.version];
    return os;
}
