/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config-parser-def.hpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlaporte <nlaporte@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/22 22:37:13 by nlaporte          #+#    #+#             */
/*   Updated: 2026/06/18 22:05:16 by nlaporte         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <regex.h>
#include <stdint.h>

#include <string>
#include <vector>

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

namespace tokens {

#define TOKENS                                                                 \
    X(BRACE_OPEN, '{')                                                         \
    X(BRACE_CLOSE, '}')                                                        \
    X(END, ';')                                                                \
    X(D_QUOTE, '"')                                                            \
    X(S_QUOTE, '\'')                                                           \
    X(WORD, ' ')

enum type {
#define X(name, _) name,
    TOKENS
#undef X
};
}

namespace location {

#define LOCATION_TYPE                                                          \
    X(STRICT, "=")                                                             \
    X(CASE_SENSITIVE, "~")                                                     \
    X(CASE_INSENSITIVE, "~*")                                                  \
    X(PRIO, "^~")                                                              \
    X(CLASSIC, " ")

enum type {
#define X(name, _) name,
    LOCATION_TYPE
#undef X
};

UNUSED
static const char *strings[] = {
#define X(str, _) #str,
    LOCATION_TYPE
#undef X
};

}

namespace keywords {

// clang-format off
// ENUM STRING MAX_ARG DOC (scope dep) HTTP SERVER LOCATION TYPE_WANTED
#define KEYWORDS                                                                                                                                                                                      \
    X(HTTP, http, 1, "https://nginx.org/en/docs/beginners_guide.html", 0, 0, 0, string_check, std::string)                                                                                            \
    X(LOCATION, location, 3, "https://nginx.org/en/docs/beginners_guide.html", 0, 1, 0, string_check, std::string)                                                                                    \
    X(SERVER, server, 1, "https://nginx.org/en/docs/beginners_guide.html", 1, 0, 0, string_check, std::string)                                                                                        \
    X(SERVER_NAME, server_name, 4, "https://nginx.org/en/docs/http/server_names.html", 0, 1, 0, string_check, std::string)                                                                            \
    X(LISTEN, listen, 2,"https://nginx.org/en/docs/http/ngx_http_core_module.html#listen", 0, 1, 0, string_check, std::string)                                                                        \
    X(ROOT, root, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#root", 1, 1, 1, string_check, std::string)                                                                             \
    X(INDEX, index, 3, "https://docs.nginx.com/nginx/admin-guide/web-server/serving-static-content/", 1, 1, 1, string_check, std::string)                                                             \
    X(CHUNKED_TRANSFER_ENCODING, chunked_transfer_encoding, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#chunked_transfer_encoding", 1, 1, 1, size_check, std::size_t)                \
    X(CLIENT_BODY_BUFFER_SIZE, client_body_buffer_size, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#client_body_buffer_size", 1, 1, 1, size_check, std::size_t)                      \
    X(CLIENT_MAX_BODY_SIZE, client_max_body_size, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#client_max_body_size", 1, 1, 1, size_check, std::size_t)                               \
    X(DISABLE_SYMLINKS, disable_symlinks, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#disable_symlinks", 1, 1, 1, bool_check, bool)                                                  \
    X(ERROR_PAGE, error_page, 7, "https://nginx.org/en/docs/http/ngx_http_core_module.html#error_page", 1, 1, 1, string_check, std::string)                                                           \
    X(SENDFILE, sendfile, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#sendfile", 1, 1, 1, bool_check, bool)                                                                          \
    X(TRY_FILES, try_files, 6, "https://nginx.org/en/docs/http/ngx_http_core_module.html#try_files", 0, 0, 1, string_check, std::string)                                                              \
    X(PROXY_PASS, proxy_pass, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#proxy_pass", 0, 0, 1, string_check, std::string)                                                           \
    X(AUTOINDEX, autoindex, 1, "https://nginx.org/en/docs/http/ngx_http_autoindex_module.html", 1, 1, 1, bool_check, bool)                                                                            \
    X(ABSOLUTE_REDIRECT, absolute_redirect, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#absolute_redirect", 1, 1, 1, bool_check, bool)                                               \
    X(AIO, aio, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#aio", 1, 1, 1, bool_check, bool)                                                                                         \
    X(AIO_WRITE, aio_write, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#aio_write", 1, 1, 1, bool_check, bool)                                                                       \
    X(ALIAS, alias, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#alias", 1, 1, 1, string_check, std::string)                                                                          \
    X(AUTH_DELAY, auth_delay, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#auth_delay", 1, 1, 1, time_check, uint32_t)                                                                \
    X(CLIENT_BODY_IN_FILE_ONLY, client_body_in_file_only , 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#client_body_in_file_only", 1, 1, 1, bool_check, bool)                         \
    X(CLIENT_BODY_IN_SINGLE_BUFFER, client_body_in_single_buffer, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#client_body_in_single_buffer", 1, 1, 1, bool_check, bool)              \
    X(CLIENT_BODY_TEMP_PATH, client_body_temp_path, 3, "https://nginx.org/en/docs/http/ngx_http_core_module.html#client_body_temp_path", 1, 1, 1, string_check, std::string)                          \
    X(CLIENT_BODY_TIMEOUT, client_body_timeout, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#client_body_timeout", 1, 1, 1, time_check, uint32_t)                                     \
    X(CLIENT_HEADER_BUFFER_SIZE, client_header_buffer_size, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#client_header_buffer_size", 1, 1, 0, size_check, std::size_t)                \
    X(CLIENT_HEADER_TIMEOUT, client_header_timeout, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#client_header_timeout", 1, 1, 1, time_check, uint32_t)                               \
    X(CONNECTION_POOL_SIZE, connection_pool_size, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#connection_pool_size", 1, 1, 0, size_check, std::size_t)                               \
    X(DEFAULT_TYPE, default_type, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#default_type", 1, 1, 1, string_check, std::string)                                                     \
    X(DIRECTIO, directio, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#directio", 1, 1, 1, size_check, std::size_t)                                                                   \
    X(DIRECTIO_ALIGNMENT, directio_alignment, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#directio_alignment",1, 1, 1, size_check, std::size_t)                                      \
    X(EARLY_HINTS, early_hints, 4, "https://nginx.org/en/docs/http/ngx_http_core_module.html#early_hints", 1, 1, 1, string_check, std::string)                                                        \
    X(ERROR_LOG_TAG, error_log_tag, 2, "https://nginx.org/en/docs/http/ngx_http_core_module.html#", 1, 1, 1, string_check, std::string)                                                               \
    X(ETAG, etag, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#etag", 1, 1, 1, bool_check, bool)                                                                                      \
    X(IGNORE_INVALID_HEADERS, ignore_invalid_headers, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#ignore_invalid_headers", 1, 1, 0, bool_check, bool)                                \
    X(KEEPALIVE_DISABLE, keepalive_disable, 5, "https://nginx.org/en/docs/http/ngx_http_core_module.html#keepalive_disable", 1, 1, 1, string_check, std::string)                                      \
    X(KEEPALIVE_MIN_TIMEOUT, keepalive_min_timeout, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#keepalive_min_timeout", 1, 1, 1, time_check, uint32_t)                               \
    X(KEEPALIVE_REQUESTS, keepalive_requests, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#keepalive_requests", 1, 1, 1, int_check, uint32_t)                                         \
    X(KEEPALIVE_TIME, keepalive_time, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#keepalive_time", 1, 1, 1, time_check, uint32_t)                                                    \
    X(KEEPALIVE_TIMEOUT, keepalive_timeout, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#keepalive_timeout", 1, 1, 1, time_check, uint32_t)                                           \
    X(LIMIT_RATE, limit_rate, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#limit_rate", 1, 1, 1, size_check, std::size_t)                                                             \
    X(LIMIT_RATE_AFTER, limit_rate_after, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#limit_rate_after", 1, 1, 1, size_check, std::size_t)                                           \
    X(LINGERING_TIME, lingering_time, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#lingering_time", 1, 1, 1, time_check, uint32_t)                                                    \
    X(LOG_NOT_FOUND, log_not_found, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#log_not_found", 1, 1, 1, bool_check, bool)                                                           \
    X(LOG_SUBREQUEST, log_subrequest, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#log_subrequest", 1, 1, 1, bool_check, bool)                                                        \
    X(MAX_HEADERS, max_headers, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#max_headers", 1, 1, 0, int_check, uint32_t)                                                              \
    X(MAX_RANGES, max_ranges, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#max_headers", 1, 1, 1, int_check, uint32_t)                                                                \
    X(MERGE_SLASHES, merge_slashes, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#merge_slashes", 1, 1, 0, bool_check, bool)                                                           \
    X(MSIE_PADDING, msie_padding, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#msie_padding", 1, 1, 1, int_check, uint32_t)                                                           \
    X(MSIE_REFRESH, msie_refresh, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#msie_refresh", 1, 1, 1, bool_check, bool)                                                              \
    X(OPEN_FILE_CACHE_ERRORS, open_file_cache_errors, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#open_file_cache_errors", 1, 1, 1, bool_check, bool)                                \
    X(OPEN_FILE_CACHE_MIN_USES, open_file_cache_min_uses, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#open_file_cache_min_uses", 1, 1, 1, int_check, uint32_t)                       \
    X(OPEN_FILE_CACHE_VALID, open_file_cache_valid, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#open_file_cache_valid", 1, 1, 1, time_check, uint32_t)                               \
    X(PORT_IN_REDIRECT, port_in_redirect, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#port_in_redirect", 1, 1, 1, bool_check, bool)                                                  \
    X(POSTPONE_OUTPUT, postpone_output, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#postpone_output", 1, 1, 1, size_check, std::size_t)                                              \
    X(READ_AHEAD, read_ahead, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#read_ahead", 1, 1, 1, size_check, std::size_t)                                                             \
    X(RECURSIVE_ERROR_PAGES, recursive_error_pages, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#recursive_error_pages", 1, 1, 1, bool_check, bool)                                   \
    X(REQUEST_POOL_SIZE, request_pool_size, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#request_pool_size", 1, 1, 0, size_check, std::size_t)                                        \
    X(RESET_TIMEDOUT_CONNECTION, reset_timedout_connection, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#reset_timedout_connection", 1, 1, 1, bool_check, bool)                       \
    X(RESOLVER_TIMEOUT, resolver_timeout, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#resolver_timeout", 1, 1, 1, time_check, uint32_t)                                              \
    X(SEND_LOWAT, send_lowat, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#send_lowat", 1, 1, 1, size_check, std::size_t)                                                             \
    X(SEND_TIMEOUT, send_timeout, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#send_timeout", 1, 1, 1, time_check, uint32_t)                                                          \
    X(SERVER_NAME_IN_REDIRECT, server_name_in_redirect, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#server_name_in_redirect", 1, 1, 1, bool_check, bool)                             \
    X(SERVER_NAMES_HASH_BUCKET_SIZE, server_names_hash_bucket_size, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#server_names_hash_bucket_size", 1, 0, 0, size_check, std::size_t)    \
    X(SERVER_NAMES_HASH_MAX_SIZE, server_names_hash_max_size, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#server_names_hash_max_size", 1, 0, 0, size_check, std::size_t)             \
    X(SERVER_TOKENS, server_tokens, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#server_tokens", 1, 1, 1, string_check, std::string)                                                  \
    X(SUBREQUEST_OUTPUT_BUFFER_SIZE, subrequest_output_buffer_size, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#subrequest_output_buffer_size", 1, 1, 1, size_check, std::size_t)    \
    X(TCP_NODELAY, tcp_nodelay, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#tcp_nodelay", 1, 1, 1, bool_check, bool)                                                                 \
    X(TCP_NOPUSH, tcp_nopush, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#tcp_nopush", 1, 1, 1, bool_check, bool)                                                                    \
    X(TYPES_HASH_BUCKET_SIZE, types_hash_bucket_size, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#types_hash_bucket_size", 1, 1, 1, size_check, std::size_t)                         \
    X(TYPES_HASH_MAX_SIZE, types_hash_max_size, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#types_hash_max_size", 1, 1, 1, size_check, std::size_t)                                  \
    X(UNDERSCORES_IN_HEADERS, underscores_in_headers, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#underscores_in_headers", 1, 1, 0, bool_check, bool)                                \
    X(VARIABLES_HASH_BUCKET_SIZE, variables_hash_bucket_size, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#variables_hash_bucket_size", 1, 0, 0, size_check, std::size_t)             \
    X(VARIABLES_HASH_MAX_SIZE, variables_hash_max_size, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#variables_hash_max_size", 1, 0, 0, size_check, std::size_t)                      \
    X(LINGERING_CLOSE, lingering_close, 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html#lingering_close", 1, 1, 1, bool_check, bool)                                                     \
    X(PROXY_CONNECT_TIMEOUT, proxy_connect_timeout, 1, "https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_connect_timeout", 1, 1, 1, time_check, uint32_t)                              \
    X(LINGERING_TIMEOUT, lingering_timeout, 1, "https://nginx.org/en/docs/http/ngx_http_proxy_module.html#lingering_timeout", 1, 1, 1, time_check, uint32_t)                                          \
    X(PROXY_READ_TIMEOUT, proxy_read_timeout, 1, "https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_read_timeout", 1, 1, 1, time_check, uint32_t)                                       \
    X(PROXY_SEND_TIMEOUT, proxy_send_timeout, 1, "https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_send_timeout", 1, 1, 1, time_check, uint32_t)                                       \
    X(PROXY_BUFFER_SIZE, proxy_buffer_size, 1, " ", 1, 1, 1, size_check, std::size_t)                                                                                                                 \
    X(OUTPUT_BUFFERS, output_buffers, 1, " ", 1, 1, 1, string_check, std::string)                                                                                                                     \
    X(LIMIT_EXCEPT, limit_except, 8, "https://nginx.org/en/docs/http/ngx_http_core_module.html#limit_except", 0, 0, 1, string_check, std::string)                                                     \
    X_SPECIAL(OPEN_SCOPE, "{", 1, " ", 0, 0, 0, string_check, std::string)                                                                                                                            \
    X_SPECIAL(CLOSE_SCOPE, "}", 1, " ", 0, 0, 0, string_check, std::string)                                                                                                                           \
    X_SPECIAL(UNKNOWN,  "", 1, "https://nginx.org/en/docs/http/ngx_http_core_module.html", 0, 0, 0, string_check, std::string)                                                                        \
    X_SPECIAL(GLOBAL, "  ", 8, "default", 0, 0, 0, string_check, std::string)
// clang-format on
// https://nginx.org/en/docs/http/ngx_http_gzip_module.html
// https://nginx.org/en/docs/http/ngx_http_proxy_module.html#
// NOTE:A CHECK EN DETAILS
// X(if_modified_since , "", ,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#") X(internal,
// "", , "https://nginx.org/en/docs/http/ngx_http_core_module.html#")
// X(large_client_header_buffers, "large_client_header_buffers", 2,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#", 1,1 ,0,
// INT_CHECK) X(open_file_cache , "", ,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#")
// X(output_buffers, "", ,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#") X(resolver,
// "", , "https://nginx.org/en/docs/http/ngx_http_core_module.html#")
// X(satisfy, "satisfy", 1,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#satisfy",1,1,1,
// ???)
// SCOPE
// "")
// X(types, "types", ,
// "https://nginx.org/en/docs/http/ngx_http_core_module.html#types")

enum type {
#define X(name, ...) name,
#define X_SPECIAL(name, ...) name,
    KEYWORDS
#undef X_SPECIAL
#undef X
};
}

struct config_webserv {
#define X(_, name, keyword, __, ___, ____, _____, ______, type)                \
    std::vector<std::string> name;

#define X_SPECIAL(...)

    KEYWORDS

#undef X
#undef X_SPECIAL
};

UNUSED
static const char *strings_keyword[] = {
#define X(str, _, ...) #str,
#define X_SPECIAL(str, _, ...) #str,
    KEYWORDS
#undef X_SPECIAL
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
    location::type location_type;
    regex_t location_regexp;
    std::string key;
    std::vector<std::string> vals;
    std::vector<config_node *> children;
    config_node *parent;
    uint32_t line;
};
