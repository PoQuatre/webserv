/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 07:53:07 by nlaporte          #+#    #+#             */
<<<<<<< HEAD
/*   Updated: 2026/05/20 09:39:54 by nlaporte         ###   ########.fr       */
=======
/*   Updated: 2026/05/20 09:51:28 by nlaporte         ###   ########.fr       */
>>>>>>> a3e3b92 (refractor: add .hpp)
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "webserv.hpp"

#define CALL(macro, val) macro(val);

#define STRING_CHECK(val)                                                      \
    do {                                                                       \
        return true;                                                           \
    } while (0)

#define BOOL_CHECK(val)                                                        \
    do {                                                                       \
        return ((val) == "on" || (val) == "off");                              \
    } while (0)

#define INT_CHECK(val)                                                         \
    do {                                                                       \
        for (int i = 0; (val)[i]; i++) {                                       \
            if (!std::isdigit((val)[i]))                                       \
                return false;                                                  \
        }                                                                      \
        return true;                                                           \
    } while (0)

#define TIME_CHECK(val)                                                        \
    bool valid = false;                                                        \
    uint32_t i = 0;                                                            \
    do {                                                                       \
        for ((val)[i]; i++;) {                                                 \
            if (!std::isdigit((val)[i]))                                       \
                break;                                                         \
        }                                                                      \
        if (i + 1 < (val).length() && (val)[i] == 'm' && ((val)[i]) == 's') {  \
            valid = true;                                                      \
            i += 2;                                                            \
        } else {                                                               \
            switch ((val)[i]) {                                                \
            case 's':                                                          \
            case 'm':                                                          \
            case 'h':                                                          \
            case 'd':                                                          \
            case 'w':                                                          \
            case 'M':                                                          \
            case 'y':                                                          \
                i++;                                                           \
                valid = true;                                                  \
                break;                                                         \
            case 0:                                                            \
                return valid;                                                  \
            default:                                                           \
                return false;                                                  \
            }                                                                  \
        }                                                                      \
        if (i == (val).length())                                                 \
            return valid;                                                      \
    } while (0)

#define SIZE_CHECK(val)                                                        \
    do {                                                                       \
        uint32_t i;                                                            \
        for (i = 0; (val)[i]; i++) {                                           \
            if (!std::isdigit((val)[i]))                                       \
                break;                                                         \
        }                                                                      \
        if (!(val)[i])                                                         \
            return true;                                                       \
        if (((val)[i] == 'k' || (val)[i] == 'K') && (i + 1) == ((val).size())) \
            return true;                                                       \
        if (((val)[i] == 'm' || (val)[i] == 'M') && (i + 1) == ((val).size())) \
            return true;                                                       \
        if (((val)[i] == 'g' || (val)[i] == 'G')                               \
            && (i + 1) == ((val).length()))                                    \
            return true;                                                       \
        return (std::isdigit((val)[i]));                                       \
    } while (0)

namespace tokens {

#define TOKENS                                                                 \
    X(BRACE_OPEN, '{')                                                         \
    X(BRACE_CLOSE, '}')                                                        \
    X(WORD, ' ')                                                               \
    X(END, ';')                                                                \
    X(EQUAL, '=')                                                              \
    X(COM, '#')

enum type {
#define X(name, _) name,
    TOKENS
#undef X
};
}

namespace keywords {

// clang-format off
// ENUM STRING MAX_ARG DOC (scope dep) HTTP SERVER LOCATION TYPE_WANTED
#define KEYWORDS                                                               \
X(HTTP, "http", 1, "https://nginx.org/en/docs/beginners_guide.html", 0, 0, 0, STRING_CHECK) \
    X(LOCATION, "location", 3, "https://nginx.org/en/docs/beginners_guide.html", 0, 1, 0, STRING_CHECK) \
    X(SERVER, "server", 1, "https://nginx.org/en/docs/beginners_guide.html", 1, 0, 0, STRING_CHECK) \
    X(OPEN_SCOPE, "{", 1, " ", 0, 0, 0, STRING_CHECK)                          \
    X(CLOSE_SCOPE, "}", 1, " ", 0, 0, 0, STRING_CHECK)                         \
    X(SERVER_NAME, "server_name", 4,                                           \
        "https://nginx.org/en/docs/http/server_names.html", 0, 1, 0,           \
        STRING_CHECK)                                                          \
    X(LISTEN, "listen", 2,                                                     \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#listen", 0,  \
        1, 0, STRING_CHECK)                                                    \
    X(ROOT, "root", 1,                                                         \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#root", 1, 1, \
        1, STRING_CHECK)                                                       \
    X(INDEX, "index", 3,                                                       \
        "https://docs.nginx.com/nginx/admin-guide/web-server/"                 \
        "serving-static-content/",                                             \
        1, 1, 1, STRING_CHECK)                                                 \
    X(CHUNKED_TRANSFER_ENCODING, "chunked_transfer_encoding", 1,               \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#chunked_transfer_encoding",                 \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(CLIENT_BODY_BUFFER_SIZE, "client_body_buffer_size", 1,                   \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#client_body_buffer_size",                   \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(CLIENT_MAX_BODY_SIZE, "client_max_body_size", 1,                         \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#client_max_body_size",                      \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(DISABLE_SYMLINKS, "disable_symlinks", 1,                                 \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#disable_symlinks",                          \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(ERROR_PAGE, "error_page", 7,                                             \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#error_page", \
        1, 1, 1, STRING_CHECK)                                                 \
    X(SENDFILE, "sendfile", 1,                                                 \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#sendfile",   \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(TRY_FILES, "try_files", 6,                                               \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#try_files",  \
        0, 0, 1, STRING_CHECK)                                                 \
    X(PROXY_PASS, "proxy_pass", 1,                                             \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#proxy_pass", \
        0, 0, 1, STRING_CHECK)                                                 \
    X(AUTOINDEX, "autoindex", 1,                                               \
        "https://nginx.org/en/docs/http/ngx_http_autoindex_module.html", 1, 1, \
        1, BOOL_CHECK)                                                         \
    X(UNKNOWN, " ", 1,                                                         \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html", 0, 0, 0,   \
        STRING_CHECK)                                                          \
    X(ABSOLUTE_REDIRECT, "absolute_redirect", 1,                               \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#absolute_redirect",                         \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(AIO, "aio", 1,                                                           \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#aio", 1, 1,  \
        1, BOOL_CHECK)                                                         \
    X(AIO_WRITE, "aio_write", 1,                                               \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#aio_write",  \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(ALIAS, "alias", 1,                                                       \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#alias", 1,   \
        1, 1, STRING_CHECK)                                                    \
    X(AUTH_DELAY, "auth_delay", 1,                                             \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#auth_delay", \
        1, 1, 1, TIME_CHECK)                                                   \
    X(CLIENT_BODY_IN_FILE_ONLY, "client_body_in_file_only ", 1,                \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#client_body_in_file_only",                  \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(CLIENT_BODY_IN_SINGLE_BUFFER, "client_body_in_single_buffer", 1,         \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#client_body_in_single_buffer",              \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(CLIENT_BODY_TEMP_PATH, "client_body_temp_path", 3,                       \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#client_body_temp_path",                     \
        1, 1, 1, STRING_CHECK)                                                 \
    X(CLIENT_BODY_TIMEOUT, "client_body_timeout", 1,                           \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#client_body_timeout",                       \
        1, 1, 1, TIME_CHECK)                                                   \
    X(CLIENT_HEADER_BUFFER_SIZE, "client_header_buffer_size", 1,               \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#client_header_buffer_size",                 \
        1, 1, 0, SIZE_CHECK)                                                   \
    X(CLIENT_HEADER_TIMEOUT, "client_header_timeout", 1,                       \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#client_header_timeout",                     \
        1, 1, 1, TIME_CHECK)                                                   \
    X(CONNECTION_POOL_SIZE, "connection_pool_size", 1,                         \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#connection_pool_size",                      \
        1, 1, 0, SIZE_CHECK)                                                   \
    X(DEFAULT_TYPE, "default_type", 1,                                         \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#default_type",                              \
        1, 1, 1, STRING_CHECK)                                                 \
    X(DIRECTIO, "directio", 1,                                                 \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#directio",   \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(DIRECTIO_ALIGNMENT, "directio_alignment", 1,                             \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#directio_alignment",                        \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(EARLY_HINTS, "early_hints", 4,                                           \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#early_hints",                               \
        1, 1, 1, STRING_CHECK)                                                 \
    X(ERROR_LOG_TAG, "error_log_tag", 2,                                       \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#", 1, 1, 1,  \
        STRING_CHECK)                                                          \
    X(ETAG, "etag", 1,                                                         \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#etag", 1, 1, \
        1, BOOL_CHECK)                                                         \
    X(IGNORE_INVALID_HEADERS, "ignore_invalid_headers", 1,                     \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#ignore_invalid_headers",                    \
        1, 1, 0, BOOL_CHECK)                                                   \
    X(KEEPALIVE_DISABLE, "keepalive_disable", 5,                               \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#keepalive_disable",                         \
        1, 1, 1, STRING_CHECK)                                                 \
    X(KEEPALIVE_MIN_TIMEOUT, "keepalive_min_timeout", 1,                       \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#keepalive_min_timeout",                     \
        1, 1, 1, TIME_CHECK)                                                   \
    X(KEEPALIVE_REQUESTS, "keepalive_requests", 1,                             \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#keepalive_requests",                        \
        1, 1, 1, INT_CHECK)                                                    \
    X(KEEPALIVE_TIME, "keepalive_time", 1,                                     \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#keepalive_time",                            \
        1, 1, 1, TIME_CHECK)                                                   \
    X(KEEPALIVE_TIMEOUT, "keepalive_timeout", 1,                               \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#keepalive_timeout",                         \
        1, 1, 1, TIME_CHECK)                                                   \
    X(LIMIT_RATE, "limit_rate", 1,                                             \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#limit_rate", \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(LIMIT_RATE_AFTER, "limit_rate_after", 1,                                 \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#limit_rate_after",                          \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(LINGERING_TIME, "lingering_time", 1,                                     \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#lingering_time",                            \
        1, 1, 1, TIME_CHECK)                                                   \
    X(LOG_NOT_FOUND, "log_not_found", 1,                                       \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#log_not_found",                             \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(LOG_SUBREQUEST, "log_subrequest", 1,                                     \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#log_subrequest",                            \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(MAX_HEADERS, "max_headers", 1,                                           \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#max_headers",                               \
        1, 1, 0, INT_CHECK)                                                    \
    X(MAX_RANGES, "max_ranges", 1,                                             \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#max_headers",                               \
        1, 1, 1, INT_CHECK)                                                    \
    X(MERGE_SLASHES, "merge_slashes", 1,                                       \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#merge_slashes",                             \
        1, 1, 0, BOOL_CHECK)                                                   \
    X(MSIE_PADDING, "msie_padding", 1,                                         \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#msie_padding",                              \
        1, 1, 1, INT_CHECK)                                                    \
    X(MSIE_REFRESH, "msie_refresh", 1,                                         \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#msie_refresh",                              \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(OPEN_FILE_CACHE_ERRORS, "open_file_cache_errors", 1,                     \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#open_file_cache_errors",                    \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(OPEN_FILE_CACHE_MIN_USES, "open_file_cache_min_uses", 1,                 \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#open_file_cache_min_uses",                  \
        1, 1, 1, INT_CHECK)                                                    \
    X(OPEN_FILE_CACHE_VALID, "open_file_cache_valid", 1,                       \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#open_file_cache_valid",                     \
        1, 1, 1, TIME_CHECK)                                                   \
    X(PORT_IN_REDIRECT, "port_in_redirect", 1,                                 \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#port_in_redirect",                          \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(POSTPONE_OUTPUT, "postpone_output", 1,                                   \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#postpone_output",                           \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(READ_AHEAD, "read_ahead", 1,                                             \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#read_ahead", \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(RECURSIVE_ERROR_PAGES, "recursive_error_pages", 1,                       \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#recursive_error_pages",                     \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(REQUEST_POOL_SIZE, "request_pool_size", 1,                               \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#request_pool_size",                         \
        1, 1, 0, SIZE_CHECK)                                                   \
    X(RESET_TIMEDOUT_CONNECTION, "reset_timedout_connection", 1,               \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#reset_timedout_connection",                 \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(RESOLVER_TIMEOUT, "resolver_timeout", 1,                                 \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#resolver_timeout",                          \
        1, 1, 1, TIME_CHECK)                                                   \
    X(SEND_LOWAT, "send_lowat", 1,                                             \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#send_lowat", \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(SEND_TIMEOUT, "send_timeout", 1,                                         \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#send_timeout",                              \
        1, 1, 1, TIME_CHECK)                                                   \
    X(SERVER_NAME_IN_REDIRECT, "server_name_in_redirect", 1,                   \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#server_name_in_redirect",                   \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(SERVER_NAMES_HASH_BUCKET_SIZE, "server_names_hash_bucket_size", 1,       \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#server_names_hash_bucket_size",             \
        1, 0, 0, SIZE_CHECK)                                                   \
    X(SERVER_NAMES_HASH_MAX_SIZE, "server_names_hash_max_size", 1,             \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#server_names_hash_max_size",                \
        1, 0, 0, SIZE_CHECK)                                                   \
    X(SERVER_TOKENS, "server_tokens", 1,                                       \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#server_tokens",                             \
        1, 1, 1, STRING_CHECK)                                                 \
    X(SUBREQUEST_OUTPUT_BUFFER_SIZE, "subrequest_output_buffer_size", 1,       \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#subrequest_output_buffer_size",             \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(TCP_NODELAY, "tcp_nodelay", 1,                                           \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#tcp_nodelay",                               \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(TCP_NOPUSH, "tcp_nopush", 1,                                             \
        "https://nginx.org/en/docs/http/ngx_http_core_module.html#tcp_nopush", \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(TYPES_HASH_BUCKET_SIZE, "types_hash_bucket_size", 1,                     \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#types_hash_bucket_size",                    \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(TYPES_HASH_MAX_SIZE, "types_hash_max_size", 1,                           \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#types_hash_max_size",                       \
        1, 1, 1, SIZE_CHECK)                                                   \
    X(UNDERSCORES_IN_HEADERS, "underscores_in_headers", 1,                     \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#underscores_in_headers",                    \
        1, 1, 0, BOOL_CHECK)                                                   \
    X(VARIABLES_HASH_BUCKET_SIZE, "variables_hash_bucket_size", 1,             \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#variables_hash_bucket_size",                \
        1, 0, 0, SIZE_CHECK)                                                   \
    X(VARIABLES_HASH_MAX_SIZE, "variables_hash_max_size", 1,                   \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#variables_hash_max_size",                   \
        1, 0, 0, SIZE_CHECK)                                                   \
    X(LINGERING_CLOSE, "lingering_close", 1,                                   \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_core_module.html#lingering_close",                           \
        1, 1, 1, BOOL_CHECK)                                                   \
    X(PROXY_CONNECT_TIMEOUT, "proxy_connect_timeout", 1,                       \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_proxy_module.html#proxy_connect_timeout",                    \
        1, 1, 1, TIME_CHECK)                                                   \
    X(LINGERING_TIMEOUT, "lingering_timeout", 1,                               \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_proxy_module.html#lingering_timeout",                        \
        1, 1, 1, TIME_CHECK)                                                   \
    X(PROXY_READ_TIMEOUT, "proxy_read_timeout", 1,                             \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_proxy_module.html#proxy_read_timeout",                       \
        1, 1, 1, TIME_CHECK)                                                   \
    X(PROXY_SEND_TIMEOUT, "proxy_send_timeout", 1,                             \
        "https://nginx.org/en/docs/http/"                                      \
        "ngx_http_proxy_module.html#proxy_send_timeout",                       \
        1, 1, 1, TIME_CHECK)                                                   \
    X(PROXY_BUFFER_SIZE, "proxy_buffer_size", 1, " ", 1, 1, 1, SIZE_CHECK)     \
    X(OUTPUT_BUFFERS, "output_buffers", 1, " ", 1, 1, 1, STRING_CHECK)
// clang-format on

// https://nginx.org/en/docs/http/ngx_http_gzip_module.html
// https://nginx.org/en/docs/http/ngx_http_proxy_module.html#
// NOTE:A CHECK EN DETAILS
// X(if_modified_since , "", ,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#") X(internal, "",
// , "https://nginx.org/en/docs/http/ngx_http_core_module.html#")
// X(large_client_header_buffers, "large_client_header_buffers", 2,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#", 1,1 ,0,
// INT_CHECK) X(open_file_cache , "", ,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#")
// X(output_buffers, "", ,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#") X(resolver, "",
// , "https://nginx.org/en/docs/http/ngx_http_core_module.html#") X(satisfy,
// "satisfy", 1,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#satisfy",1,1,1,
// ???)
// SCOPE
// X(limit_except, "limit_except", ,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#limit_except")
// X(types, "types", ,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#types")

enum type {
#define X(name, _, __, ___, ____, _____, ______, _______) name,
    KEYWORDS
#undef X
};
}

const char *strings[] = {
#define X(_, __, ___, text, ____, _____, ______, _______) #text,
    KEYWORDS
#undef X
};

const char *stringss[] = {
#define X(_, text, ___, __, ____, _____, ______, _______) #text,
    KEYWORDS
#undef X
};

enum node_type { ROOT, NODE, LEAF };

struct config_token {
    std::string value;
    tokens::type type;
    keywords::type keyword;
    bool alive;
    uint32_t size;
    uint32_t line;
};

struct config_node {
    node_type type;
    keywords::type keyword;
    std::string key;
    std::vector<std::string> vals;
    std::vector<config_node *> children;
    config_node *parent;
    bool strict;
    uint32_t line;
};

struct config_webserv {
    std::string root_path;
    std::string error_page;
    std::string index;
    std::size_t client_max_body_size;
    std::size_t client_max_buffer_size;
    bool chunked_transfer_encoding;
    bool autoindex;
};
