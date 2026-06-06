/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Ssl.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade </var/spool/mail/uanglade>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 06:30:19 by uanglade          #+#    #+#             */
/*   Updated: 2026/06/06 18:05:06 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Ssl.hpp"

#include <sys/socket.h>
#include <sys/types.h>

#include <cstring>

#include "logger.hpp"

#ifndef RECV_CHUNK
#define RECV_CHUNK 4096
#endif

namespace ssl {

namespace {

handshake_protocol::extensions::Extenion convert_to_extension(
    const uint8_t *buf)
{
    handshake_protocol::extensions::Extenion ret;

    ret.type = static_cast<handshake_protocol::extensions::ExtensionType::type>(
        buf[0] << 8 | buf[1]);
    buf += 2;
    ret.length = buf[0] << 8 | buf[1];
    buf += 2;
    ret.extenion_data = buf;

    return ret;
}

handshake_protocol::extensions::SupportedGroups convert_to_supported_groups(
    const handshake_protocol::extensions::Extenion &ext)
{
    handshake_protocol::extensions::SupportedGroups ret;
    const uint8_t *buf = ext.extenion_data;

    ret.length = buf[0] << 8 | buf[1];
    ret.count = ret.length / 2;
    buf += 2;
    for (int i = 0; i < ret.count; i++) {
        handshake_protocol::extensions::NamedGroup::type group;

        group = static_cast<handshake_protocol::extensions::NamedGroup::type>(
            buf[0] << 8 | buf[1]);
        buf += 2;
        ret.groups.push_back(group);
    }
    return ret;
}

handshake_protocol::extensions::KeyShare convert_to_key_share(
    const handshake_protocol::extensions::Extenion &ext)
{
    handshake_protocol::extensions::KeyShare ret;
    const uint8_t *buf = ext.extenion_data;

    ret.length = buf[0] << 8 | buf[1];
    buf += 2;

    int n = ret.length;
    while (n > 0) {
        handshake_protocol::extensions::KeyShareEntry key;

        key.group
            = static_cast<handshake_protocol::extensions::NamedGroup::type>(
                buf[0] << 8 | buf[1]);
        buf += 2;
        key.length = buf[0] << 8 | buf[1];
        buf += 2;
        key.key_exchange = buf;

        ret.client_shares.push_back(key);

        n -= key.length + 4;
        buf += key.length + 4;
    }

    return ret;
}

handshake_protocol::ClientHello convert_to_client_hello(const uint8_t *buf)
{
    handshake_protocol::ClientHello ret;

    ret.type = static_cast<handshake_protocol::HandshakeType::type>(*buf++);
    ret.length[0] = *buf++;
    ret.length[1] = *buf++;
    ret.length[2] = *buf++;
    ret.version.major = *buf++;
    ret.version.minor = *buf++;
    for (int i = 0; i < 32; ++i) {
        ret.random[i] = *buf++;
    }
    ret.session_id_length = *buf++;
    ret.session_id = buf;
    buf += ret.session_id_length;
    ret.cipher_suites_length = buf[0] << 8 | buf[1];
    buf += 2;
    ret.cipher_suites = (cipher_suites::type *)buf;
    ret.cipher_suites_count = ret.cipher_suites_length / 2;
    buf += ret.cipher_suites_length;
    ret.legacy_compression_methods_length = *buf++;
    ret.legacy_compression_methods = buf;
    buf += ret.legacy_compression_methods_length;
    ret.extensions_length = buf[0] << 8 | buf[1];
    buf += 2;
    int i = ret.extensions_length;
    int j = 0;
    while (i > 0) {
        ret.extensions.push_back(convert_to_extension(buf));
        buf += ret.extensions[j].length + 4;
        i -= ret.extensions[j].length + 4;
        j++;
        ret.extensions_count++;
    }

    return ret;
}

bool check_supported_versions(
    const handshake_protocol::extensions::Extenion &ext)
{
    const uint8_t *buf = ext.extenion_data;

    uint8_t length = *buf++;
    for (int i = 0; i < length; i += 2) {
        versions::type_int version;
        version.major = *buf++;
        version.minor = *buf++;
        if (version.major == 3 && version.minor == 4) {
            return true;
        }
    }

    return false;
}

}

Ssl::SslError Ssl::check_client_hello(
    const handshake_protocol::ClientHello &msg)
{

    for (int i = 0; i < msg.cipher_suites_count; ++i) {
        if (cipher_suites::is_supported(msg.cipher_suites[i])) {
            _chosen_cipher = msg.cipher_suites[i];
        }
    }
    if (_chosen_cipher == cipher_suites::TLS_NULL_WITH_NULL_NULL) {
        L_ERROR(
            "No supported cipher in cipher suites provided by client {}", _fd);
        return ERROR_FATAL;
    }

    handshake_protocol::extensions::KeyShare key_share;
    handshake_protocol::extensions::SupportedGroups supported_groups;

    for (int i = 0; i < msg.extensions_count; i++) {
        handshake_protocol::extensions::Extenion ext = msg.extensions[i];

        switch (ext.type) {
        case handshake_protocol::extensions::ExtensionType::
            EXTENSION_TYPE_key_share:
            key_share = convert_to_key_share(ext);
            break;
        case handshake_protocol::extensions::ExtensionType::
            EXTENSION_TYPE_supported_groups:
            supported_groups = convert_to_supported_groups(ext);
            break;
        case handshake_protocol::extensions::ExtensionType::
            EXTENSION_TYPE_supported_versions:
            if (!check_supported_versions(ext)) {
                L_TRACE("Client hello does not support TLS 1.3");
                return ERROR_FATAL;
            }
            break;
        default:
            break;
        }
    }

    for (int i = 0; i < supported_groups.count; i++) {
        if (handshake_protocol::extensions::NamedGroup::is_supported(
                supported_groups.groups[i])) {
            for (size_t j = 0; j < key_share.client_shares.size(); j++) {
                if (key_share.client_shares[j].group
                    == supported_groups.groups[i]) {
                    _chosen_group = supported_groups.groups[i];
                    _chose_key_share = key_share.client_shares[j];
                }
            }
        }
    }

    if (_chosen_group
        == handshake_protocol::extensions::NamedGroup::NAMED_GROUP_null) {
        L_ERROR("No supported group in supported groups extension provided by "
                "client {}",
            _fd);
        return ERROR_FATAL;
    }

    return ERROR_OK;
}

Ssl::Ssl(int32_t fd)
    : _fd(fd)
    , _state(STATE_NONE)
    , _client_hello_received(false)
    , _chosen_cipher(cipher_suites::TLS_NULL_WITH_NULL_NULL)
    , _chosen_group(
          handshake_protocol::extensions::NamedGroup::NAMED_GROUP_null)
{
}

Ssl::SslError Ssl::connect(const SslContext *ssl_ctx)
{
    _ctx = ssl_ctx;
    _state = STATE_HANDSHAKE;
    return ERROR_OK;
}

Ssl::SslError Ssl::accept()
{

    uint8_t tmp[RECV_CHUNK];
    ssize_t n = -1;

    n = recv(_fd, tmp, sizeof(tmp), 0);
    if (n > 0) {
        _buf.resize(_buf.size() + n);
        uint8_t *end = _buf.end().base() - n;
        std::memcpy(end, tmp, n);
        L_TRACE("Received {} bytes from client {}", n, _fd);
        if (_buf.size() > 5) {
            _current_message.type
                = static_cast<record_protocol::ContentType::type>(_buf[0]);
            _current_message.legacy_record_version.major = _buf[1];
            _current_message.legacy_record_version.minor = _buf[2];
            _current_message.content_length = _buf[3] << 8 | _buf[4];
            L_TRACE("Content type: {}, Protocol version: major: {} minor: {}, "
                    "Content length: {}",
                record_protocol::ContentType::strings[_current_message.type == 0
                        ? 0
                        : _current_message.type - 19],
                (int)_current_message.legacy_record_version.major,
                (int)_current_message.legacy_record_version.minor,
                _current_message.content_length);
        }
        if (_buf.size() >= _current_message.content_length) {
            _current_message.data = &_buf[5];
            if (_current_message.type
                == record_protocol::ContentType::CONTENT_TYPE_handshake) {
                handshake_protocol::ClientHello client_hello
                    = convert_to_client_hello(&_buf[5]);
                L_TRACE("client hello : {}", client_hello);
                if (check_client_hello(client_hello) == ERROR_FATAL) {
                    L_TRACE("Failed to check client hello");
                    return ERROR_FATAL;
                }
                L_TRACE("Client hello is ok");
                return ERROR_NEED_WRITE;
            }
        }
    }
    if (n == 0) {
        L_TRACE("Ssl client closed cleanly");
        return ERROR_FATAL;
    }

    return ERROR_NEED_READ;
}

Ssl::SslError Ssl::disconnect() { return ERROR_OK; }

int32_t Ssl::read(void *buf, int size)
{
    (void)buf;
    (void)size;
    return 0;
}
int32_t Ssl::write(const void *buf, int num)
{
    (void)buf;
    (void)num;
    return 1;
}

}
