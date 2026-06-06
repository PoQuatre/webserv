/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cipher_suites.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade <uanglade@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 22:08:34 by uanglade          #+#    #+#             */
/*   Updated: 2026/06/06 02:02:49 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <stdint.h>
#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

namespace ssl {
namespace cipher_suites {

#define CIPHER_SUITES                                                          \
    X(TLS_NULL_WITH_NULL_NULL, 0x00, 0x00, false)                              \
    X(TLS_KRB5_WITH_DES_CBC_SHA, 0x00, 0x1E, false)                            \
    X(TLS_KRB5_WITH_3DES_EDE_CBC_SHA, 0x00, 0x1F, false)                       \
    X(TLS_KRB5_WITH_RC4_128_SHA, 0x00, 0x20, false)                            \
    X(TLS_KRB5_WITH_IDEA_CBC_SHA, 0x00, 0x21, false)                           \
    X(TLS_KRB5_WITH_DES_CBC_MD5, 0x00, 0x22, false)                            \
    X(TLS_KRB5_WITH_3DES_EDE_CBC_MD5, 0x00, 0x23, false)                       \
    X(TLS_KRB5_WITH_RC4_128_MD5, 0x00, 0x24, false)                            \
    X(TLS_KRB5_WITH_IDEA_CBC_MD5, 0x00, 0x25, false)                           \
    X(TLS_KRB5_EXPORT_WITH_DES_CBC_40_SHA, 0x00, 0x26, false)                  \
    X(TLS_KRB5_EXPORT_WITH_RC2_CBC_40_SHA, 0x00, 0x27, false)                  \
    X(TLS_KRB5_EXPORT_WITH_RC4_40_SHA, 0x00, 0x28, false)                      \
    X(TLS_KRB5_EXPORT_WITH_DES_CBC_40_MD5, 0x00, 0x29, false)                  \
    X(TLS_KRB5_EXPORT_WITH_RC2_CBC_40_MD5, 0x00, 0x2A, false)                  \
    X(TLS_KRB5_EXPORT_WITH_RC4_40_MD5, 0x00, 0x2B, false)                      \
    X(TLS_PSK_WITH_NULL_SHA, 0x00, 0x2C, false)                                \
    X(TLS_DH_RSA_WITH_CAMELLIA_256_CBC_SHA, 0x00, 0x86, false)                 \
    X(TLS_DH_anon_WITH_CAMELLIA_256_CBC_SHA, 0x00, 0x89, false)                \
    X(TLS_PSK_WITH_RC4_128_SHA, 0x00, 0x8A, false)                             \
    X(TLS_PSK_WITH_3DES_EDE_CBC_SHA, 0x00, 0x8B, false)                        \
    X(TLS_PSK_WITH_AES_128_CBC_SHA, 0x00, 0x8C, false)                         \
    X(TLS_PSK_WITH_AES_256_CBC_SHA, 0x00, 0x8D, false)                         \
    X(TLS_PSK_WITH_AES_128_GCM_SHA256, 0x00, 0xA8, false)                      \
    X(TLS_PSK_WITH_AES_256_GCM_SHA384, 0x00, 0xA9, false)                      \
    X(TLS_PSK_WITH_AES_128_CBC_SHA256, 0x00, 0xAE, false)                      \
    X(TLS_PSK_WITH_AES_256_CBC_SHA384, 0x00, 0xAF, false)                      \
    X(TLS_PSK_WITH_NULL_SHA256, 0x00, 0xB0, false)                             \
    X(TLS_PSK_WITH_NULL_SHA384, 0x00, 0xB1, false)                             \
    X(TLS_EMPTY_RENEGOTIATION_INFO_SCSV, 0x00, 0xFF, false)                    \
    X(TLS_AES_128_GCM_SHA256, 0x13, 0x01, true)                                \
    X(TLS_AES_256_GCM_SHA384, 0x13, 0x02, true)                                \
    X(TLS_CHACHA20_POLY1305_SHA256, 0x13, 0x03, true)                          \
    X(TLS_AES_128_CCM_SHA256, 0x13, 0x04, false)                               \
    X(TLS_AES_128_CCM_8_SHA256, 0x13, 0x05, false)                             \
    X(TLS_FALLBACK_SCSV, 0x56, 0x00, false)                                    \
    X(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA, 0xC0, 0x04, false)                  \
    X(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA, 0xC0, 0x05, false)                  \
    X(TLS_ECDHE_ECDSA_WITH_NULL_SHA, 0xC0, 0x06, false)                        \
    X(TLS_ECDHE_ECDSA_WITH_RC4_128_SHA, 0xC0, 0x07, false)                     \
    X(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, 0xC0, 0x08, false)                \
    X(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, 0xC0, 0x09, false)                 \
    X(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, 0xC0, 0x0A, false)                 \
    X(TLS_ECDHE_RSA_WITH_NULL_SHA, 0xC0, 0x10, false)                          \
    X(TLS_ECDHE_RSA_WITH_RC4_128_SHA, 0xC0, 0x11, false)                       \
    X(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA, 0xC0, 0x12, false)                  \
    X(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, 0xC0, 0x13, false)                   \
    X(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA, 0xC0, 0x14, false)                   \
    X(TLS_SRP_SHA_WITH_3DES_EDE_CBC_SHA, 0xC0, 0x1A, false)                    \
    X(TLS_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA, 0xC0, 0x1B, false)                \
    X(TLS_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA, 0xC0, 0x1C, false)                \
    X(TLS_SRP_SHA_WITH_AES_128_CBC_SHA, 0xC0, 0x1D, false)                     \
    X(TLS_SRP_SHA_RSA_WITH_AES_128_CBC_SHA, 0xC0, 0x1E, false)                 \
    X(TLS_SRP_SHA_DSS_WITH_AES_128_CBC_SHA, 0xC0, 0x1F, false)                 \
    X(TLS_SRP_SHA_WITH_AES_256_CBC_SHA, 0xC0, 0x20, false)                     \
    X(TLS_SRP_SHA_RSA_WITH_AES_256_CBC_SHA, 0xC0, 0x21, false)                 \
    X(TLS_SRP_SHA_DSS_WITH_AES_256_CBC_SHA, 0xC0, 0x22, false)                 \
    X(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256, 0xC0, 0x23, false)              \
    X(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384, 0xC0, 0x24, false)              \
    X(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, 0xC0, 0x27, false)                \
    X(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384, 0xC0, 0x28, false)                \
    X(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0xC0, 0x2B, false)              \
    X(TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, 0xC0, 0x2C, false)              \
    X(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0xC0, 0x2F, false)                \
    X(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, 0xC0, 0x30, false)                \
    X(TLS_ECDHE_PSK_WITH_RC4_128_SHA, 0xC0, 0x33, false)                       \
    X(TLS_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA, 0xC0, 0x34, false)                  \
    X(TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA, 0xC0, 0x35, false)                   \
    X(TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA, 0xC0, 0x36, false)                   \
    X(TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256, 0xC0, 0x37, false)                \
    X(TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384, 0xC0, 0x38, false)                \
    X(TLS_ECDHE_PSK_WITH_NULL_SHA, 0xC0, 0x39, false)                          \
    X(TLS_ECDHE_PSK_WITH_NULL_SHA256, 0xC0, 0x3A, false)                       \
    X(TLS_ECDHE_PSK_WITH_NULL_SHA384, 0xC0, 0x3B, false)                       \
    X(TLS_ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256, 0xC0, 0x48, false)             \
    X(TLS_ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384, 0xC0, 0x49, false)             \
    X(TLS_ECDHE_RSA_WITH_ARIA_128_CBC_SHA256, 0xC0, 0x4C, false)               \
    X(TLS_ECDHE_RSA_WITH_ARIA_256_CBC_SHA384, 0xC0, 0x4D, false)               \
    X(TLS_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256, 0xC0, 0x5C, false)             \
    X(TLS_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384, 0xC0, 0x5D, false)             \
    X(TLS_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256, 0xC0, 0x60, false)               \
    X(TLS_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384, 0xC0, 0x61, false)               \
    X(TLS_PSK_WITH_ARIA_128_CBC_SHA256, 0xC0, 0x64, false)                     \
    X(TLS_PSK_WITH_ARIA_256_CBC_SHA384, 0xC0, 0x65, false)                     \
    X(TLS_PSK_WITH_ARIA_128_GCM_SHA256, 0xC0, 0x6A, false)                     \
    X(TLS_PSK_WITH_ARIA_256_GCM_SHA384, 0xC0, 0x6B, false)                     \
    X(TLS_ECDHE_PSK_WITH_ARIA_128_CBC_SHA256, 0xC0, 0x70, false)               \
    X(TLS_ECDHE_PSK_WITH_ARIA_256_CBC_SHA384, 0xC0, 0x71, false)               \
    X(TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256, 0xC0, 0x72, false)         \
    X(TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384, 0xC0, 0x73, false)         \
    X(TLS_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256, 0xC0, 0x76, false)           \
    X(TLS_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384, 0xC0, 0x77, false)           \
    X(TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256, 0xC0, 0x86, false)         \
    X(TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384, 0xC0, 0x87, false)         \
    X(TLS_ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256, 0xC0, 0x8A, false)           \
    X(TLS_ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384, 0xC0, 0x8B, false)           \
    X(TLS_PSK_WITH_CAMELLIA_128_GCM_SHA256, 0xC0, 0x8E, false)                 \
    X(TLS_PSK_WITH_CAMELLIA_256_GCM_SHA384, 0xC0, 0x8F, false)                 \
    X(TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256, 0xC0, 0x94, false)                 \
    X(TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384, 0xC0, 0x95, false)                 \
    X(TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256, 0xC0, 0x9A, false)           \
    X(TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384, 0xC0, 0x9B, false)           \
    X(TLS_PSK_WITH_AES_128_CCM, 0xC0, 0xA4, false)                             \
    X(TLS_PSK_WITH_AES_256_CCM, 0xC0, 0xA5, false)                             \
    X(TLS_PSK_WITH_AES_128_CCM_8, 0xC0, 0xA8, false)                           \
    X(TLS_PSK_WITH_AES_256_CCM_8, 0xC0, 0xA9, false)                           \
    X(TLS_ECDHE_ECDSA_WITH_AES_128_CCM, 0xC0, 0xAC, false)                     \
    X(TLS_ECDHE_ECDSA_WITH_AES_256_CCM, 0xC0, 0xAD, false)                     \
    X(TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8, 0xC0, 0xAE, false)                   \
    X(TLS_ECDHE_ECDSA_WITH_AES_256_CCM_8, 0xC0, 0xAF, false)                   \
    X(TLS_ECCPWD_WITH_AES_128_GCM_SHA256, 0xC0, 0xB0, false)                   \
    X(TLS_ECCPWD_WITH_AES_256_GCM_SHA384, 0xC0, 0xB1, false)                   \
    X(TLS_ECCPWD_WITH_AES_128_CCM_SHA256, 0xC0, 0xB2, false)                   \
    X(TLS_ECCPWD_WITH_AES_256_CCM_SHA384, 0xC0, 0xB3, false)                   \
    X(TLS_SHA256_SHA256, 0xC0, 0xB4, false)                                    \
    X(TLS_SHA384_SHA384, 0xC0, 0xB5, false)                                    \
    X(TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0xCC, 0xA8, false)          \
    X(TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 0xCC, 0xA9, false)        \
    X(TLS_PSK_WITH_CHACHA20_POLY1305_SHA256, 0xCC, 0xAB, false)                \
    X(TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256, 0xCC, 0xAC, false)          \
    X(TLS_ECDHE_PSK_WITH_AES_128_GCM_SHA256, 0xD0, 0x01, false)                \
    X(TLS_ECDHE_PSK_WITH_AES_256_GCM_SHA384, 0xD0, 0x02, false)                \
    X(TLS_ECDHE_PSK_WITH_AES_128_CCM_8_SHA256, 0xD0, 0x03, false)              \
    X(TLS_ECDHE_PSK_WITH_AES_128_CCM_SHA256, 0xD0, 0x05, false)

enum __attribute__((__packed__)) type {
#define X(name, value_1, value_2, _) name = (value_2) << 8 | (value_1),
    CIPHER_SUITES
#undef X
};

inline const char *to_string(type cs)
{
    switch (cs) {
#define X(name, _, __, ___)                                                    \
    case name:                                                                 \
        return #name;
        CIPHER_SUITES
#undef X
    }
    return "UNKNOWN";
}

inline bool is_supported(type cs)
{
    switch (cs) {
#define X(name, _, __, support)                                                \
    case name:                                                                 \
        return support;
        CIPHER_SUITES
#undef X
    }
    return "UNKNOWN";
}

}

}
