/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade <uanglade@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 03:12:02 by uanglade          #+#    #+#             */
/*   Updated: 2026/05/13 06:00:55 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

enum LogLevel {
    LEVEL_DEBUG = 0,
    LEVEL_INFO,
    LEVEL_WARNING,
    LEVEL_ERROR,
};

#define FMT_DEBUG "\x1b[0;1;7;95m DEBUG \x1b[0m "
#define FMT_INFO "\x1b[0;1;7;96m INFO  \x1b[0m "
#define FMT_WARNING "\x1b[0;1;7;93m WARN  \x1b[0m "
#define FMT_ERROR "\x1b[0;1;7;31m ERROR \x1b[0m "

class Logger {
public:
    static void set_log_output(std::ostream *stream);

    template <typename A>
    static void log(LogLevel level, const std::string &format, A arg1)
    {
        if (level < _logger_level - 1)
            return;

        char time_buff[30];
        time_t now = time(0);
        struct tm tstruct;
        tstruct = *localtime(&now);
        if (!strftime(time_buff, sizeof(time_buff), "%Y-%m-%d %X", &tstruct))
            return;

        *_out_stream << "\x1b[2m" << time_buff << "\x1b[0m ";

        switch (level) {
        case LEVEL_DEBUG:
            *_out_stream << FMT_DEBUG;
            break;
        case LEVEL_INFO:
            *_out_stream << FMT_INFO;
            break;
        case LEVEL_WARNING:
            *_out_stream << FMT_WARNING;
            break;
        case LEVEL_ERROR:
            *_out_stream << FMT_ERROR;
            break;
        }

        std::stringstream stream;

        size_t next_param = 0;
        size_t last_pos = 0;
        uint32_t curr_param = 0;

        while (next_param != std::string::npos) {
            next_param = format.find("{}", last_pos + 1);
            if (next_param == std::string::npos) {

                // stream << "ascacs";
                stream.write(format.c_str() + last_pos,
                    static_cast<int64_t>(format.length() - last_pos));
                break;
            }

            stream.write(format.c_str() + last_pos,
                static_cast<int64_t>(next_param - last_pos));

            last_pos = next_param + 2;
            switch (curr_param) {
            case 0:
                stream << arg1;
                break;
            default:;
            }
            curr_param++;
        }
        *_out_stream << stream.str() << "\n";
    }

    template <typename A, typename B>
    static void log(LogLevel level, const std::string &format, A arg1, B arg2)
    {
        if (level < _logger_level - 1)
            return;

        char time_buff[30];
        time_t now = time(0);
        struct tm tstruct;
        tstruct = *localtime(&now);
        if (!strftime(time_buff, sizeof(time_buff), "%Y-%m-%d %X", &tstruct))
            return;

        *_out_stream << "\x1b[2m" << time_buff << "\x1b[0m ";

        switch (level) {
        case LEVEL_DEBUG:
            *_out_stream << FMT_DEBUG;
            break;
        case LEVEL_INFO:
            *_out_stream << FMT_INFO;
            break;
        case LEVEL_WARNING:
            *_out_stream << FMT_WARNING;
            break;
        case LEVEL_ERROR:
            *_out_stream << FMT_ERROR;
            break;
        }
        std::stringstream stream;

        size_t next_param = 0;
        size_t last_pos = 0;
        uint32_t curr_param = 0;

        while (next_param != std::string::npos) {
            next_param = format.find("{}", last_pos + 1);
            if (next_param == std::string::npos) {
                stream.write(format.c_str() + last_pos,
                    static_cast<int64_t>(format.length() - last_pos));
                break;
            }

            stream.write(format.c_str() + last_pos,
                static_cast<int64_t>(next_param - last_pos));

            last_pos = next_param + 2;

            switch (curr_param) {
            case 0:
                stream << arg1;
                break;
            case 1:
                stream << arg2;
                break;
            default:;
            }
            curr_param++;
        }

        *_out_stream << stream.str() << "\n";
    }

    template <typename A, typename B, typename C>
    static void log(
        LogLevel level, const std::string &format, A arg1, B arg2, C arg3)
    {
        if (level < _logger_level - 1)
            return;

        char time_buff[30];
        time_t now = time(0);
        struct tm tstruct;
        tstruct = *localtime(&now);
        if (!strftime(time_buff, sizeof(time_buff), "%Y-%m-%d %X", &tstruct))
            return;

        *_out_stream << "\x1b[2m" << time_buff << "\x1b[0m ";

        switch (level) {
        case LEVEL_DEBUG:
            *_out_stream << FMT_DEBUG;
            break;
        case LEVEL_INFO:
            *_out_stream << FMT_INFO;
            break;
        case LEVEL_WARNING:
            *_out_stream << FMT_WARNING;
            break;
        case LEVEL_ERROR:
            *_out_stream << FMT_ERROR;
            break;
        }

        std::stringstream stream;

        size_t next_param = 0;
        size_t last_pos = 0;
        uint32_t curr_param = 0;

        while (next_param != std::string::npos) {
            next_param = format.find("{}", last_pos + 1);
            if (next_param == std::string::npos) {
                stream.write(format.c_str() + last_pos,
                    static_cast<int64_t>(format.length() - last_pos));
                break;
            }

            stream.write(format.c_str() + last_pos,
                static_cast<int64_t>(next_param - last_pos));

            last_pos = next_param + 2;

            switch (curr_param) {
            case 0:
                stream << arg1;
                break;
            case 1:
                stream << arg2;
                break;
            case 2:
                stream << arg3;
                break;
            default:;
            }
            curr_param++;
        }
        *_out_stream << stream.str() << "\n";
    }

private:
#ifndef NDEBUG
    static const LogLevel _logger_level = LEVEL_INFO;
#else
    static const LogLevel _logger_level = LEVEL_ERROR;
#endif // !NDEBUG

    static std::ostream *_out_stream;
};

#define L_DEBUG(format, ...) Logger::log(LEVEL_DEBUG, format, __VA_ARGS__)
#define L_INFO(format, ...) Logger::log(LEVEL_INFO, format, __VA_ARGS__)
#define L_WARN(format, ...) Logger::log(LEVEL_WARNING, format, __VA_ARGS__)
#define L_ERROR(format, ...) Logger::log(LEVEL_ERROR, format, __VA_ARGS__)
