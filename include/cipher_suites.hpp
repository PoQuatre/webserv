/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cipher_suites.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade <uanglade@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 22:08:34 by uanglade          #+#    #+#             */
/*   Updated: 2026/06/04 22:50:56 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

namespace ssl {
namespace cipher_suites {

#define CIPHER_SUITES                                                          \
    X(TLS_NULL_WITH_NULL_NULL, 0x00, 0x00)                                     \
    X(TLS_KRB5_WITH_DES_CBC_SHA, 0x00, 0x1E)                                   \
    X(TLS_KRB5_WITH_3DES_EDE_CBC_SHA, 0x00, 0x1F)                              \
    X(TLS_KRB5_WITH_RC4_128_SHA, 0x00, 0x20)                                   \
    X(TLS_KRB5_WITH_IDEA_CBC_SHA, 0x00, 0x21)                                  \
    X(TLS_KRB5_WITH_DES_CBC_MD5, 0x00, 0x22)                                   \
    X(TLS_KRB5_WITH_3DES_EDE_CBC_MD5, 0x00, 0x23)                              \
    X(TLS_KRB5_WITH_RC4_128_MD5, 0x00, 0x24)                                   \
    X(TLS_KRB5_WITH_IDEA_CBC_MD5, 0x00, 0x25)                                  \
    X(TLS_KRB5_EXPORT_WITH_DES_CBC_40_SHA, 0x00, 0x26)                         \
    X(TLS_KRB5_EXPORT_WITH_RC2_CBC_40_SHA, 0x00, 0x27)                         \
    X(TLS_KRB5_EXPORT_WITH_RC4_40_SHA, 0x00, 0x28)                             \
    X(TLS_KRB5_EXPORT_WITH_DES_CBC_40_MD5, 0x00, 0x29)                         \
    X(TLS_KRB5_EXPORT_WITH_RC2_CBC_40_MD5, 0x00, 0x2A)                         \
    X(TLS_KRB5_EXPORT_WITH_RC4_40_MD5, 0x00, 0x2B)                             \
    X(TLS_PSK_WITH_NULL_SHA, 0x00, 0x2C)                                       \
    X(TLS_DH_RSA_WITH_CAMELLIA_256_CBC_SHA, 0x00, 0x86)                        \
    X(TLS_DH_anon_WITH_CAMELLIA_256_CBC_SHA, 0x00, 0x89)                       \
    X(TLS_PSK_WITH_RC4_128_SHA, 0x00, 0x8A)                                    \
    X(TLS_PSK_WITH_3DES_EDE_CBC_SHA, 0x00, 0x8B)                               \
    X(TLS_PSK_WITH_AES_128_CBC_SHA, 0x00, 0x8C)                                \
    X(TLS_PSK_WITH_AES_256_CBC_SHA, 0x00, 0x8D)                                \
    X(TLS_PSK_WITH_AES_128_GCM_SHA256, 0x00, 0xA8)                             \
    X(TLS_PSK_WITH_AES_256_GCM_SHA384, 0x00, 0xA9)                             \
    X(TLS_PSK_WITH_AES_128_CBC_SHA256, 0x00, 0xAE)                             \
    X(TLS_PSK_WITH_AES_256_CBC_SHA384, 0x00, 0xAF)                             \
    X(TLS_PSK_WITH_NULL_SHA256, 0x00, 0xB0)                                    \
    X(TLS_PSK_WITH_NULL_SHA384, 0x00, 0xB1)                                    \
    X(TLS_EMPTY_RENEGOTIATION_INFO_SCSV, 0x00, 0xFF)                           \
    X(TLS_AES_128_GCM_SHA256, 0x13, 0x01)                                      \
    X(TLS_AES_256_GCM_SHA384, 0x13, 0x02)                                      \
    X(TLS_CHACHA20_POLY1305_SHA256, 0x13, 0x03)                                \
    X(TLS_AES_128_CCM_SHA256, 0x13, 0x04)                                      \
    X(TLS_AES_128_CCM_8_SHA256, 0x13, 0x05)                                    \
    X(TLS_FALLBACK_SCSV, 0x56, 0x00)                                           \
    X(TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA, 0xC0, 0x04)                         \
    X(TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA, 0xC0, 0x05)                         \
    X(TLS_ECDHE_ECDSA_WITH_NULL_SHA, 0xC0, 0x06)                               \
    X(TLS_ECDHE_ECDSA_WITH_RC4_128_SHA, 0xC0, 0x07)                            \
    X(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, 0xC0, 0x08)                       \
    X(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, 0xC0, 0x09)                        \
    X(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, 0xC0, 0x0A)                        \
    X(TLS_ECDHE_RSA_WITH_NULL_SHA, 0xC0, 0x10)                                 \
    X(TLS_ECDHE_RSA_WITH_RC4_128_SHA, 0xC0, 0x11)                              \
    X(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA, 0xC0, 0x12)                         \
    X(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, 0xC0, 0x13)                          \
    X(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA, 0xC0, 0x14)                          \
    X(TLS_SRP_SHA_WITH_3DES_EDE_CBC_SHA, 0xC0, 0x1A)                           \
    X(TLS_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA, 0xC0, 0x1B)                       \
    X(TLS_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA, 0xC0, 0x1C)                       \
    X(TLS_SRP_SHA_WITH_AES_128_CBC_SHA, 0xC0, 0x1D)                            \
    X(TLS_SRP_SHA_RSA_WITH_AES_128_CBC_SHA, 0xC0, 0x1E)                        \
    X(TLS_SRP_SHA_DSS_WITH_AES_128_CBC_SHA, 0xC0, 0x1F)                        \
    X(TLS_SRP_SHA_WITH_AES_256_CBC_SHA, 0xC0, 0x20)                            \
    X(TLS_SRP_SHA_RSA_WITH_AES_256_CBC_SHA, 0xC0, 0x21)                        \
    X(TLS_SRP_SHA_DSS_WITH_AES_256_CBC_SHA, 0xC0, 0x22)                        \
    X(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256, 0xC0, 0x23)                     \
    X(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384, 0xC0, 0x24)                     \
    X(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, 0xC0, 0x27)                       \
    X(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384, 0xC0, 0x28)                       \
    X(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0xC0, 0x2B)                     \
    X(TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, 0xC0, 0x2C)                     \
    X(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0xC0, 0x2F)                       \
    X(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, 0xC0, 0x30)                       \
    X(TLS_ECDHE_PSK_WITH_RC4_128_SHA, 0xC0, 0x33)                              \
    X(TLS_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA, 0xC0, 0x34)                         \
    X(TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA, 0xC0, 0x35)                          \
    X(TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA, 0xC0, 0x36)                          \
    X(TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256, 0xC0, 0x37)                       \
    X(TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384, 0xC0, 0x38)                       \
    X(TLS_ECDHE_PSK_WITH_NULL_SHA, 0xC0, 0x39)                                 \
    X(TLS_ECDHE_PSK_WITH_NULL_SHA256, 0xC0, 0x3A)                              \
    X(TLS_ECDHE_PSK_WITH_NULL_SHA384, 0xC0, 0x3B)                              \
    X(TLS_ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256, 0xC0, 0x48)                    \
    X(TLS_ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384, 0xC0, 0x49)                    \
    X(TLS_ECDHE_RSA_WITH_ARIA_128_CBC_SHA256, 0xC0, 0x4C)                      \
    X(TLS_ECDHE_RSA_WITH_ARIA_256_CBC_SHA384, 0xC0, 0x4D)                      \
    X(TLS_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256, 0xC0, 0x5C)                    \
    X(TLS_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384, 0xC0, 0x5D)                    \
    X(TLS_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256, 0xC0, 0x60)                      \
    X(TLS_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384, 0xC0, 0x61)                      \
    X(TLS_PSK_WITH_ARIA_128_CBC_SHA256, 0xC0, 0x64)                            \
    X(TLS_PSK_WITH_ARIA_256_CBC_SHA384, 0xC0, 0x65)                            \
    X(TLS_PSK_WITH_ARIA_128_GCM_SHA256, 0xC0, 0x6A)                            \
    X(TLS_PSK_WITH_ARIA_256_GCM_SHA384, 0xC0, 0x6B)                            \
    X(TLS_ECDHE_PSK_WITH_ARIA_128_CBC_SHA256, 0xC0, 0x70)                      \
    X(TLS_ECDHE_PSK_WITH_ARIA_256_CBC_SHA384, 0xC0, 0x71)                      \
    X(TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256, 0xC0, 0x72)                \
    X(TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384, 0xC0, 0x73)                \
    X(TLS_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256, 0xC0, 0x76)                  \
    X(TLS_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384, 0xC0, 0x77)                  \
    X(TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256, 0xC0, 0x86)                \
    X(TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384, 0xC0, 0x87)                \
    X(TLS_ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256, 0xC0, 0x8A)                  \
    X(TLS_ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384, 0xC0, 0x8B)                  \
    X(TLS_PSK_WITH_CAMELLIA_128_GCM_SHA256, 0xC0, 0x8E)                        \
    X(TLS_PSK_WITH_CAMELLIA_256_GCM_SHA384, 0xC0, 0x8F)                        \
    X(TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256, 0xC0, 0x94)                        \
    X(TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384, 0xC0, 0x95)                        \
    X(TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256, 0xC0, 0x9A)                  \
    X(TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384, 0xC0, 0x9B)                  \
    X(TLS_PSK_WITH_AES_128_CCM, 0xC0, 0xA4)                                    \
    X(TLS_PSK_WITH_AES_256_CCM, 0xC0, 0xA5)                                    \
    X(TLS_PSK_WITH_AES_128_CCM_8, 0xC0, 0xA8)                                  \
    X(TLS_PSK_WITH_AES_256_CCM_8, 0xC0, 0xA9)                                  \
    X(TLS_ECDHE_ECDSA_WITH_AES_128_CCM, 0xC0, 0xAC)                            \
    X(TLS_ECDHE_ECDSA_WITH_AES_256_CCM, 0xC0, 0xAD)                            \
    X(TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8, 0xC0, 0xAE)                          \
    X(TLS_ECDHE_ECDSA_WITH_AES_256_CCM_8, 0xC0, 0xAF)                          \
    X(TLS_ECCPWD_WITH_AES_128_GCM_SHA256, 0xC0, 0xB0)                          \
    X(TLS_ECCPWD_WITH_AES_256_GCM_SHA384, 0xC0, 0xB1)                          \
    X(TLS_ECCPWD_WITH_AES_128_CCM_SHA256, 0xC0, 0xB2)                          \
    X(TLS_ECCPWD_WITH_AES_256_CCM_SHA384, 0xC0, 0xB3)                          \
    X(TLS_SHA256_SHA256, 0xC0, 0xB4)                                           \
    X(TLS_SHA384_SHA384, 0xC0, 0xB5)                                           \
    X(TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0xCC, 0xA8)                 \
    X(TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 0xCC, 0xA9)               \
    X(TLS_PSK_WITH_CHACHA20_POLY1305_SHA256, 0xCC, 0xAB)                       \
    X(TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256, 0xCC, 0xAC)                 \
    X(TLS_ECDHE_PSK_WITH_AES_128_GCM_SHA256, 0xD0, 0x01)                       \
    X(TLS_ECDHE_PSK_WITH_AES_256_GCM_SHA384, 0xD0, 0x02)                       \
    X(TLS_ECDHE_PSK_WITH_AES_128_CCM_8_SHA256, 0xD0, 0x03)                     \
    X(TLS_ECDHE_PSK_WITH_AES_128_CCM_SHA256, 0xD0, 0x05)

enum type {
#define X(name, value_1, value_2) name = (value_1) << 8 | (value_2),
    CIPHER_SUITES
#undef X
};

const char *to_string(type cs)
{
    switch (cs) {
#define X(name, _, __)                                                         \
    case name:                                                                 \
        return #name;
        CIPHER_SUITES
#undef X
    }
    return "UNKNOWN";
}

}

}
