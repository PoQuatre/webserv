/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Ssl.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: uanglade </var/spool/mail/uanglade>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 06:30:19 by uanglade          #+#    #+#             */
/*   Updated: 2026/06/10 23:43:38 by uanglade         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Ssl.hpp"

#include <fcntl.h>
#include <openssl/crypto.h>
#include <openssl/kdf.h>
#include <openssl/pem.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

handshake_protocol::extensions::SignatureAlgorithms
convert_to_signature_algorithms(
    const handshake_protocol::extensions::Extenion &ext)
{
    handshake_protocol::extensions::SignatureAlgorithms ret;
    const uint8_t *buf = ext.extenion_data;

    ret.length = buf[0] << 8 | buf[1];
    ret.count = ret.length / 2;
    buf += 2;
    for (int i = 0; i < ret.count; i++) {
        handshake_protocol::extensions::SignatureScheme::type group;

        group = static_cast<
            handshake_protocol::extensions::SignatureScheme::type>(
            buf[0] << 8 | buf[1]);
        buf += 2;
        ret.algorithms.push_back(group);
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
    ret.cipher_suites_count = ret.cipher_suites_length / 2;
    buf += 2;
    for (int i = 0; i < ret.cipher_suites_count; i++) {
        ret.cipher_suites.push_back(
            static_cast<cipher_suites::type>(buf[0] << 8 | buf[1]));
        buf += 2;
    }
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

std::vector<uint8_t> build_hkdf_label(size_t out_len, const std::string &label,
    const uint8_t *context, size_t context_len)
{
    std::vector<uint8_t> ret;

    std::string tls_label = "tls13 " + label;

    ret.push_back((out_len >> 8) & 0xff);
    ret.push_back(out_len & 0xff);
    ret.push_back(tls_label.size());
    ret.insert(ret.end(), tls_label.begin(), tls_label.end());
    ret.push_back(context_len);
    ret.insert(ret.end(), context, context + context_len);

    return ret;
}

bool hkdf_expand_label(const uint8_t *secret, size_t secret_len,
    const char *label, const uint8_t *context, size_t context_len,
    uint8_t *output, size_t output_len)
{
    std::vector<uint8_t> info
        = build_hkdf_label(output_len, label, context, context_len);

    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);

    if (!pctx)
        return false;

    if (EVP_PKEY_derive_init(pctx) <= 0)
        return false;

    if (EVP_PKEY_CTX_set_hkdf_mode(pctx, EVP_PKEY_HKDEF_MODE_EXPAND_ONLY) <= 0)
        return false;

    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0)
        return false;

    if (EVP_PKEY_CTX_set1_hkdf_key(
            pctx, secret, static_cast<int32_t>(secret_len))
        <= 0)
        return false;

    if (EVP_PKEY_CTX_add1_hkdf_info(
            pctx, info.data(), static_cast<int32_t>(info.size()))
        <= 0)
        return false;

    size_t len = output_len;

    if (EVP_PKEY_derive(pctx, output, &len) <= 0)
        return false;

    EVP_PKEY_CTX_free(pctx);

    return true;
}

bool hkdf_extract(const uint8_t *salt, size_t salt_len, const uint8_t *ikm,
    size_t ikm_len, uint8_t out[32])
{
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);

    if (!pctx)
        return false;

    if (EVP_PKEY_derive_init(pctx) <= 0)
        return false;
    if (EVP_PKEY_CTX_set_hkdf_mode(pctx, EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY) <= 0)
        return false;
    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0)
        return false;
    if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt, static_cast<int32_t>(salt_len))
        <= 0)
        return false;
    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, ikm, static_cast<int32_t>(ikm_len))
        <= 0)
        return false;

    size_t len = 32;
    if (EVP_PKEY_derive(pctx, out, &len) <= 0)
        return false;

    EVP_PKEY_CTX_free(pctx);

    return true;
}

void generate_random_number(uint8_t *rand, uint8_t len)
{
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1) {
        for (int i = 0; i < len; i++) {
            rand[i] = (uint8_t)random();
        }
        return;
    }
    read(fd, rand, len);
    close(fd);
}

}

Ssl::SslError Ssl::check_client_hello(
    const handshake_protocol::ClientHello &msg)
{

    for (int i = 0; i < msg.cipher_suites_count; ++i) {
        if (cipher_suites::is_supported(msg.cipher_suites[i])) {
            _chosen_cipher = msg.cipher_suites[i];
            L_TRACE(
                "Chose cipher: {}", cipher_suites::to_string(_chosen_cipher));
        }
    }
    if (_chosen_cipher == cipher_suites::TLS_NULL_WITH_NULL_NULL) {
        L_ERROR(
            "No supported cipher in cipher suites provided by client {}", _fd);
        return ERROR_FATAL;
    }

    handshake_protocol::extensions::KeyShare key_share;
    handshake_protocol::extensions::SupportedGroups supported_groups;
    handshake_protocol::extensions::SignatureAlgorithms signature_algorithms;

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
        case ssl::handshake_protocol::extensions::ExtensionType::
            EXTENSION_TYPE_signature_algorithms:
            signature_algorithms = convert_to_signature_algorithms(ext);
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

    L_TRACE("Key share: {}", _chose_key_share);

    memcpy(_client_public_key, _chose_key_share.key_exchange, 32);

    return ERROR_OK;
}

Ssl::Ssl(int32_t fd)
    : _fd(fd)
    , _state(STATE_NONE)
    , _client_hello_received(false)
    , _chosen_cipher(cipher_suites::TLS_NULL_WITH_NULL_NULL)
    , _chosen_group(
          handshake_protocol::extensions::NamedGroup::NAMED_GROUP_null)
    , _chosen_signature_algo(handshake_protocol::extensions::SignatureScheme::
              SIGNATURE_SCHEME_null)
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
        _read_buf.resize(_read_buf.size() + n);
        uint8_t *end = _read_buf.end().base() - n;
        std::memcpy(end, tmp, n);
        L_TRACE("Received {} bytes from client {}", n, _fd);
        if (_read_buf.size() > 5) {
            _current_message.type
                = static_cast<record_protocol::ContentType::type>(_read_buf[0]);
            _current_message.legacy_record_version.major = _read_buf[1];
            _current_message.legacy_record_version.minor = _read_buf[2];
            _current_message.content_length = _read_buf[3] << 8 | _read_buf[4];
            L_TRACE("Content type: {}, Protocol version: major: {} minor: {}, "
                    "Content length: {}",
                record_protocol::ContentType::strings[_current_message.type == 0
                        ? 0
                        : _current_message.type - 19],
                (int)_current_message.legacy_record_version.major,
                (int)_current_message.legacy_record_version.minor,
                _current_message.content_length);
        }
        if (_read_buf.size() >= _current_message.content_length) {
            _buf_records.push_back(_read_buf);
            _current_message.data = &_read_buf[5];
            if (_current_message.type
                == record_protocol::ContentType::CONTENT_TYPE_handshake) {
                _handshake_records.push_back(_current_message);
                _client_hello = convert_to_client_hello(&_read_buf[5]);
                L_TRACE("client hello : {}", _client_hello);
                if (check_client_hello(_client_hello) == ERROR_FATAL) {
                    L_TRACE("Failed to check client hello");
                    return ERROR_FATAL;
                }
                generate_server_hello();
                size_t n = send(_fd, _send_buf.data(), _send_buf.size(), 0);
                if (n > 0) {
                    L_TRACE("Send {} bytes to client {}", n, _fd);
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
    if (_send_buf.size() > 0) {}
    (void)buf;
    (void)num;
    return 1;
}

#define KEY_PRINT(name, value)                                                 \
    std::cout << (name);                                                       \
    for (int i = 0; i < 32; i++) {                                             \
        std::cout << std::hex << (int)(value)[i];                              \
    }                                                                          \
    std::cout << "\n";

void Ssl::generate_keys()
{
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    EVP_PKEY_keygen_init(pctx);
    EVP_PKEY_keygen(pctx, &pkey);

    EVP_PKEY_CTX *ctx;
    size_t skeylen;
    EVP_PKEY *peerkey = NULL;

    peerkey = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_X25519, NULL, _client_public_key, 32);
    if (!peerkey)
        L_ERROR("Failed to create peer key");

    ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (EVP_PKEY_derive_init(ctx) <= 0)
        L_ERROR("Failed to derive key");

    if (EVP_PKEY_derive_set_peer(ctx, peerkey) <= 0)
        L_ERROR("Failed to derive key");

    if (EVP_PKEY_derive(ctx, NULL, &skeylen) <= 0)
        L_ERROR("Failed to derive key");

    if (skeylen != 32)
        L_ERROR("Failed to derive key");

    if (EVP_PKEY_derive(ctx, _shared_key, &skeylen) <= 0)
        L_ERROR("Failed to derive shared key");

    EVP_PKEY_CTX_free(pctx);
    EVP_PKEY_CTX_free(ctx);
    size_t len = 32;
    EVP_PKEY_get_raw_private_key(pkey, _server_private_key, &len);
    len = 32;
    EVP_PKEY_get_raw_public_key(pkey, _server_public_key, &len);
    EVP_PKEY_free(pkey);
    EVP_PKEY_free(peerkey);

    KEY_PRINT("Client Public: ", _client_public_key);
    KEY_PRINT("Server private: ", _server_public_key);
    KEY_PRINT("Server public: ", _server_private_key);
    KEY_PRINT("Shared: ", _shared_key);

    uint8_t zeros[32] = {};
    uint8_t early_secret[32];

    hkdf_extract(zeros, 32, zeros, 32, early_secret);

    KEY_PRINT("Early secret: ", early_secret);

    std::vector<uint8_t> empty;

    uint8_t derived_secret[32];

    derive_secret(early_secret, "derived", empty, derived_secret);

    KEY_PRINT("Derived secret: ", early_secret);
    uint8_t handshake_secret[32];

    hkdf_extract(derived_secret, 32, _shared_key, 32, handshake_secret);

    KEY_PRINT("Handshake secret: ", early_secret);

    // std::vector<uint8_t> transcript;
    // transcript.insert(
    //     transcript.end(), _buf_records[0].begin(), _buf_records[0].end());
    // transcript.insert(
    //     transcript.end(), _buf_records[1].begin(), _buf_records[1].end());
    //
    // uint8_t client_hs_secret[32];
    // derive_secret(
    //     handshake_secret, "c hs traffic", transcript, client_hs_secret);
    // uint8_t server_hs_secret[32];
    // derive_secret(
    //     handshake_secret, "s hs traffic", transcript, server_hs_secret);
}

void Ssl::generate_server_hello()
{
    generate_keys();
    std::vector<uint8_t> msg;

    msg.push_back(record_protocol::ContentType::CONTENT_TYPE_handshake);
    // VERSION tls 1.2
    msg.push_back(0x03);
    msg.push_back(0x03);

    uint32_t length_idx = msg.size();

    msg.push_back(0x00);
    msg.push_back(0x00);

    msg.push_back(
        handshake_protocol::HandshakeType::HANDSHAKE_TYPE_server_hello);

    uint32_t hello_idx = msg.size();
    msg.push_back(0x00);
    msg.push_back(0x00);
    msg.push_back(0x00);

    // VERSION tls 1.2
    msg.push_back(0x03);
    msg.push_back(0x03);

    // random number
    uint8_t random[32];
    generate_random_number(random, 32);
    for (int i = 0; i < 32; i++)
        msg.push_back(random[i]);

    // session id length
    msg.push_back(32);
    for (int i = 0; i < 32; i++)
        msg.push_back(_client_hello.session_id[i]);

    L_TRACE("Pushed {} cipher, value {} {}",
        cipher_suites::to_string(_chosen_cipher),
        (int)(_chosen_cipher >> 8 & 0xff), (int(_chosen_cipher & 0xff)));
    // chosen cipher
    msg.push_back(_chosen_cipher >> 8);
    msg.push_back(_chosen_cipher);

    // compression method: 0x00
    msg.push_back(0x00);

    uint16_t extensions_begin = msg.size();

    msg.push_back(0x00);
    msg.push_back(0x00);

    uint8_t supported_versions_bytes[] = { 0x0, 0x2b, 0x0, 0x2, 0x3, 0x4 };
    for (size_t i = 0; i
        < sizeof(supported_versions_bytes) / sizeof(*supported_versions_bytes);
        i++)
        msg.push_back(supported_versions_bytes[i]);

    unsigned char key_share_extension[]
        = { 0x0, 0x33, 0x0, 0x24, 0x0, 0x1d, 0x0, 0x20 };

    for (size_t i = 0;
        i < sizeof(key_share_extension) / sizeof(*key_share_extension); i++)
        msg.push_back(key_share_extension[i]);

    for (size_t i = 0; i < 32; i++)
        msg.push_back(_server_public_key[i]);

    msg[extensions_begin] = ((msg.size() - extensions_begin - 2) >> 8);
    msg[extensions_begin + 1] = ((msg.size() - extensions_begin - 2));

    msg[hello_idx] = (uint8_t)((msg.size() - hello_idx - 3) >> 16);
    msg[hello_idx + 1] = (uint8_t)((msg.size() - hello_idx - 3) >> 8);
    msg[hello_idx + 2] = (uint8_t)((msg.size() - hello_idx - 3));

    msg[length_idx] = ((msg.size() - 5) >> 8);
    msg[length_idx + 1] = (msg.size() - 5);

    _send_buf.insert(_send_buf.end(), msg.begin(), msg.end());
}

void Ssl::derive_secret(const uint8_t *secret, const char *label,
    const std::vector<uint8_t> &transcript, uint8_t out[32])
{
    uint8_t hash[32];

    SHA256(transcript.data(), transcript.size(), hash);

    hkdf_expand_label(secret, 32, label, hash, 32, out, 32);
}

}
