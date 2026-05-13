/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade <uanglade@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 03:26:02 by uanglade          #+#    #+#             */
/*   Updated: 2026/05/13 04:44:57 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Logger.hpp"

std::ostream *Logger::_out_stream = &std::cout;

void Logger::set_log_output(std::ostream *stream) { _out_stream = stream; }
