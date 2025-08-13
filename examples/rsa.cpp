/*
    COPYRIGHT: David Geis 2025
    LICENSE:   MIT
    CONTACT:   davidgeis@web.de
*/

#include <filesystem>
#include <fstream>
#include <string_view>
#include <thread>
#include <iostream>
#include <cstring>

#include "../mpa_integer.h"

using word_t = uint64_t;

/*
    Version ::= INTEGER { two-prime(0), multi(1) }
    (CONSTRAINED BY
    {-- version must be multi if otherPrimeInfos present --})

    RSAPrivateKey ::= SEQUENCE {
        version           Version,
        modulus           INTEGER,  -- n
        publicExponent    INTEGER,  -- e
        privateExponent   INTEGER,  -- d
        prime1            INTEGER,  -- p
        prime2            INTEGER,  -- q
        exponent1         INTEGER,  -- d mod (p-1)
        exponent2         INTEGER,  -- d mod (q-1)
        coefficient       INTEGER,  -- (inverse of q) mod p
        otherPrimeInfos   OtherPrimeInfos OPTIONAL
}
*/

namespace
{
    constexpr size_t bits_in_word = sizeof(word_t) * 8;

    constexpr std::string_view base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    void b64_decode(const std::vector<uint8_t> &b64_data, std::vector<uint8_t> &ret)
    {
        size_t input_length = b64_data.size();
        size_t i = 0;
        size_t j = 0;
        size_t offset = 0;
        uint8_t tmp_3[3], tmp_4[4];
        while (input_length-- && (b64_data[offset] != '=') &&
               (isalnum(b64_data[offset]) || (b64_data[offset] == '+') || (b64_data[offset] == '/')))
        {
            tmp_4[i++] = b64_data[offset];
            offset += 1;
            if (i == 4)
            {
                for (i = 0; i < 4; i++)
                {
                    tmp_4[i] = base64_chars.find(tmp_4[i]);
                }
                tmp_3[0] = (tmp_4[0] << 2U) + ((tmp_4[1] & 0x30) >> 4U);
                tmp_3[1] = ((tmp_4[1] & 0xf) << 4U) + ((tmp_4[2] & 0x3c) >> 2U);
                tmp_3[2] = ((tmp_4[2] & 0x3) << 6U) + tmp_4[3];
                for (i = 0; (i < 3); i++)
                {
                    ret.push_back(tmp_3[i]);
                }
                i = 0;
            }
        }
        if (i)
        {
            for (j = i; j < 4; j++)
            {
                tmp_4[j] = 0;
            }
            for (j = 0; j < 4; j++)
            {
                tmp_4[j] = base64_chars.find(tmp_4[j]);
            }
            tmp_3[0] = (tmp_4[0] << 2U) + ((tmp_4[1] & 0x30) >> 4U);
            tmp_3[1] = ((tmp_4[1] & 0xf) << 4U) + ((tmp_4[2] & 0x3c) >> 2U);
            tmp_3[2] = ((tmp_4[2] & 0x3) << 6U) + tmp_4[3];
            for (j = 0; (j < i - 1); j++)
            {
                ret.push_back(tmp_3[j]);
            }
        }
    }

    void b64_encode(const uint8_t *buf, size_t buf_size, std::vector<uint8_t> &out)
    {
        out.reserve((1 + buf_size / 3) << 2U);
        size_t i = 0;
        uint8_t tmp_3[3], tmp_4[4];
        while (buf_size--)
        {
            tmp_3[i++] = *(buf++);
            if (i == 3)
            {
                tmp_4[0] = (tmp_3[0] & 0xfc) >> 2U;
                tmp_4[1] = ((tmp_3[0] & 0x03) << 4U) | ((tmp_3[1] & 0xf0) >> 4U);
                tmp_4[2] = ((tmp_3[1] & 0x0f) << 2U) | ((tmp_3[2] & 0xc0) >> 6U);
                tmp_4[3] = tmp_3[2] & 0x3f;
                out.push_back(base64_chars[tmp_4[0]]);
                out.push_back(base64_chars[tmp_4[1]]);
                out.push_back(base64_chars[tmp_4[2]]);
                out.push_back(base64_chars[tmp_4[3]]);
                i = 0;
            }
        }
        if (i)
        {
            memset(tmp_3 + i, 0, 3 - i);
            tmp_4[0] = (tmp_3[0] & 0xfc) >> 2U;
            tmp_4[1] = ((tmp_3[0] & 0x03) << 4U) | ((tmp_3[1] & 0xf0) >> 4U);
            tmp_4[2] = ((tmp_3[1] & 0x0f) << 2U) | ((tmp_3[2] & 0xc0) >> 6U);
            tmp_4[3] = tmp_3[2] & 0x3f;
            for (size_t j = 0; j <= i; ++j)
            {
                out.push_back(base64_chars[tmp_4[j]]);
            }
            while (i++ < 3)
            {
                out.push_back('=');
            }
        }
    }

    size_t DER_read_length(size_t &offset, std::vector<uint8_t> &b64_decoded)
    {
        if (b64_decoded[offset] != 2)
        {
            std::cerr << "expected integer tag, but received: " << (int)b64_decoded[offset] << "\n";
            return false;
        }
        offset += 1;
        uint8_t length_tag = b64_decoded[offset];
        offset += 1;
        size_t length;
        if (length_tag <= 0x7f)
        {
            length = length_tag;
            return length;
        }
        if (length_tag == 0x81)
        {
            length = b64_decoded[offset];
            offset += 1;
            return length;
        }
        if (length_tag == 0x82)
        {
            length = (b64_decoded[offset] << 8U) | (b64_decoded[offset + 1]);
            offset += 2;
            return length;
        }
        if (length_tag == 0x83)
        {
            length = (b64_decoded[offset] << 16U) | (b64_decoded[offset + 1] << 8U) | (b64_decoded[offset + 2]);
            offset += 3;
            return length;
        }
        if (length_tag == 0x84)
        {
            length = (b64_decoded[offset] << 24U) | (b64_decoded[offset + 1] << 16U) | (b64_decoded[offset + 2] << 8U) | (b64_decoded[offset + 3]);
            offset += 4;
            return length;
        }
        std::cerr << "error: bad length tag " << (int)length_tag << "\n";
        return 0;
    }

    void DER_put_length(std::vector<uint8_t> &bytes, const size_t byte_length)
    {
        if (byte_length <= 0x7f)
        {
            bytes.push_back(byte_length);
        }
        else if (byte_length <= 0xff)
        {
            bytes.push_back(0x81);
            bytes.push_back(byte_length);
        }
        else if (byte_length <= 0xffff)
        {
            bytes.push_back(0x82);
            bytes.push_back(byte_length >> 8U);
            bytes.push_back(byte_length & 255);
        }
        else if (byte_length <= 0xffffff)
        {
            bytes.push_back(0x83);
            bytes.push_back(byte_length >> 16U);
            bytes.push_back((byte_length >> 8U) & 255);
            bytes.push_back(byte_length & 255);
        }
        else if (byte_length <= 0xffffffff)
        {
            bytes.push_back(0x84);
            bytes.push_back(byte_length >> 24U);
            bytes.push_back((byte_length >> 16U) & 255);
            bytes.push_back((byte_length >> 8U) & 255);
            bytes.push_back(byte_length & 255);
        }
        else
        {
            std::cerr << "ERROR: length too large: " << byte_length << ", terminating\n";
            exit(1);
        }
    }

    MPA::Integer<word_t> construct_integer_from_bigendian_bytebuffer(const std::vector<uint8_t> &bytebuffer, const size_t bytebuffer_size, size_t &offset)
    {
        MPA::Integer<word_t> out = 0;
        for (size_t i = 0; i < bytebuffer_size; ++i)
        {
            out += MPA::Integer<word_t>(bytebuffer[offset]) << ((bytebuffer_size - 1 - i) << 3U);
            offset += 1;
        }
        return out;
    }

    void serialize_tail(std::vector<uint8_t> &bytes, const MPA::Integer<word_t> &x)
    {
        for (size_t i = x.get_head() - 1; i < x.get_head(); --i)
        {
            word_t word = x.get_word(i);
            int shift = bits_in_word - 8;
            while (shift >= 0)
            {
                bytes.push_back((word >> shift) & 0xFF);
                shift -= 8;
            }
        }
    }

    void serialize_head_private_key(std::vector<uint8_t> &bytes, const word_t leading_word, const size_t bytesize)
    {
        for (size_t j = 0; j < bits_in_word / 8; ++j)
        {
            if (((leading_word >> ((bits_in_word - 8) - j * 8)) & 255) || j == bits_in_word / 8 - 1)
            {
                bytes.push_back(2); // type tag for int
                const bool MSB_set = ((leading_word >> ((bits_in_word - 8) - j * 8)) & 255) & 128;
                if (MSB_set) // in this case we prepend a zero byte
                {
                    DER_put_length(bytes, bytesize + 1);
                    bytes.push_back(0);
                }
                else
                {
                    DER_put_length(bytes, bytesize);
                }
                for (size_t i = j; i < bits_in_word / 8; ++i)
                {
                    bytes.push_back((leading_word >> ((bits_in_word - 8) - i * 8)) & 255);
                }
                return;
            }
        }
    }

    void DER_serialize(std::vector<uint8_t> &bytes, const MPA::Integer<word_t> &x)
    {
        const uint32_t x_binary_size = x.to_binary().size() - 2;
        const uint32_t x_byte_size = x_binary_size & 7 ? (x_binary_size >> 3U) + 1 : (x_binary_size >> 3U);
        const word_t leading_word = x.get_word(x.get_head());
        serialize_head_private_key(bytes, leading_word, x_byte_size);
        serialize_tail(bytes, x);
    }

    void serialize_head_ssh_public_key(std::vector<uint8_t> &bytes, const word_t leading_word)
    {
        for (size_t j = 0; j < bits_in_word / 8; ++j)
        {
            if (((leading_word >> ((bits_in_word - 8) - j * 8)) & 255) || j == bits_in_word / 8 - 1)
            {
                const bool MSB_set = ((leading_word >> ((bits_in_word - 8) - j * 8)) & 255) & 128;
                if (MSB_set) // in this case we prepend a zero byte
                {
                    bytes.back() += 1; // adjust the byte size
                    if (!bytes.back()) // may cause overflow
                    {
                        bytes[bytes.size() - 2] += 1;
                    }
                    bytes.push_back(0);
                }
                for (size_t i = j; i < bits_in_word / 8; ++i)
                {
                    bytes.push_back((leading_word >> ((bits_in_word - 8) - i * 8)) & 255);
                }
                break;
            }
        }
    }
}

struct RSA // two-prime only
{
    MPA::Integer<word_t> n;
    MPA::Integer<word_t> e;
    MPA::Integer<word_t> d;
    MPA::Integer<word_t> p;
    MPA::Integer<word_t> q;

    RSA() : n(0), e(0), d(0), p(0), q(0){};

    RSA(const MPA::Integer<word_t> &n_, const MPA::Integer<word_t> &p_, const MPA::Integer<word_t> &q_, const MPA::Integer<word_t> &e_, const MPA::Integer<word_t> &d_)
        : n(n_), e(e_), d(d_), p(p_), q(q_) {}

    size_t write_private_key(const std::string &outfile_name) const
    {
        std::vector<uint8_t> bytes;

        // sequence
        bytes.push_back(0x30);       // sequence type
        uint8_t control_byte = 0x82; // sequence length in long form: use 2 bytes for the length
        bytes.push_back(control_byte);
        bytes.push_back(0); // fill the actual length later;
        bytes.push_back(0); // fill the actual length later;

        // version
        DER_serialize(bytes, MPA::Integer<word_t>(0)); // "two-prime" only

        // modulus -> n = p * q
        DER_serialize(bytes, n);

        // encryption exponent -> e
        DER_serialize(bytes, e);

        // decryption exponent -> d
        DER_serialize(bytes, d);

        // prime 1 -> p
        DER_serialize(bytes, p);

        // prime 2 -> q
        DER_serialize(bytes, q);

        // exponent 1 -> d mod (p-1)
        DER_serialize(bytes, d % (p - 1));

        // exponent 2 -> d mod (q-1)
        DER_serialize(bytes, d % (q - 1));

        // coefficient -> q^-1 mod p
        DER_serialize(bytes, MPA::modular_inverse(q, p));

        // fix the sequence length
        const size_t effective_length = bytes.size() - 4;
        bytes[2] = effective_length >> 8U;
        bytes[3] = effective_length & 255;

        // write the output file
        size_t bytes_written = 0;
        const std::string prefix = "-----BEGIN RSA PRIVATE KEY-----\n";
        const std::string postfix = "-----END RSA PRIVATE KEY-----\n";
        std::vector<uint8_t> b64;
        b64_encode(bytes.data(), bytes.size(), b64);
        std::ofstream fs(outfile_name, std::ios::out | std::ios::binary);
        fs.write(prefix.data(), prefix.size());
        bytes_written += prefix.size();
        size_t offset = 0;
        const uint8_t *b64_data = b64.data();
        const size_t b64_size = b64.size();
        while (offset < b64_size)
        {
            const size_t leftover = b64_size - offset;
            const size_t chunksize = leftover >= 70 ? 70 : leftover;
            fs.write((const char *)(b64_data + offset), chunksize);
            fs.write("\n", 1);
            bytes_written += chunksize + 1;
            offset += chunksize;
        }
        fs.write(postfix.data(), postfix.size());
        bytes_written += postfix.size();
        fs.close();

        return bytes_written;
    }

    size_t write_ssh_public_key(const std::string &outfile_name) const
    {
        // initialize buffer with string length 7 and string content 'ssh-rsa'
        std::vector<uint8_t> bytes{0x0, 0x0, 0x0, 0x07, 0x73, 0x73, 0x68, 0x2d, 0x72, 0x73, 0x61};

        // write the length of encryption exponent (will fit in one byte, but we need four bytes for the length)
        const uint32_t e_binary_size = e.to_binary().size() - 2;
        const uint32_t e_byte_size = e_binary_size & 7 ? (e_binary_size >> 3U) + 1 : (e_binary_size >> 3U);
        bytes.push_back(0);
        bytes.push_back(0);
        bytes.push_back(0);
        bytes.push_back(e_byte_size);

        // write the encryption exponent
        serialize_head_ssh_public_key(bytes, e.get_word(e.get_head()));
        serialize_tail(bytes, e);

        // write the length of modulus
        const uint32_t modulus_binary_size = n.to_binary().size() - 2;
        const uint32_t modulus_byte_size = modulus_binary_size & 7 ? (modulus_binary_size >> 3U) + 1 : (modulus_binary_size >> 3U);
        bytes.push_back(modulus_byte_size >> 24U);
        bytes.push_back((modulus_byte_size >> 16U) & 255);
        bytes.push_back((modulus_byte_size >> 8U) & 255);
        bytes.push_back(modulus_byte_size & 255);

        // write the modulus
        serialize_head_ssh_public_key(bytes, n.get_word(n.get_head()));
        serialize_tail(bytes, n);

        // write the output file
        size_t bytes_written = 0;
        const std::string prefix = "ssh-rsa ";
        const std::string postfix = " generated-by-MPA\n";
        std::vector<uint8_t> b64;
        b64_encode(bytes.data(), bytes.size(), b64);
        std::ofstream fs(outfile_name, std::ios::out | std::ios::binary);
        fs.write(prefix.data(), prefix.size());
        bytes_written += prefix.size();
        fs.write(reinterpret_cast<const char *>(b64.data()), b64.size());
        bytes_written += b64.size();
        fs.write(postfix.data(), postfix.size());
        bytes_written += postfix.size();
        fs.close();

        return bytes_written;
    }
};

std::ostream &operator<<(std::ostream &os, const RSA &rsa) noexcept
{
    os << "<<<RSA PRIVATE KEY DETAIL START>>>\n\n";
    os << "modulus:\n"
       << rsa.n << "\n\n";
    os << "prime 1:\n"
       << rsa.p << "\n\n";
    os << "prime 2:\n"
       << rsa.q << "\n\n";
    os << "encryption exponent:\n"
       << rsa.e << "\n\n";
    os << "decryption exponent:\n"
       << rsa.d << "\n\n";
    os << "<<<RSA PRIVATE KEY DETAIL END>>>\n\n";
    return os;
}

RSA generate_rsa_key(const size_t bitlength)
{
    MPA::Integer<word_t> p, q, n, lambda, e, d;
    while (true) // as long as p == q
    {
        // choose p, q prime such that p, q require bitlength / 2 many bits
        std::thread get_p([&p, bitlength]()
                          { p = MPA::get_random_prime<word_t>(bitlength / (2 * bits_in_word), true); });
        q = MPA::get_random_prime<word_t>(bitlength / (2 * bits_in_word), true);
        get_p.join();
        if (p != q)
        {
            break;
        }
    }
    // compute modulus
    n = p * q;
    // compute carmichael totient function
    lambda = MPA::lcm(p - 1, q - 1);
    // choose e coprime to lambda such that 1 < e < lambda
    e = MPA::Integer<word_t>("65537"); // use string constructor to make this line work also for uint16_t ;)
    d = MPA::modular_inverse(e, lambda);
    while (d.is_zero()) // as long as e and lambda are not coprime
    {
        e -= 1;
        d = MPA::modular_inverse(e, lambda);
    }
    return RSA(n, p, q, e, d);
}

bool parse_rsa_public_key(const std::vector<uint8_t> &rsa_public_key_bytes, MPA::Integer<word_t> &exponent, MPA::Integer<word_t> &modulus)
{
    std::vector<uint8_t> b64_decoded;
    b64_decode(rsa_public_key_bytes, b64_decoded);
    const static std::string expected_validation_str = "ssh-rsa";
    const size_t expected_validation_str_size = 7;
    size_t offset = 0;
    const size_t actual_validation_str_size = (b64_decoded[offset] << 24U) |
                                              (b64_decoded[offset + 1] << 16U) |
                                              (b64_decoded[offset + 2] << 8U) |
                                              (b64_decoded[offset + 3]);
    offset += 4;
    if (expected_validation_str_size != actual_validation_str_size)
    {
        return false;
    }
    const std::string actual_validation_str{(char)b64_decoded[offset], (char)b64_decoded[offset + 1],
                                            (char)b64_decoded[offset + 2], (char)b64_decoded[offset + 3],
                                            (char)b64_decoded[offset + 4], (char)b64_decoded[offset + 5],
                                            (char)b64_decoded[offset + 6]};
    offset += 7;
    if (expected_validation_str != actual_validation_str)
    {
        return false;
    }
    const size_t exponent_size = (b64_decoded[offset] << 24U) |
                                 (b64_decoded[offset + 1] << 16U) |
                                 (b64_decoded[offset + 2] << 8U) |
                                 (b64_decoded[offset + 3]);
    offset += 4;
    exponent = construct_integer_from_bigendian_bytebuffer(b64_decoded, exponent_size, offset);
    const size_t modulus_size = (b64_decoded[offset] << 24U) |
                                (b64_decoded[offset + 1] << 16U) |
                                (b64_decoded[offset + 2] << 8U) |
                                (b64_decoded[offset + 3]);
    offset += 4;
    modulus = construct_integer_from_bigendian_bytebuffer(b64_decoded, modulus_size, offset);
    if (offset != b64_decoded.size())
    {
        return false;
    }
    return true;
}

bool parse_rsa_private_key(const std::vector<uint8_t> &rsa_private_key_bytes, RSA &rsa)
{
    // b64 decode
    std::vector<uint8_t> b64_decoded;
    b64_decode(rsa_private_key_bytes, b64_decoded);
    size_t offset = 0;
    // handle openssh key format
    const char openssh_identifier[15] = "openssh-key-v1";
    if (!memcmp(openssh_identifier, b64_decoded.data(), 15))
    {
        offset += 15;
        // parse the cipher name
        size_t cipher_name_len = (b64_decoded[offset] << 24U) |
                                 (b64_decoded[offset + 1] << 16U) |
                                 (b64_decoded[offset + 2] << 8U) |
                                 (b64_decoded[offset + 3]);
        offset += 4;
        std::vector<uint8_t> cipher_name;
        for (size_t i = 0; i < cipher_name_len; ++i)
        {
            cipher_name.push_back(b64_decoded[offset + i]);
        }
        offset += cipher_name_len;
        // parse kdf name
        size_t kdf_name_len = (b64_decoded[offset] << 24U) |
                              (b64_decoded[offset + 1] << 16U) |
                              (b64_decoded[offset + 2] << 8U) |
                              (b64_decoded[offset + 3]);
        offset += 4;
        std::vector<uint8_t> kdf_name;
        for (size_t i = 0; i < kdf_name_len; ++i)
        {
            kdf_name.push_back(b64_decoded[offset + i]);
        }
        offset += kdf_name_len;
        // parse kdf
        size_t kdf_len = (b64_decoded[offset] << 24U) |
                         (b64_decoded[offset + 1] << 16U) |
                         (b64_decoded[offset + 2] << 8U) |
                         (b64_decoded[offset + 3]);
        offset += 4;
        std::vector<uint8_t> kdf;
        for (size_t i = 0; i < kdf_len; ++i)
        {
            kdf.push_back(b64_decoded[offset + i]);
        }
        offset += kdf_len;
        // parse key count (should be 1)
        size_t key_count = (b64_decoded[offset] << 24U) |
                           (b64_decoded[offset + 1] << 16U) |
                           (b64_decoded[offset + 2] << 8U) |
                           (b64_decoded[offset + 3]);
        if (key_count != 1)
        {
            std::cerr << "ERROR: expected key count to be 1, but read " << key_count << "\n";
            return false;
        }
        offset += 4;
        // parse embedded ssh public key
        /*size_t public_key_length = (b64_decoded[offset] << 24U) |
                                   (b64_decoded[offset + 1] << 16U) |
                                   (b64_decoded[offset + 2] << 8U) |
                                   (b64_decoded[offset + 3]);*/
        offset += 4;
        size_t pub_identifier_length = (b64_decoded[offset] << 24U) |
                                       (b64_decoded[offset + 1] << 16U) |
                                       (b64_decoded[offset + 2] << 8U) |
                                       (b64_decoded[offset + 3]);
        offset += 4;
        const char identifier[8] = "ssh-rsa";
        if (pub_identifier_length != 7 || memcmp(identifier, &b64_decoded[offset], 7))
        {
            std::cerr << "ERROR: embedded public key format not supported\n";
            return false;
        }
        offset += 7;
        size_t pub_exponent_size = (b64_decoded[offset] << 24U) |
                                   (b64_decoded[offset + 1] << 16U) |
                                   (b64_decoded[offset + 2] << 8U) |
                                   (b64_decoded[offset + 3]);
        offset += 4;
        MPA::Integer pub_exponent = construct_integer_from_bigendian_bytebuffer(b64_decoded, pub_exponent_size, offset);
        const size_t pub_modulus_size = (b64_decoded[offset] << 24U) |
                                        (b64_decoded[offset + 1] << 16U) |
                                        (b64_decoded[offset + 2] << 8U) |
                                        (b64_decoded[offset + 3]);
        offset += 4;
        MPA::Integer pub_modulus = construct_integer_from_bigendian_bytebuffer(b64_decoded, pub_modulus_size, offset);
        // parse the length for rnd + private key + comment + padding
        /*size_t private_key_size = (b64_decoded[offset] << 24U) |
                                  (b64_decoded[offset + 1] << 16U) |
                                  (b64_decoded[offset + 2] << 8U) |
                                  (b64_decoded[offset + 3]);*/
        offset += 4;
        /*uint64_t rnd = ((uint64_t)b64_decoded[offset] << 56U) |
                       ((uint64_t)b64_decoded[offset + 1] << 48U) |
                       ((uint64_t)b64_decoded[offset + 2] << 40U) |
                       ((uint64_t)b64_decoded[offset + 3] << 32U) |
                       ((uint64_t)b64_decoded[offset + 4] << 24U) |
                       ((uint64_t)b64_decoded[offset + 5] << 16U) |
                       ((uint64_t)b64_decoded[offset + 6] << 8U) |
                       ((uint64_t)b64_decoded[offset + 7]);*/
        offset += 8;
        size_t priv_identifier_length = (b64_decoded[offset] << 24U) |
                                        (b64_decoded[offset + 1] << 16U) |
                                        (b64_decoded[offset + 2] << 8U) |
                                        (b64_decoded[offset + 3]);
        offset += 4;
        if (priv_identifier_length != 7 || memcmp(identifier, &b64_decoded[offset], 7))
        {
            std::cerr << "ERROR: embedded public key format not supported\n";
            return false;
        }
        offset += 7;
        // parse modulus -> n
        size_t modulus_size = (b64_decoded[offset] << 24U) |
                              (b64_decoded[offset + 1] << 16U) |
                              (b64_decoded[offset + 2] << 8U) |
                              (b64_decoded[offset + 3]);
        offset += 4;
        MPA::Integer modulus = construct_integer_from_bigendian_bytebuffer(b64_decoded, modulus_size, offset);
        // parse encryption exponent -> e
        const size_t encryption_exponent_size = (b64_decoded[offset] << 24U) |
                                                (b64_decoded[offset + 1] << 16U) |
                                                (b64_decoded[offset + 2] << 8U) |
                                                (b64_decoded[offset + 3]);
        offset += 4;
        MPA::Integer encryption_exponent = construct_integer_from_bigendian_bytebuffer(b64_decoded, encryption_exponent_size, offset);
        // parse decryption exponent -> d
        size_t decryption_exponent_size = (b64_decoded[offset] << 24U) |
                                          (b64_decoded[offset + 1] << 16U) |
                                          (b64_decoded[offset + 2] << 8U) |
                                          (b64_decoded[offset + 3]);
        offset += 4;
        MPA::Integer decryption_exponent = construct_integer_from_bigendian_bytebuffer(b64_decoded, decryption_exponent_size, offset);
        // parse cofficient -> q^-1 mod p
        size_t cofficient_size = (b64_decoded[offset] << 24U) |
                                 (b64_decoded[offset + 1] << 16U) |
                                 (b64_decoded[offset + 2] << 8U) |
                                 (b64_decoded[offset + 3]);
        offset += 4;
        MPA::Integer coefficient = construct_integer_from_bigendian_bytebuffer(b64_decoded, cofficient_size, offset);
        // parse prime 1 -> p
        size_t prime1_size = (b64_decoded[offset] << 24U) |
                             (b64_decoded[offset + 1] << 16U) |
                             (b64_decoded[offset + 2] << 8U) |
                             (b64_decoded[offset + 3]);
        offset += 4;
        MPA::Integer prime1 = construct_integer_from_bigendian_bytebuffer(b64_decoded, prime1_size, offset);
        // parse prime 2 -> q
        size_t prime2_size = (b64_decoded[offset] << 24U) |
                             (b64_decoded[offset + 1] << 16U) |
                             (b64_decoded[offset + 2] << 8U) |
                             (b64_decoded[offset + 3]);
        offset += 4;
        MPA::Integer prime2 = construct_integer_from_bigendian_bytebuffer(b64_decoded, prime2_size, offset);
        // validate the private key components
        if (decryption_exponent != MPA::modular_inverse(encryption_exponent, (prime1 - 1) * (prime2 - 1)) ||
            prime1 * prime2 != modulus || MPA::modular_inverse(prime2, prime1) != coefficient ||
            !MPA::is_probably_prime(prime1) || !MPA::is_probably_prime(prime2))
        {
            std::cerr << "ERROR: bad private key, required component relations don't hold !\n";
            return false;
        }
        rsa = RSA(modulus, prime1, prime2, encryption_exponent, decryption_exponent);
        return true;
    }
    // handle DER key format
    // parse sequence header
    uint8_t sequence_type = b64_decoded[offset];
    offset += 1;
    if (sequence_type != 0x30)
    {
        std::cerr << "bad sequence tag\n";
        return false;
    }
    uint8_t sequence_length_type = b64_decoded[offset];
    offset += 1;
    if (sequence_length_type != 0x82)
    {
        std::cerr << "bad sequence length type\n";
        return false;
    }
    size_t sequence_length = (b64_decoded[offset] << 8U) | b64_decoded[offset + 1];
    offset += 2;
    // parse version
    size_t version_length = DER_read_length(offset, b64_decoded);
    if (version_length > 1 || b64_decoded[offset] != 0)
    {
        std::cerr << "unsupported version: " << (int)b64_decoded[offset] << "\n";
        return false;
    }
    offset += 1;
    // parse modulus -> n
    size_t modulus_length = DER_read_length(offset, b64_decoded);
    MPA::Integer modulus = construct_integer_from_bigendian_bytebuffer(b64_decoded, modulus_length, offset);
    // parse encryption exponent -> e
    size_t encryption_exponent_length = DER_read_length(offset, b64_decoded);
    if (encryption_exponent_length > 4)
    {
        std::cerr << "unexpected encryption exponent length: " << (int)encryption_exponent_length << "\n";
        return false;
    }
    MPA::Integer encryption_exponent = construct_integer_from_bigendian_bytebuffer(b64_decoded, encryption_exponent_length, offset);
    // parse decryption exponent -> d
    size_t decryption_exponent_length = DER_read_length(offset, b64_decoded);
    MPA::Integer decryption_exponent = construct_integer_from_bigendian_bytebuffer(b64_decoded, decryption_exponent_length, offset);
    // parse prime 1 -> p
    size_t prime1_length = DER_read_length(offset, b64_decoded);
    MPA::Integer prime1 = construct_integer_from_bigendian_bytebuffer(b64_decoded, prime1_length, offset);
    // parse prime 2 -> q
    size_t prime2_length = DER_read_length(offset, b64_decoded);
    MPA::Integer prime2 = construct_integer_from_bigendian_bytebuffer(b64_decoded, prime2_length, offset);
    // parse exponent 1 -> d mod (p-1)
    size_t exponent1_length = DER_read_length(offset, b64_decoded);
    MPA::Integer exponent1 = construct_integer_from_bigendian_bytebuffer(b64_decoded, exponent1_length, offset);
    // parse exponent 2 -> d mod (q-1)
    size_t exponent2_length = DER_read_length(offset, b64_decoded);
    MPA::Integer exponent2 = construct_integer_from_bigendian_bytebuffer(b64_decoded, exponent2_length, offset);
    // parse coefficient -> q^-1 mod p
    size_t coefficient_length = DER_read_length(offset, b64_decoded);
    MPA::Integer coefficient = construct_integer_from_bigendian_bytebuffer(b64_decoded, coefficient_length, offset);
    // validate the RSA key
    if (offset != b64_decoded.size() || offset != sequence_length + 4)
    {
        std::cerr << "error: bad sequence length or unexpected padding\n";
        return false;
    }
    if (modulus != prime1 * prime2)
    {
        std::cerr << "bad private key: modulus does not match p and q!\n";
        return false;
    }
    if (!MPA::is_probably_prime(prime1) || !MPA::is_probably_prime(prime2))
    {
        std::cerr << "bad private key: p or q are not prime !\n";
        return false;
    }
    if (exponent1 != decryption_exponent % (prime1 - 1) || exponent2 != decryption_exponent % (prime2 - 1))
    {
        std::cerr << "bad private key: decryption exponent relations don't hold!\n";
        return false;
    }
    if (MPA::modular_inverse(prime2, prime1) != coefficient)
    {
        std::cerr << "bad private key: coefficient relations don't hold!\n";
        return false;
    }
    rsa = RSA(modulus, prime1, prime2, encryption_exponent, decryption_exponent);
    return true;
}

bool read_rsa_public_key_file(const std::string &filepath)
{
    std::ifstream file(filepath);
    std::string line;
    if (!file.is_open())
    {
        std::cerr << "ERROR! Unable to open rsa public key file!\n";
        return false;
    }
    std::vector<uint8_t> tmp;
    while (getline(file, line))
    {
        for (const auto character : line)
        {
            tmp.push_back(character);
        }
    }
    std::vector<uint8_t> b64;
    size_t i;
    for (i = 0; i < tmp.size(); ++i)
    {
        if (tmp[i] == ' ')
        {
            break;
        }
    }
    for (size_t j = i + 1; j < tmp.size(); ++j)
    {
        if (tmp[j] == ' ')
        {
            break;
        }
        b64.push_back(tmp[j]);
    }
    MPA::Integer<word_t> modulus, exponent;
    bool success = parse_rsa_public_key(b64, exponent, modulus);
    if (success)
    {
        std::cout << "<<<RSA PUBLIC KEY DETAIL START>>>\n\n";
        std::cout
            << "encryption exponent:\n"
            << exponent << "\n";
        std::cout << "modulus:\n"
                  << modulus << "\n";
        std::cout << "<<<RSA PUBLIC KEY DETAIL END>>>\n\n";
        return true;
    }
    std::cerr << "ERROR: cannot parse public key\n";
    return false;
}

bool read_rsa_private_key_file(const std::string &filepath)
{
    std::ifstream file(filepath);
    std::string line;
    if (!file.is_open())
    {
        std::cerr << "ERROR! Unable to open rsa private key file!\n";
        return false;
    }
    std::vector<uint8_t> b64;
    while (getline(file, line))
    {
        if (line.find("-----BEGIN") != std::string::npos && line.find("PRIVATE KEY-----") != std::string::npos)
        {
            continue;
        }
        if (line.find("-----END") != std::string::npos && line.find("PRIVATE KEY-----") != std::string::npos)
        {
            continue;
        }
        for (const auto character : line)
        {
            b64.push_back(character);
        }
    }
    RSA rsa;
    bool success = parse_rsa_private_key(b64, rsa);
    if (success)
    {
        std::cout << rsa << "\n";
        return true;
    }
    std::cerr << "ERROR: cannot parse private key\n";
    return false;
}

bool read_rsa_key_file(const std::string &filepath)
{
    std::ifstream file(filepath);
    std::string line;
    if (!file.is_open())
    {
        std::cerr << "ERROR! Unable to open rsa key file!\n";
        return false;
    }
    getline(file, line);
    file.close();
    if (line.find("PRIVATE KEY") != std::string::npos)
    {
        bool success = read_rsa_private_key_file(filepath);
        return success;
    }
    else
    {
        bool success = read_rsa_public_key_file(filepath);
        return success;
    }
}

void show_usage()
{
    std::cerr << "USAGE: 1) <path-to-rsa-tool> generate <bitlength>\n";
    std::cerr << "          to generate a RSA key with 'bitlength' bits\n\n";
    std::cerr << "       2) <path-to-rsa-tool> parse <filepath>\n";
    std::cerr << "          to parse a RSA public or private key at <filepath> \n";
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        show_usage();    
        return 1;
    }
    std::string generate = "generate";
    std::string parse = "parse";
    // check if we generate a key
    if (argv[1] == generate)
    {
        const size_t bitlength = std::stoull(argv[2]);
        if (bitlength < 512)
        {
            std::cerr << "Provided bitlength " << bitlength << " is too short.\n";
            std::cerr << "Must be at least 512. Abort.\n";
            return 1;
        }

        const std::string private_key_file_name = "example.rsa";
        const std::string public_key_file_name = "example.rsa.pub";

        std::cout << "generating rsa key\n";
        std::cout << "bitlength: " << bitlength << "\n";
        RSA rsa = generate_rsa_key(bitlength);

        std::cout << "\nwriting private key to: " << private_key_file_name << "\n";
        size_t bytes_written = rsa.write_private_key(private_key_file_name);
        std::cout << "wrote " << bytes_written << " bytes in total\n";

        std::error_code ec;
        std::filesystem::permissions("example.rsa", std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, ec);
        if (ec)
        {
            std::cerr << "ERROR trying to set permissions for RSA private key file 'example.rsa'.\n";
            std::cerr << "ERROR CODE: " << ec.value() << "\n";
            return 1;
        }

        std::cout << "\nwriting public key to: " << public_key_file_name << "\n";
        bytes_written = rsa.write_ssh_public_key(public_key_file_name);
        std::cout << "wrote " << bytes_written << " bytes in total\n\n";
        return 0;
    }
    // or if we read a key
    else if (argv[1] == parse)
    {
        std::string filepath = argv[2];
        bool success = read_rsa_key_file(filepath);
        return success ? 0 : 1;
    }
    // if not, show usage
    else
    {
        show_usage();
        return 1;
    }
}
