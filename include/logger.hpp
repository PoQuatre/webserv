/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   logger.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/14 17:56:20 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/14 20:23:01 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>

#include <iostream>
#include <sstream>
#include <string>

//
// ENUM_PARAMS(n, T) -> T0, T1, ..., T(n-1)
//
#define ENUM_PARAMS_1(T) T##0
#define ENUM_PARAMS_2(T) ENUM_PARAMS_1(T), T##1
#define ENUM_PARAMS_3(T) ENUM_PARAMS_2(T), T##2
#define ENUM_PARAMS_4(T) ENUM_PARAMS_3(T), T##3
#define ENUM_PARAMS_5(T) ENUM_PARAMS_4(T), T##4
#define ENUM_PARAMS_6(T) ENUM_PARAMS_5(T), T##5
#define ENUM_PARAMS_7(T) ENUM_PARAMS_6(T), T##6
#define ENUM_PARAMS_8(T) ENUM_PARAMS_7(T), T##7
#define ENUM_PARAMS_9(T) ENUM_PARAMS_8(T), T##8
#define ENUM_PARAMS_10(T) ENUM_PARAMS_9(T), T##9

//
// ENUM_BINARY_PARAMS(n, T, S) -> T0 S0, T1 S1, ..., T(n-1) S(n-1)
//
#define ENUM_BINARY_PARAMS_1(T, S) T##0 S##0
#define ENUM_BINARY_PARAMS_2(T, S) ENUM_BINARY_PARAMS_1(T, S), T##1 S##1
#define ENUM_BINARY_PARAMS_3(T, S) ENUM_BINARY_PARAMS_2(T, S), T##2 S##2
#define ENUM_BINARY_PARAMS_4(T, S) ENUM_BINARY_PARAMS_3(T, S), T##3 S##3
#define ENUM_BINARY_PARAMS_5(T, S) ENUM_BINARY_PARAMS_4(T, S), T##4 S##4
#define ENUM_BINARY_PARAMS_6(T, S) ENUM_BINARY_PARAMS_5(T, S), T##5 S##5
#define ENUM_BINARY_PARAMS_7(T, S) ENUM_BINARY_PARAMS_6(T, S), T##6 S##6
#define ENUM_BINARY_PARAMS_8(T, S) ENUM_BINARY_PARAMS_7(T, S), T##7 S##7
#define ENUM_BINARY_PARAMS_9(T, S) ENUM_BINARY_PARAMS_8(T, S), T##8 S##8
#define ENUM_BINARY_PARAMS_10(T, S) ENUM_BINARY_PARAMS_9(T, S), T##9 S##9

//
// CHAIN_ARG(n) -> .arg(argN)
// CHAIN_ARGS(n) -> .arg(arg0).arg(arg1)...arg(argN-1)
//
#define CHAIN_ARG(n) .arg(arg##n)

#define CHAIN_ARGS_1 CHAIN_ARG(0)
#define CHAIN_ARGS_2 CHAIN_ARGS_1 CHAIN_ARG(1)
#define CHAIN_ARGS_3 CHAIN_ARGS_2 CHAIN_ARG(2)
#define CHAIN_ARGS_4 CHAIN_ARGS_3 CHAIN_ARG(3)
#define CHAIN_ARGS_5 CHAIN_ARGS_4 CHAIN_ARG(4)
#define CHAIN_ARGS_6 CHAIN_ARGS_5 CHAIN_ARG(5)
#define CHAIN_ARGS_7 CHAIN_ARGS_6 CHAIN_ARG(6)
#define CHAIN_ARGS_8 CHAIN_ARGS_7 CHAIN_ARG(7)
#define CHAIN_ARGS_9 CHAIN_ARGS_8 CHAIN_ARG(8)
#define CHAIN_ARGS_10 CHAIN_ARGS_9 CHAIN_ARG(9)

//
// MAKE_LOG_OVERLOAD(n, level) -> defines a template overload of `level` for n
// arguments
//
#define MAKE_LOG_OVERLOAD(n, name, level)                                      \
    template <ENUM_PARAMS_##n(typename T)>                                     \
    inline void name(                                                          \
        const std::string &fmt, ENUM_BINARY_PARAMS_##n(const T, &arg))         \
    {                                                                          \
        write_log(level, Formatter(fmt) CHAIN_ARGS_##n.str());                 \
    }

//
// MAKE_LOG_OVERLOADS(level) -> defines all n-argument overloads of `level` for
// n in [1, 10]
//
#define MAKE_LOG_OVERLOADS(name, level)                                        \
    MAKE_LOG_OVERLOAD(1, name, level)                                          \
    MAKE_LOG_OVERLOAD(2, name, level)                                          \
    MAKE_LOG_OVERLOAD(3, name, level)                                          \
    MAKE_LOG_OVERLOAD(4, name, level)                                          \
    MAKE_LOG_OVERLOAD(5, name, level)                                          \
    MAKE_LOG_OVERLOAD(6, name, level)                                          \
    MAKE_LOG_OVERLOAD(7, name, level)                                          \
    MAKE_LOG_OVERLOAD(8, name, level)                                          \
    MAKE_LOG_OVERLOAD(9, name, level)                                          \
    MAKE_LOG_OVERLOAD(10, name, level)

namespace logger {

namespace levels {

#define LOG_LEVELS                                                             \
    X(DEBUG, debug)                                                            \
    X(INFO, info)                                                              \
    X(WARN, warn)                                                              \
    X(ERROR, error)

enum type {
#define X(name, _) name,
    LOG_LEVELS
#undef X
};

static const char *strings[] = {
#define X(name, _) #name,
    LOG_LEVELS
#undef X
};

static const std::size_t COUNT = sizeof(strings) / sizeof(*strings);

}

class Formatter {
public:
    explicit Formatter(const std::string &fmt)
        : _fmt(fmt)
        , _pos(0)
    {
    }

    template <typename T>
    Formatter &arg(const T &val)
    {
        std::size_t open = _fmt.find('{', _pos);
        if (open != std::string::npos && open + 1 < _fmt.size()
            && _fmt[open + 1] == '}') {
            _out.write(
                _fmt.c_str() + _pos, static_cast<std::streamsize>(open - _pos));
            _out << val;
            _pos = open + 2;
        }
        return *this;
    }

    std::string str()
    {
        _out.write(_fmt.c_str() + _pos,
            static_cast<std::streamsize>(_fmt.size() - _pos));
        return _out.str();
    }

private:
    const std::string &_fmt;
    std::ostringstream _out;
    std::size_t _pos;
};

std::ostream *out_stream = &std::cout;

inline void write_log(levels::type level, const std::string &msg)
{
    char time_buff[30];
    time_t now = time(0);
    struct tm tstruct;
    tstruct = *localtime(&now);
    if (!strftime(time_buff, sizeof(time_buff), "%Y-%m-%d %X", &tstruct))
        return;

    *out_stream << "\x1b[2m" << time_buff << "\x1b[0m ";

    switch (level) {
    case levels::DEBUG:
        *out_stream << "\x1b[0;1;7;95m DEBUG \x1b[0m ";
        break;
    case levels::INFO:
        *out_stream << "\x1b[0;1;7;96m INFO  \x1b[0m ";
        break;
    case levels::WARN:
        *out_stream << "\x1b[0;1;7;93m WARN  \x1b[0m ";
        break;
    case levels::ERROR:
        *out_stream << "\x1b[0;1;7;31m ERROR \x1b[0m ";
        break;
    }

    *out_stream << msg << '\n';
}

#define X(name, func)                                                          \
    inline void func(const std::string &fmt) { write_log(levels::name, fmt); } \
    MAKE_LOG_OVERLOADS(func, levels::name)
LOG_LEVELS
#undef X

}

#undef ENUM_PARAMS_1
#undef ENUM_PARAMS_2
#undef ENUM_PARAMS_3
#undef ENUM_PARAMS_4
#undef ENUM_PARAMS_5
#undef ENUM_PARAMS_6
#undef ENUM_PARAMS_7
#undef ENUM_PARAMS_8
#undef ENUM_PARAMS_9
#undef ENUM_PARAMS_10
#undef ENUM_BINARY_PARAMS_1
#undef ENUM_BINARY_PARAMS_2
#undef ENUM_BINARY_PARAMS_3
#undef ENUM_BINARY_PARAMS_4
#undef ENUM_BINARY_PARAMS_5
#undef ENUM_BINARY_PARAMS_6
#undef ENUM_BINARY_PARAMS_7
#undef ENUM_BINARY_PARAMS_8
#undef ENUM_BINARY_PARAMS_9
#undef ENUM_BINARY_PARAMS_10
#undef CHAIN_ARG
#undef CHAIN_ARGS_1
#undef CHAIN_ARGS_2
#undef CHAIN_ARGS_3
#undef CHAIN_ARGS_4
#undef CHAIN_ARGS_5
#undef CHAIN_ARGS_6
#undef CHAIN_ARGS_7
#undef CHAIN_ARGS_8
#undef CHAIN_ARGS_9
#undef CHAIN_ARGS_10
#undef MAKE_LOG_OVERLOAD
#undef MAKE_LOG_OVERLOADS
#undef LOG_LEVELS
