/*
    COPYRIGHT: David Geis 2025
    LICENSE:   MIT
    CONTACT:   davidgeis@web.de
*/

#pragma once

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>
#include <sstream>
#include <string>
#include <string.h>
#include <vector>

#define MPA_SHIFTBASE 1ULL

#ifndef MPA_DIVMOD_BUFFER_SIZE
#define MPA_DIVMOD_BUFFER_SIZE 2048 * 8
#endif

#ifndef MPA_POWER_BUFFER_SIZE
#define MPA_POWER_BUFFER_SIZE 2048 * 8
#endif

#ifndef MPA_KARATSUBA_BUFFER_SIZE
#define MPA_KARATSUBA_BUFFER_SIZE 2048 * 8
#endif

namespace MPA
{
    namespace
    {
        constexpr size_t SIGN_OFF = std::numeric_limits<size_t>::max() - 1;
        constexpr size_t OWNERSHIP_OFF = std::numeric_limits<size_t>::max() - 2;
        constexpr size_t SIGN_OFF_OWNERSHIP_OFF = std::numeric_limits<size_t>::max() - 3;

        constexpr size_t divmod_buffer_size = MPA_DIVMOD_BUFFER_SIZE;
        thread_local uint8_t divmod_buffer[divmod_buffer_size] = {0};

        constexpr size_t power_buffer_size = MPA_POWER_BUFFER_SIZE;
        thread_local uint8_t power_buffer[power_buffer_size] = {0};

        constexpr size_t karatsuba_buffer_size = MPA_KARATSUBA_BUFFER_SIZE;
        thread_local uint8_t karatsuba_buffer[karatsuba_buffer_size] = {0};
        thread_local size_t karatsuba_buffer_offset = 0;

        template <typename word_t>
        struct BitsInWord
        {
            static constexpr size_t value = sizeof(word_t) << 3;
        };

        template <typename word_t>
        struct MSB
        {
            static constexpr word_t value = MPA_SHIFTBASE << (BitsInWord<word_t>::value - 1);
        };

        template <typename word_t>
        struct BufferSize
        {
            static constexpr size_t divmod = divmod_buffer_size / sizeof(word_t);
            static constexpr size_t power = power_buffer_size / sizeof(word_t);
            static constexpr size_t karatsuba = karatsuba_buffer_size / sizeof(word_t);
        };

        template <typename word_t>
        size_t find_head(const word_t *l, const size_t start_point) noexcept
        { // clang-format off
            size_t i = start_point;
            for (i = start_point; !l[i] && i >= 1; --i);
            return i;
        } // clang-format on

        template <typename word_t>
        word_t *allocate_words(const size_t word_count) noexcept
        {
            word_t *buffer;
            if (!(buffer = (word_t *)(calloc(word_count, sizeof(word_t)))))
                std::cerr << "ERROR: OUT OF MEMORY, ABORT !\n", abort();
            return buffer;
        }

        template <typename word_t>
        void clear_words(word_t *dst, const size_t wordcount) noexcept
        {
            memset(dst, 0, wordcount * sizeof(word_t));
        }

        template <typename word_t>
        void copy_words(word_t *dst, const word_t *src, const size_t wordcount) noexcept
        {
            memcpy(dst, src, wordcount * sizeof(word_t));
        }

        template <typename word_t>
        void move_words(word_t *dst, const word_t *src, const size_t wordcount) noexcept
        {
            memmove(dst, src, wordcount * sizeof(word_t));
        }

        template <typename word_t>
        word_t add_overflow(const word_t a, const word_t b, word_t &overflow) noexcept
        {
            return (overflow = a > std::numeric_limits<word_t>::max() - b, a + b);
        }

        template <typename word_t>
        word_t sub_underflow(const word_t a, const word_t b, word_t &underflow) noexcept
        {
            return (underflow = a < b, a - b);
        }

        template <typename word_t>
        bool add_words(const word_t *bigger, const word_t *smaller, const size_t bigger_size, const size_t smaller_size,
                       word_t *total_sum) noexcept
        {
            word_t carry = 0;
            for (size_t i = 0; i < smaller_size; ++i)
            {
                word_t current_carry;
                const word_t sum = add_overflow(bigger[i], smaller[i], current_carry);
                total_sum[i] = add_overflow(sum, carry, carry);
                carry += current_carry;
            }
            for (size_t i = smaller_size; i < bigger_size; ++i)
                total_sum[i] = add_overflow(bigger[i], carry, carry);
            total_sum[bigger_size] = carry;
            return carry;
        }

        template <typename word_t>
        void subtract_words(const word_t *bigger, const word_t *smaller, const size_t bigger_head, const size_t smaller_head,
                            word_t *total_diff, size_t &total_diff_head) noexcept
        {
            word_t carry = 0;
            total_diff_head = 0; // safe guard !
            for (size_t i = 0; i <= smaller_head; ++i)
            {
                word_t current_carry;
                const word_t diff = sub_underflow(bigger[i], smaller[i], current_carry);
                total_diff_head = (total_diff[i] = sub_underflow(diff, carry, carry)) ? i : total_diff_head;
                carry += current_carry;
            }
            for (size_t i = smaller_head + 1; i <= bigger_head; ++i)
                total_diff_head = (total_diff[i] = sub_underflow(bigger[i], carry, carry)) ? i : total_diff_head;
        }

        template <typename word_t>
        bool l_abs_geq_r_abs(const word_t *l, const word_t *r, const size_t l_head, const size_t r_head) noexcept
        { // clang-format off
            if (l_head < r_head)
                return false;
            if (l_head > r_head)
                return true;
            size_t i = 0;
            for (i = l_head; i < l_head + 1 && l[i] == r[i]; --i);
            return i < l_head + 1 ? l[i] > r[i] : true;
        } // clang-format on

#define DO_ADD(l, r, bigger_head, smaller_head)                                                                 \
    add_words(bigger_head == l.get_head() ? l.words : r.words, bigger_head == l.get_head() ? r.words : l.words, \
              bigger_head + 1, smaller_head + 1, out_words);                                                    \
    const size_t out_head = out_words[bigger_head + 1] ? bigger_head + 1 : bigger_head;                         \
    const bool out_sign = l.is_negative();                                                                      \
    return out_sign | (out_head << 2U) // no ownership

#define DO_SUB(l, r, bigger_head, smaller_head)                                                                    \
    const bool l_geq_r = l_abs_geq_r_abs(l.words, r.words, l.get_head(), r.get_head());                            \
    size_t out_head = 0;                                                                                           \
    subtract_words(l_geq_r ? l.words : r.words, l_geq_r ? r.words : l.words, bigger_head, smaller_head, out_words, \
                   out_head);                                                                                      \
    bool out_sign;                                                                                                 \
    if (!out_head && !out_words[0])                                                                                \
    {                                                                                                              \
        out_sign = 0;                                                                                              \
    }                                                                                                              \
    else if (!l.is_negative())                                                                                     \
    {                                                                                                              \
        out_sign = !l_geq_r;                                                                                       \
    }                                                                                                              \
    else                                                                                                           \
    {                                                                                                              \
        out_sign = l_geq_r;                                                                                        \
    }                                                                                                              \
    return out_sign | (out_head << 2U) // no ownership

        template <typename word_t>
        void inplace_decrement(word_t *minuend, const word_t *subtrahend, const size_t subtrahend_size)
        {
            word_t carry = 0;
            size_t j = 0;
            for (j = 0; j < subtrahend_size; ++j)
            {
                word_t current_carry;
                const word_t diff = sub_underflow(minuend[j], subtrahend[j], current_carry);
                minuend[j] = sub_underflow(diff, carry, carry);
                carry += current_carry;
            }
            while (carry)
                j += (minuend[j] = sub_underflow(minuend[j], carry, carry), 1);
        }

        template <typename word_t>
        void inplace_increment(word_t *final_sum, const word_t *summand, const size_t summand_size) noexcept
        {
            word_t carry = 0;
            size_t j = 0;
            for (j = 0; j < summand_size; ++j)
            {
                word_t current_carry;
                word_t sum = add_overflow(final_sum[j], summand[j], current_carry);
                final_sum[j] = add_overflow(sum, carry, carry);
                carry += current_carry;
            }
            while (carry)
                j += (final_sum[j] = add_overflow(final_sum[j], carry, carry), 1);
        }

        bool validate_input_string(const std::string &input, int base) noexcept
        {
            if (base != 2 && base != 10 && base != 16)
                return std::cerr << "WARNING: base " << base << " is not supported.\n", false;
            const size_t start_index = [&base, &input]()
            {
                if (base != 10)
                    return input[0] == '-' ? 3 : 2;
                return input[0] == '-' ? 1 : 0;
            }();
            if (start_index >= input.size())
                return false;
            for (size_t i = start_index; i < input.size(); ++i)
            {
                const char current_digit = input[i];
                switch (base)
                {
                case 2:
                    if (current_digit != '0' && current_digit != '1')
                        return false;
                    break;
                case 10:
                    if (current_digit != '0' && current_digit != '1' && current_digit != '2' && current_digit != '3' &&
                        current_digit != '4' && current_digit != '5' && current_digit != '6' && current_digit != '7' &&
                        current_digit != '8' && current_digit != '9')
                        return false;
                    break;
                case 16:
                    if (current_digit != '0' && current_digit != '1' && current_digit != '2' && current_digit != '3' &&
                        current_digit != '4' && current_digit != '5' && current_digit != '6' && current_digit != '7' &&
                        current_digit != '8' && current_digit != '9' && current_digit != 'a' && current_digit != 'b' &&
                        current_digit != 'c' && current_digit != 'd' && current_digit != 'e' && current_digit != 'f' &&
                        current_digit != 'A' && current_digit != 'B' && current_digit != 'C' && current_digit != 'D' &&
                        current_digit != 'E' && current_digit != 'F')
                        return false;
                    break;
                default:
                    break;
                }
            }
            return true;
        }

        template <typename word_t>
        struct DWord
        {
            static_assert(false, "unsupported wordtype !");
        };

#if defined(__SIZEOF_INT128__) && !defined(_MSC_VER)
        template <>
        struct DWord<uint64_t>
        {
            using type = __uint128_t;
        };
#endif

        template <>
        struct DWord<uint32_t>
        {
            using type = uint64_t;
        };

        template <>
        struct DWord<uint16_t>
        {
            using type = uint32_t;
        };

        template <typename word_t>
        void multiply_by_doubleword(const word_t *l, const word_t *r, size_t r_size, word_t *out) noexcept
        {
            using dword_t = typename DWord<word_t>::type;
            dword_t x;
            const dword_t l_low = l[0];
            const dword_t l_high = l[1];
            for (size_t i = 0; i < r_size; i += 2)
            {
                x = l_low * r[i] + out[i];
                out[i] = x & std::numeric_limits<word_t>::max();
                x = l_high * r[i] + (x >> BitsInWord<word_t>::value) + out[i + 1];
                out[i + 1] = x & std::numeric_limits<word_t>::max();
                out[i + 2] = x >> BitsInWord<word_t>::value;
                // unroll once
                x = l_low * r[i + 1] + out[i + 1];
                out[i + 1] = x & std::numeric_limits<word_t>::max();
                x = l_high * r[i + 1] + (x >> BitsInWord<word_t>::value) + out[i + 2];
                out[i + 2] = x & std::numeric_limits<word_t>::max();
                out[i + 3] = x >> BitsInWord<word_t>::value;
            }
        }

        template <typename word_t>
        void multiply_by_word(const word_t l, const word_t *r, const size_t rsize, word_t *out) noexcept
        {
            using dword_t = typename DWord<word_t>::type;
            const dword_t ll = l;
            for (size_t i = 0; i < rsize; ++i)
            {
                dword_t x = ll * r[i] + out[i];
                out[i] = x & std::numeric_limits<word_t>::max();
                out[i + 1] = x >> BitsInWord<word_t>::value;
            }
        }

        constexpr size_t multable_max_wordsize = 18;

        template <typename word_t>
        struct Multable
        {
            typedef void (*mulfunction)(const word_t *l, const word_t *r, word_t *out);

            template <size_t lsize, size_t rsize>
            static void multiply_words(const word_t *l, const word_t *r, word_t *out) noexcept
            {
                using dword_t = typename DWord<word_t>::type;
                for (size_t i = 0; i < rsize; ++i)
                {
                    dword_t x = 0;
                    for (size_t j = 0; j < lsize; ++j)
                        out[i + j] = (x = (dword_t)r[i] * l[j] + (x >> BitsInWord<word_t>::value) + out[i + j]) &
                                     std::numeric_limits<word_t>::max();
                    out[i + lsize] = x >> BitsInWord<word_t>::value;
                }
            }

            static constexpr mulfunction funcs[multable_max_wordsize * (multable_max_wordsize + 1) >> 1U] = {
                multiply_words<1, 1>, // clang-format off
                multiply_words<2, 1>,  multiply_words<2, 2>,
                multiply_words<3, 1>,  multiply_words<3, 2>,  multiply_words<3, 3>,
                multiply_words<4, 1>,  multiply_words<4, 2>,  multiply_words<4, 3>,  multiply_words<4, 4>,
                multiply_words<5, 1>,  multiply_words<5, 2>,  multiply_words<5, 3>,  multiply_words<5, 4>,  multiply_words<5, 5>,
                multiply_words<6, 1>,  multiply_words<6, 2>,  multiply_words<6, 3>,  multiply_words<6, 4>,  multiply_words<6, 5>,  multiply_words<6, 6>,
                multiply_words<7, 1>,  multiply_words<7, 2>,  multiply_words<7, 3>,  multiply_words<7, 4>,  multiply_words<7, 5>,  multiply_words<7, 6>,  multiply_words<7, 7>,
                multiply_words<8, 1>,  multiply_words<8, 2>,  multiply_words<8, 3>,  multiply_words<8, 4>,  multiply_words<8, 5>,  multiply_words<8, 6>,  multiply_words<8, 7>,  multiply_words<8, 8>,
                multiply_words<9, 1>,  multiply_words<9, 2>,  multiply_words<9, 3>,  multiply_words<9, 4>,  multiply_words<9, 5>,  multiply_words<9, 6>,  multiply_words<9, 7>,  multiply_words<9, 8>,  multiply_words<9, 9>,
                multiply_words<10, 1>, multiply_words<10, 2>, multiply_words<10, 3>, multiply_words<10, 4>, multiply_words<10, 5>, multiply_words<10, 6>, multiply_words<10, 7>, multiply_words<10, 8>, multiply_words<10, 9>, multiply_words<10, 10>,
                multiply_words<11, 1>, multiply_words<11, 2>, multiply_words<11, 3>, multiply_words<11, 4>, multiply_words<11, 5>, multiply_words<11, 6>, multiply_words<11, 7>, multiply_words<11, 8>, multiply_words<11, 9>, multiply_words<11, 10>, multiply_words<11, 11>,
                multiply_words<12, 1>, multiply_words<12, 2>, multiply_words<12, 3>, multiply_words<12, 4>, multiply_words<12, 5>, multiply_words<12, 6>, multiply_words<12, 7>, multiply_words<12, 8>, multiply_words<12, 9>, multiply_words<12, 10>, multiply_words<12, 11>, multiply_words<12, 12>,
                multiply_words<13, 1>, multiply_words<13, 2>, multiply_words<13, 3>, multiply_words<13, 4>, multiply_words<13, 5>, multiply_words<13, 6>, multiply_words<13, 7>, multiply_words<13, 8>, multiply_words<13, 9>, multiply_words<13, 10>, multiply_words<13, 11>, multiply_words<13, 12>, multiply_words<13, 13>,
                multiply_words<14, 1>, multiply_words<14, 2>, multiply_words<14, 3>, multiply_words<14, 4>, multiply_words<14, 5>, multiply_words<14, 6>, multiply_words<14, 7>, multiply_words<14, 8>, multiply_words<14, 9>, multiply_words<14, 10>, multiply_words<14, 11>, multiply_words<14, 12>, multiply_words<14, 13>, multiply_words<14, 14>,
                multiply_words<15, 1>, multiply_words<15, 2>, multiply_words<15, 3>, multiply_words<15, 4>, multiply_words<15, 5>, multiply_words<15, 6>, multiply_words<15, 7>, multiply_words<15, 8>, multiply_words<15, 9>, multiply_words<15, 10>, multiply_words<15, 11>, multiply_words<15, 12>, multiply_words<15, 13>, multiply_words<15, 14>, multiply_words<15, 15>,
                multiply_words<16, 1>, multiply_words<16, 2>, multiply_words<16, 3>, multiply_words<16, 4>, multiply_words<16, 5>, multiply_words<16, 6>, multiply_words<16, 7>, multiply_words<16, 8>, multiply_words<16, 9>, multiply_words<16, 10>, multiply_words<16, 11>, multiply_words<16, 12>, multiply_words<16, 13>, multiply_words<16, 14>, multiply_words<16, 15>, multiply_words<16, 16>,
                multiply_words<17, 1>, multiply_words<17, 2>, multiply_words<17, 3>, multiply_words<17, 4>, multiply_words<17, 5>, multiply_words<17, 6>, multiply_words<17, 7>, multiply_words<17, 8>, multiply_words<17, 9>, multiply_words<17, 10>, multiply_words<17, 11>, multiply_words<17, 12>, multiply_words<17, 13>, multiply_words<17, 14>, multiply_words<17, 15>, multiply_words<17, 16>, multiply_words<17, 17>,
                multiply_words<18, 1>, multiply_words<18, 2>, multiply_words<18, 3>, multiply_words<18, 4>, multiply_words<18, 5>, multiply_words<18, 6>, multiply_words<18, 7>, multiply_words<18, 8>, multiply_words<18, 9>, multiply_words<18, 10>, multiply_words<18, 11>, multiply_words<18, 12>, multiply_words<18, 13>, multiply_words<18, 14>, multiply_words<18, 15>, multiply_words<18, 16>, multiply_words<18, 17>, multiply_words<18, 18>,
            }; // clang-format on
        };

        /*
          basic strategy: prod = z0 + B^m * z1 + B^2m * z2
            where z0 = l_low * r_low                                    --> first recursion
                  z1 = l_low * r_high + r_high * l_low
                     = (l_low + l_high) * (r_low + r_high) - z0 - z2    --> second recursion
                  z2 = l_high * r_high                                  --> third recursion
                  B  = 2^64
                  m  = max(lsize, rsize) / 2
        */
        template <typename word_t>
        void multiply_karatsuba(const word_t *l, const word_t *r, const size_t lsize, const size_t rsize, word_t *out) noexcept
        {
            static constexpr uint8_t index_lookup[19] = {0, 0, 1, 3, 6, 10, 15, 21, 28, 36,
                                                         45, 55, 66, 78, 91, 105, 120, 136, 153};
            if (lsize <= multable_max_wordsize && rsize <= multable_max_wordsize)
                return lsize >= rsize ? Multable<word_t>::funcs[index_lookup[lsize] + rsize - 1](l, r, out)
                                      : Multable<word_t>::funcs[index_lookup[rsize] + lsize - 1](r, l, out);
            const size_t m = lsize > rsize ? lsize >> 1U : rsize >> 1U;
            const bool lsize_short = lsize <= m;
            const bool rsize_short = rsize <= m;
            const size_t l_high_size = !lsize_short ? lsize - m : 0;
            const size_t l_low_size = !lsize_short ? m : lsize;
            const word_t *l_high = !lsize_short ? l + m : nullptr;
            const size_t r_high_size = !rsize_short ? rsize - m : 0;
            const size_t r_low_size = !rsize_short ? m : rsize;
            const word_t *r_high = !rsize_short ? r + m : nullptr;
            const word_t *l_low = l;
            const word_t *r_low = r;
            const bool check_l = l_low_size > l_high_size;
            const bool check_r = r_low_size > r_high_size;
            size_t sum_l_size = check_l ? l_low_size + 1 : l_high_size + 1;
            size_t sum_r_size = check_r ? r_low_size + 1 : r_high_size + 1;
            size_t z1_size = sum_l_size + sum_r_size;
            const size_t needed_scratch_words = z1_size << 1U;
            const bool dont_require_allocation = BufferSize<word_t>::karatsuba > needed_scratch_words + karatsuba_buffer_offset;
            word_t *z0 = out;
            word_t *tmp;
            word_t *z1 = dont_require_allocation ? (tmp = ((word_t *)karatsuba_buffer) + karatsuba_buffer_offset,
                                                    clear_words(tmp, needed_scratch_words), karatsuba_buffer_offset += needed_scratch_words, tmp)
                                                 : allocate_words<word_t>(needed_scratch_words);
            word_t *z2 = l_high && r_high ? out + 2 * m : nullptr;
            word_t *sum_l = z1 + z1_size;
            word_t *sum_r = sum_l + sum_l_size;
            multiply_karatsuba(l_low, r_low, l_low_size, r_low_size, z0);
            const bool carry_l = add_words(!check_l ? l_high : l_low, !check_l ? l_low : l_high,
                                           !check_l ? l_high_size : l_low_size, !check_l ? l_low_size : l_high_size, sum_l);
            const bool carry_r = add_words(!check_r ? r_high : r_low, !check_r ? r_low : r_high,
                                           !check_r ? r_high_size : r_low_size, !check_r ? r_low_size : r_high_size, sum_r);
            z1_size -= !carry_r + !carry_l, sum_l_size -= !carry_l, sum_r_size -= !carry_r;
            multiply_karatsuba(sum_l, sum_r, sum_l_size, sum_r_size, z1), inplace_decrement(z1, z0, l_low_size + r_low_size);
            z2 ? (multiply_karatsuba(l_high, r_high, l_high_size, r_high_size, z2), inplace_decrement(z1, z2, l_high_size + r_high_size),
                  inplace_increment(out + m, z1, lsize + rsize - m < z1_size ? lsize + rsize - m : z1_size))
               : inplace_increment(out + m, z1, lsize + rsize - m < z1_size ? lsize + rsize - m : z1_size);
            dont_require_allocation ? (void)(karatsuba_buffer_offset -= needed_scratch_words) : free(z1);
        }

        /*
          basic strategy: prod = z0 + B^m * z1 + B^2m * z2
            where z0 = l_low^2                          --> first recursion
                  z1 = l_low * l_high + l_high * l_low
                     = (l_low + l_high)^2 - z0 - z2     --> second recursion
                  z2 = l_high^2                         --> third recursion
                  B  = 2^64
                  m  = lsize / 2
        */
        template <typename word_t>
        void square_karatsuba(const word_t *l, const size_t lsize, word_t *out) noexcept
        {
            static constexpr uint8_t index_lookup[19] = {0, 0, 2, 5, 9, 14, 20, 27, 35, 44,
                                                         54, 65, 77, 90, 104, 119, 135, 152, 170};
            if (lsize <= multable_max_wordsize)
                return Multable<word_t>::funcs[index_lookup[lsize]](l, l, out);
            const size_t m = lsize >> 1U;
            const size_t l_high_size = lsize - m;
            const size_t l_low_size = m;
            const word_t *l_high = l + m;
            const word_t *l_low = l;
            size_t sum_l_size = l_high_size + 1;
            const size_t needed_scratch_words = 4 * sum_l_size;
            const bool dont_require_allocation = BufferSize<word_t>::karatsuba > needed_scratch_words + karatsuba_buffer_offset;
            word_t *z0 = out;
            word_t *tmp;
            word_t *z1 = dont_require_allocation ? (tmp = ((word_t *)karatsuba_buffer) + karatsuba_buffer_offset,
                                                    clear_words(tmp, needed_scratch_words), karatsuba_buffer_offset += needed_scratch_words, tmp)
                                                 : allocate_words<word_t>(needed_scratch_words);
            word_t *z2 = out + 2 * m;
            size_t z1_size = 2 * sum_l_size;
            word_t *sum_l = z1 + z1_size;
            square_karatsuba(l_low, l_low_size, z0);
            const bool carry_l = add_words(l_high, l_low, l_high_size, l_low_size, sum_l);
            z1_size -= 2 * (!carry_l), sum_l_size -= !carry_l;
            square_karatsuba(sum_l, sum_l_size, z1), inplace_decrement(z1, z0, 2 * l_low_size);
            square_karatsuba(l_high, l_high_size, z2), inplace_decrement(z1, z2, 2 * l_high_size);
            inplace_increment(out + m, z1, 2 * lsize - m < z1_size ? 2 * lsize - m : z1_size);
            dont_require_allocation ? (void)(karatsuba_buffer_offset -= needed_scratch_words) : free(z1);
        }

        template <typename word_t>
        size_t get_leading_zero_bits(word_t A) noexcept
        {
            if (!A)
                return BitsInWord<word_t>::value;
            size_t count = 0;
            while (A < (MPA_SHIFTBASE << (BitsInWord<word_t>::value - 1)))
                count += (A <<= 1U, 1);
            return count;
        }

        template <typename word_t>
        size_t get_trailing_zero_bits(word_t A) noexcept
        {
            if (!A)
                return 0;
            size_t count = 0;
            while (!(A & 1))
                count += (A >>= 1U, 1);
            return count;
        }

        template <typename word_t>
        size_t get_trailing_zero_bits(const word_t *word_ptr, const size_t head) noexcept
        {
            if (!word_ptr[head])
                return 0;
            size_t count = 0;
            word_t d = 0;
            for (size_t i = 0; !d && i <= head; ++i)
                count += (d = word_ptr[i]) ? 0 : BitsInWord<word_t>::value;
            count += get_trailing_zero_bits(d);
            return count;
        }

        template <typename word_t>
        size_t shift_left_by_words_and_bits(word_t *in_words, size_t in_head, const size_t bits_shift, const size_t words_shift,
                                            word_t *out_words) noexcept
        {
            if (bits_shift)
            {
                word_t c = 0;
                for (size_t i = 0; i <= in_head; ++i)
                {
                    const word_t tmp = in_words[i];
                    out_words[i] = (tmp << bits_shift) | c;
                    c = tmp >> (BitsInWord<word_t>::value - bits_shift);
                }
                if (c)
                    in_head += (out_words[in_head + 1] = c, 1);
                move_words(out_words + words_shift, out_words, in_head + 1);
                clear_words(out_words, words_shift);
            }
            else // no need to call "clear_words" here, because we haven't written anything to out_words
                move_words(out_words + words_shift, in_words, in_head + 1);
            return in_head + words_shift;
        }

        template <typename word_t>
        bool compare_words(const word_t *left, const word_t *right, const size_t size) noexcept
        { // clang-format off
            size_t i = size - 1;
            for (i = size - 1; i < size && left[i] == right[i]; --i);
            return i < size ? left[i] > right[i] : false;
        } // clang-format on

        template <typename word_t>
        size_t divmod(word_t *l_words, const size_t l_head, word_t *y_words, const size_t y_head, word_t *output,
                      word_t *workspace, const size_t K, // requirement: K >= l_head + 5
                      bool need_remainder = false) noexcept
        {
            if (l_head < y_head)
                return (need_remainder) ? (copy_words(output, l_words, l_head + 1), l_head << 2U) : (*output = 0, 0);
            using dword_t = typename DWord<word_t>::type;
            const auto div_two_doublewords_by_one_doubleword = [](const dword_t &AH, const dword_t &AL, const dword_t &B, word_t *q)
            {
                bool overflow;
                const dword_t overflow_barrier =
                    ((std::numeric_limits<word_t>::max()) | (dword_t)(std::numeric_limits<word_t>::max()) << BitsInWord<word_t>::value) - B;
                const word_t b1 = B >> BitsInWord<word_t>::value;
                dword_t q_tmp = AH / b1;
                dword_t D = q_tmp * (B & std::numeric_limits<word_t>::max());
                dword_t tmp = (AL >> BitsInWord<word_t>::value) | ((AH - q_tmp * b1) << BitsInWord<word_t>::value);
                if (tmp < D)
                    overflow = tmp > overflow_barrier, q_tmp -= 1, tmp += B, tmp += (!overflow && tmp < D) ? (q_tmp -= 1, B) : 0;
                const dword_t R = tmp - D;
                q[1] = q_tmp, q_tmp = R / b1;
                D = q_tmp * (B & std::numeric_limits<word_t>::max());
                tmp = (AL & std::numeric_limits<word_t>::max()) | ((R - q_tmp * b1) << BitsInWord<word_t>::value);
                if (tmp < D)
                    overflow = tmp > overflow_barrier, q_tmp -= 1, tmp += B, q_tmp -= (!overflow && tmp < D);
                q[0] = q_tmp;
            };
            const size_t backshift =
                y_head & 1 ? get_leading_zero_bits(y_words[y_head]) : BitsInWord<word_t>::value + get_leading_zero_bits(y_words[y_head]);
            const size_t backshift_words = backshift / BitsInWord<word_t>::value;
            const size_t backshift_bits = backshift - backshift_words * BitsInWord<word_t>::value;
            word_t *remainder_ptr = need_remainder ? output : workspace;
            word_t *quot_ptr = need_remainder ? workspace : output;
            size_t n = shift_left_by_words_and_bits(l_words, l_head, backshift_bits, backshift_words, remainder_ptr) + 1;
            n += (n & 1);
            const size_t t = y_head + backshift_words + 1;
            const size_t nn = (n >> 1) - 1;
            const size_t tt = (t >> 1) - 1;
            const size_t offset = n - t;
            word_t *shifted_yabs_ptr = workspace + K;
            word_t *remainder_correction_ptr = workspace + 2 * K;
            word_t *shifted_remainder_correction_ptr = remainder_correction_ptr + offset;
            shift_left_by_words_and_bits(y_words, y_head, backshift_bits, backshift_words, shifted_yabs_ptr + offset);
            const word_t *initial_yabs_ptr = shifted_yabs_ptr + offset;
            size_t remainder_correction_size = n;
            size_t shifted_yabs_size = t + offset;
            const dword_t divisor = initial_yabs_ptr[t - 2] | ((dword_t)initial_yabs_ptr[t - 1] << BitsInWord<word_t>::value);
            word_t y_checker_words[] = {t > 2 ? initial_yabs_ptr[t - 4] : (word_t)0, t > 2 ? initial_yabs_ptr[t - 3] : (word_t)0,
                                        initial_yabs_ptr[t - 2], initial_yabs_ptr[t - 1]};
            bool check = !compare_words(shifted_yabs_ptr, remainder_ptr, n);
            quot_ptr[offset] += check;
            inplace_decrement(remainder_ptr, shifted_yabs_ptr, check ? shifted_yabs_size : 0);
            const size_t words_to_clear = remainder_correction_size - offset + 2;
            const size_t loop_bound = !tt ? 1 : tt;
            size_t i;
            const auto mul_4_by_2 = Multable<word_t>::funcs[7];
            for (i = nn; i > loop_bound; --i)
            {
                word_t q_words[2] = {std::numeric_limits<word_t>::max(), std::numeric_limits<word_t>::max()};
                //  get an estimate for current quotient double-word
                //  note: it will never be less than the actual value
                if ((remainder_ptr[2 * i] | ((dword_t)remainder_ptr[2 * i + 1] << BitsInWord<word_t>::value)) != divisor)
                    div_two_doublewords_by_one_doubleword(
                        remainder_ptr[2 * i] | ((dword_t)remainder_ptr[2 * i + 1] << BitsInWord<word_t>::value),
                        remainder_ptr[2 * i - 2] | ((dword_t)remainder_ptr[2 * i - 1] << BitsInWord<word_t>::value), divisor, q_words);
                // first pass of adjusting the estimate
                word_t estimate_checker_words[6] = {0};
                mul_4_by_2(y_checker_words, q_words, estimate_checker_words);
                bool comp = compare_words(estimate_checker_words, remainder_ptr + 2 * i - 4, 6);
                bool underflow = q_words[0] < comp;
                q_words[0] -= comp, q_words[1] -= underflow;
                inplace_decrement(estimate_checker_words, y_checker_words, comp ? 4 : 0);
                comp = compare_words(estimate_checker_words, remainder_ptr + 2 * i - 4, 6);
                underflow = q_words[0] < comp;
                q_words[0] -= comp, q_words[1] -= underflow;
                shifted_yabs_size -= 2, shifted_yabs_ptr += 2, shifted_remainder_correction_ptr -= 2;
                clear_words(shifted_remainder_correction_ptr, words_to_clear);
                multiply_by_doubleword(q_words, initial_yabs_ptr, t, shifted_remainder_correction_ptr);
                // second pass of adjusting the estimate
                size_t j; // clang-format off
                for (j = 0; j < words_to_clear && remainder_ptr[remainder_correction_size - 1 - j]
                     == remainder_correction_ptr[remainder_correction_size - 1 - j]; ++j); // clang-format on
                if (j < words_to_clear && remainder_ptr[remainder_correction_size - 1 - j] <
                                              remainder_correction_ptr[remainder_correction_size - 1 - j])
                {
                    underflow = !q_words[0], q_words[0] -= 1, q_words[1] -= underflow;
                    inplace_decrement(remainder_correction_ptr, shifted_yabs_ptr, shifted_yabs_size);
                }
                // finally, set the quotient words
                copy_words(quot_ptr + 2 * (i - tt - 1), q_words, q_words[1] ? 2 : 1);
                // and update remainder
                inplace_decrement(remainder_ptr, remainder_correction_ptr, remainder_correction_size);
                remainder_correction_size -= 2;
            }
            if (i == 1 && !tt) // i == 1 is handled seperately to avoid branching in the 'main' loop
            {
                word_t q_words[2] = {std::numeric_limits<word_t>::max(), std::numeric_limits<word_t>::max()};
                //  get an estimate for current quotient double-word
                //  note: it will never be less than the actual value
                if ((remainder_ptr[2] | ((dword_t)remainder_ptr[3] << BitsInWord<word_t>::value)) != divisor)
                    div_two_doublewords_by_one_doubleword(remainder_ptr[2] | ((dword_t)remainder_ptr[3] << BitsInWord<word_t>::value),
                                                          remainder_ptr[0] | ((dword_t)remainder_ptr[1] << BitsInWord<word_t>::value),
                                                          divisor, q_words);
                // first pass of adjusting the estimate
                word_t estimate_checker_words[6] = {0};
                mul_4_by_2(y_checker_words, q_words, estimate_checker_words);
                word_t tmp_words[6] = {0, 0, remainder_ptr[0], remainder_ptr[1], remainder_ptr[2], remainder_ptr[3]};
                bool comp = compare_words(estimate_checker_words, tmp_words, 6);
                bool underflow = q_words[0] < comp;
                q_words[0] -= comp, q_words[1] -= underflow;
                inplace_decrement(estimate_checker_words, y_checker_words, comp ? 4 : 0);
                comp = compare_words(estimate_checker_words, tmp_words, 6);
                underflow = q_words[0] < comp;
                q_words[0] -= comp, q_words[1] -= underflow;
                shifted_yabs_size -= 2, shifted_yabs_ptr += 2, shifted_remainder_correction_ptr -= 2;
                clear_words(shifted_remainder_correction_ptr, words_to_clear);
                multiply_by_doubleword(q_words, initial_yabs_ptr, t, shifted_remainder_correction_ptr);
                // second pass of adjusting the estimate
                size_t j; // clang-format off
                for (j = 0; j < words_to_clear && remainder_ptr[remainder_correction_size - 1 - j] ==
                        remainder_correction_ptr[remainder_correction_size - 1 - j]; ++j); // clang-format on
                if (j < words_to_clear && remainder_ptr[remainder_correction_size - 1 - j] <
                                              remainder_correction_ptr[remainder_correction_size - 1 - j])
                {
                    underflow = !q_words[0], q_words[0] -= 1, q_words[1] -= underflow;
                    inplace_decrement(remainder_correction_ptr, shifted_yabs_ptr, shifted_yabs_size);
                }
                // finally, set the quotient words
                copy_words(quot_ptr, q_words, q_words[1] ? 2 : 1);
                // and update remainder
                inplace_decrement(remainder_ptr, remainder_correction_ptr, remainder_correction_size);
            }
            if (need_remainder)
            {
                size_t remainder_head = find_head(remainder_ptr, K - 1);
                if (remainder_head + 1 <= backshift_words)
                    return (remainder_ptr[0] = 0, 0);
                size_t remainder_wc = remainder_head + 1 - backshift_words;
                move_words(remainder_ptr, remainder_ptr + backshift_words, remainder_wc);
                remainder_head = remainder_wc - 1;
                if (backshift_bits)
                {
                    word_t c = 0;
                    for (size_t i = remainder_head; i <= remainder_head; --i)
                    {
                        word_t tmp = remainder_ptr[i];
                        remainder_ptr[i] = (tmp >> backshift_bits) | c;
                        c = (tmp & ((MPA_SHIFTBASE << backshift_bits) - 1)) << (BitsInWord<word_t>::value - backshift_bits);
                    }
                    if (remainder_head && !remainder_ptr[remainder_head])
                        remainder_head -= 1;
                }
                return remainder_head << 2U;
            }
            return find_head(quot_ptr, offset) << 2U;
        }

        constexpr size_t sieve_size = 2048;
        thread_local uint16_t primes_memory[sieve_size] = {0};
        constexpr uint32_t primes_sieve[sieve_size] = // clang-format off
        {   
            2,     3,     5,     7,     11,    13,    17,    19,    23,    29,    31,    37,    41,    43,    47,    53,
            59,    61,    67,    71,    73,    79,    83,    89,    97,    101,   103,   107,   109,   113,   127,   131,
            137,   139,   149,   151,   157,   163,   167,   173,   179,   181,   191,   193,   197,   199,   211,   223,
            227,   229,   233,   239,   241,   251,   257,   263,   269,   271,   277,   281,   283,   293,   307,   311,
            313,   317,   331,   337,   347,   349,   353,   359,   367,   373,   379,   383,   389,   397,   401,   409,
            419,   421,   431,   433,   439,   443,   449,   457,   461,   463,   467,   479,   487,   491,   499,   503,
            509,   521,   523,   541,   547,   557,   563,   569,   571,   577,   587,   593,   599,   601,   607,   613,
            617,   619,   631,   641,   643,   647,   653,   659,   661,   673,   677,   683,   691,   701,   709,   719,
            727,   733,   739,   743,   751,   757,   761,   769,   773,   787,   797,   809,   811,   821,   823,   827,
            829,   839,   853,   857,   859,   863,   877,   881,   883,   887,   907,   911,   919,   929,   937,   941,
            947,   953,   967,   971,   977,   983,   991,   997,   1009,  1013,  1019,  1021,  1031,  1033,  1039,  1049,
            1051,  1061,  1063,  1069,  1087,  1091,  1093,  1097,  1103,  1109,  1117,  1123,  1129,  1151,  1153,  1163,
            1171,  1181,  1187,  1193,  1201,  1213,  1217,  1223,  1229,  1231,  1237,  1249,  1259,  1277,  1279,  1283,
            1289,  1291,  1297,  1301,  1303,  1307,  1319,  1321,  1327,  1361,  1367,  1373,  1381,  1399,  1409,  1423,
            1427,  1429,  1433,  1439,  1447,  1451,  1453,  1459,  1471,  1481,  1483,  1487,  1489,  1493,  1499,  1511,
            1523,  1531,  1543,  1549,  1553,  1559,  1567,  1571,  1579,  1583,  1597,  1601,  1607,  1609,  1613,  1619,
            1621,  1627,  1637,  1657,  1663,  1667,  1669,  1693,  1697,  1699,  1709,  1721,  1723,  1733,  1741,  1747,
            1753,  1759,  1777,  1783,  1787,  1789,  1801,  1811,  1823,  1831,  1847,  1861,  1867,  1871,  1873,  1877,
            1879,  1889,  1901,  1907,  1913,  1931,  1933,  1949,  1951,  1973,  1979,  1987,  1993,  1997,  1999,  2003,
            2011,  2017,  2027,  2029,  2039,  2053,  2063,  2069,  2081,  2083,  2087,  2089,  2099,  2111,  2113,  2129,
            2131,  2137,  2141,  2143,  2153,  2161,  2179,  2203,  2207,  2213,  2221,  2237,  2239,  2243,  2251,  2267,
            2269,  2273,  2281,  2287,  2293,  2297,  2309,  2311,  2333,  2339,  2341,  2347,  2351,  2357,  2371,  2377,
            2381,  2383,  2389,  2393,  2399,  2411,  2417,  2423,  2437,  2441,  2447,  2459,  2467,  2473,  2477,  2503,
            2521,  2531,  2539,  2543,  2549,  2551,  2557,  2579,  2591,  2593,  2609,  2617,  2621,  2633,  2647,  2657,
            2659,  2663,  2671,  2677,  2683,  2687,  2689,  2693,  2699,  2707,  2711,  2713,  2719,  2729,  2731,  2741,
            2749,  2753,  2767,  2777,  2789,  2791,  2797,  2801,  2803,  2819,  2833,  2837,  2843,  2851,  2857,  2861,
            2879,  2887,  2897,  2903,  2909,  2917,  2927,  2939,  2953,  2957,  2963,  2969,  2971,  2999,  3001,  3011,
            3019,  3023,  3037,  3041,  3049,  3061,  3067,  3079,  3083,  3089,  3109,  3119,  3121,  3137,  3163,  3167,
            3169,  3181,  3187,  3191,  3203,  3209,  3217,  3221,  3229,  3251,  3253,  3257,  3259,  3271,  3299,  3301,
            3307,  3313,  3319,  3323,  3329,  3331,  3343,  3347,  3359,  3361,  3371,  3373,  3389,  3391,  3407,  3413,
            3433,  3449,  3457,  3461,  3463,  3467,  3469,  3491,  3499,  3511,  3517,  3527,  3529,  3533,  3539,  3541,
            3547,  3557,  3559,  3571,  3581,  3583,  3593,  3607,  3613,  3617,  3623,  3631,  3637,  3643,  3659,  3671,
            3673,  3677,  3691,  3697,  3701,  3709,  3719,  3727,  3733,  3739,  3761,  3767,  3769,  3779,  3793,  3797,
            3803,  3821,  3823,  3833,  3847,  3851,  3853,  3863,  3877,  3881,  3889,  3907,  3911,  3917,  3919,  3923,
            3929,  3931,  3943,  3947,  3967,  3989,  4001,  4003,  4007,  4013,  4019,  4021,  4027,  4049,  4051,  4057,
            4073,  4079,  4091,  4093,  4099,  4111,  4127,  4129,  4133,  4139,  4153,  4157,  4159,  4177,  4201,  4211,
            4217,  4219,  4229,  4231,  4241,  4243,  4253,  4259,  4261,  4271,  4273,  4283,  4289,  4297,  4327,  4337,
            4339,  4349,  4357,  4363,  4373,  4391,  4397,  4409,  4421,  4423,  4441,  4447,  4451,  4457,  4463,  4481,
            4483,  4493,  4507,  4513,  4517,  4519,  4523,  4547,  4549,  4561,  4567,  4583,  4591,  4597,  4603,  4621,
            4637,  4639,  4643,  4649,  4651,  4657,  4663,  4673,  4679,  4691,  4703,  4721,  4723,  4729,  4733,  4751,
            4759,  4783,  4787,  4789,  4793,  4799,  4801,  4813,  4817,  4831,  4861,  4871,  4877,  4889,  4903,  4909,
            4919,  4931,  4933,  4937,  4943,  4951,  4957,  4967,  4969,  4973,  4987,  4993,  4999,  5003,  5009,  5011,
            5021,  5023,  5039,  5051,  5059,  5077,  5081,  5087,  5099,  5101,  5107,  5113,  5119,  5147,  5153,  5167,
            5171,  5179,  5189,  5197,  5209,  5227,  5231,  5233,  5237,  5261,  5273,  5279,  5281,  5297,  5303,  5309,
            5323,  5333,  5347,  5351,  5381,  5387,  5393,  5399,  5407,  5413,  5417,  5419,  5431,  5437,  5441,  5443,
            5449,  5471,  5477,  5479,  5483,  5501,  5503,  5507,  5519,  5521,  5527,  5531,  5557,  5563,  5569,  5573,
            5581,  5591,  5623,  5639,  5641,  5647,  5651,  5653,  5657,  5659,  5669,  5683,  5689,  5693,  5701,  5711,
            5717,  5737,  5741,  5743,  5749,  5779,  5783,  5791,  5801,  5807,  5813,  5821,  5827,  5839,  5843,  5849,
            5851,  5857,  5861,  5867,  5869,  5879,  5881,  5897,  5903,  5923,  5927,  5939,  5953,  5981,  5987,  6007,
            6011,  6029,  6037,  6043,  6047,  6053,  6067,  6073,  6079,  6089,  6091,  6101,  6113,  6121,  6131,  6133,
            6143,  6151,  6163,  6173,  6197,  6199,  6203,  6211,  6217,  6221,  6229,  6247,  6257,  6263,  6269,  6271,
            6277,  6287,  6299,  6301,  6311,  6317,  6323,  6329,  6337,  6343,  6353,  6359,  6361,  6367,  6373,  6379,
            6389,  6397,  6421,  6427,  6449,  6451,  6469,  6473,  6481,  6491,  6521,  6529,  6547,  6551,  6553,  6563,
            6569,  6571,  6577,  6581,  6599,  6607,  6619,  6637,  6653,  6659,  6661,  6673,  6679,  6689,  6691,  6701,
            6703,  6709,  6719,  6733,  6737,  6761,  6763,  6779,  6781,  6791,  6793,  6803,  6823,  6827,  6829,  6833,
            6841,  6857,  6863,  6869,  6871,  6883,  6899,  6907,  6911,  6917,  6947,  6949,  6959,  6961,  6967,  6971,
            6977,  6983,  6991,  6997,  7001,  7013,  7019,  7027,  7039,  7043,  7057,  7069,  7079,  7103,  7109,  7121,
            7127,  7129,  7151,  7159,  7177,  7187,  7193,  7207,  7211,  7213,  7219,  7229,  7237,  7243,  7247,  7253,
            7283,  7297,  7307,  7309,  7321,  7331,  7333,  7349,  7351,  7369,  7393,  7411,  7417,  7433,  7451,  7457,
            7459,  7477,  7481,  7487,  7489,  7499,  7507,  7517,  7523,  7529,  7537,  7541,  7547,  7549,  7559,  7561,
            7573,  7577,  7583,  7589,  7591,  7603,  7607,  7621,  7639,  7643,  7649,  7669,  7673,  7681,  7687,  7691,
            7699,  7703,  7717,  7723,  7727,  7741,  7753,  7757,  7759,  7789,  7793,  7817,  7823,  7829,  7841,  7853,
            7867,  7873,  7877,  7879,  7883,  7901,  7907,  7919,  7927,  7933,  7937,  7949,  7951,  7963,  7993,  8009,
            8011,  8017,  8039,  8053,  8059,  8069,  8081,  8087,  8089,  8093,  8101,  8111,  8117,  8123,  8147,  8161,
            8167,  8171,  8179,  8191,  8209,  8219,  8221,  8231,  8233,  8237,  8243,  8263,  8269,  8273,  8287,  8291,
            8293,  8297,  8311,  8317,  8329,  8353,  8363,  8369,  8377,  8387,  8389,  8419,  8423,  8429,  8431,  8443,
            8447,  8461,  8467,  8501,  8513,  8521,  8527,  8537,  8539,  8543,  8563,  8573,  8581,  8597,  8599,  8609,
            8623,  8627,  8629,  8641,  8647,  8663,  8669,  8677,  8681,  8689,  8693,  8699,  8707,  8713,  8719,  8731,
            8737,  8741,  8747,  8753,  8761,  8779,  8783,  8803,  8807,  8819,  8821,  8831,  8837,  8839,  8849,  8861,
            8863,  8867,  8887,  8893,  8923,  8929,  8933,  8941,  8951,  8963,  8969,  8971,  8999,  9001,  9007,  9011,
            9013,  9029,  9041,  9043,  9049,  9059,  9067,  9091,  9103,  9109,  9127,  9133,  9137,  9151,  9157,  9161,
            9173,  9181,  9187,  9199,  9203,  9209,  9221,  9227,  9239,  9241,  9257,  9277,  9281,  9283,  9293,  9311,
            9319,  9323,  9337,  9341,  9343,  9349,  9371,  9377,  9391,  9397,  9403,  9413,  9419,  9421,  9431,  9433,
            9437,  9439,  9461,  9463,  9467,  9473,  9479,  9491,  9497,  9511,  9521,  9533,  9539,  9547,  9551,  9587,
            9601,  9613,  9619,  9623,  9629,  9631,  9643,  9649,  9661,  9677,  9679,  9689,  9697,  9719,  9721,  9733,
            9739,  9743,  9749,  9767,  9769,  9781,  9787,  9791,  9803,  9811,  9817,  9829,  9833,  9839,  9851,  9857,
            9859,  9871,  9883,  9887,  9901,  9907,  9923,  9929,  9931,  9941,  9949,  9967,  9973,  10007, 10009, 10037,
            10039, 10061, 10067, 10069, 10079, 10091, 10093, 10099, 10103, 10111, 10133, 10139, 10141, 10151, 10159, 10163,
            10169, 10177, 10181, 10193, 10211, 10223, 10243, 10247, 10253, 10259, 10267, 10271, 10273, 10289, 10301, 10303,
            10313, 10321, 10331, 10333, 10337, 10343, 10357, 10369, 10391, 10399, 10427, 10429, 10433, 10453, 10457, 10459,
            10463, 10477, 10487, 10499, 10501, 10513, 10529, 10531, 10559, 10567, 10589, 10597, 10601, 10607, 10613, 10627,
            10631, 10639, 10651, 10657, 10663, 10667, 10687, 10691, 10709, 10711, 10723, 10729, 10733, 10739, 10753, 10771,
            10781, 10789, 10799, 10831, 10837, 10847, 10853, 10859, 10861, 10867, 10883, 10889, 10891, 10903, 10909, 10937,
            10939, 10949, 10957, 10973, 10979, 10987, 10993, 11003, 11027, 11047, 11057, 11059, 11069, 11071, 11083, 11087,
            11093, 11113, 11117, 11119, 11131, 11149, 11159, 11161, 11171, 11173, 11177, 11197, 11213, 11239, 11243, 11251,
            11257, 11261, 11273, 11279, 11287, 11299, 11311, 11317, 11321, 11329, 11351, 11353, 11369, 11383, 11393, 11399,
            11411, 11423, 11437, 11443, 11447, 11467, 11471, 11483, 11489, 11491, 11497, 11503, 11519, 11527, 11549, 11551,
            11579, 11587, 11593, 11597, 11617, 11621, 11633, 11657, 11677, 11681, 11689, 11699, 11701, 11717, 11719, 11731,
            11743, 11777, 11779, 11783, 11789, 11801, 11807, 11813, 11821, 11827, 11831, 11833, 11839, 11863, 11867, 11887,
            11897, 11903, 11909, 11923, 11927, 11933, 11939, 11941, 11953, 11959, 11969, 11971, 11981, 11987, 12007, 12011,
            12037, 12041, 12043, 12049, 12071, 12073, 12097, 12101, 12107, 12109, 12113, 12119, 12143, 12149, 12157, 12161,
            12163, 12197, 12203, 12211, 12227, 12239, 12241, 12251, 12253, 12263, 12269, 12277, 12281, 12289, 12301, 12323,
            12329, 12343, 12347, 12373, 12377, 12379, 12391, 12401, 12409, 12413, 12421, 12433, 12437, 12451, 12457, 12473,
            12479, 12487, 12491, 12497, 12503, 12511, 12517, 12527, 12539, 12541, 12547, 12553, 12569, 12577, 12583, 12589,
            12601, 12611, 12613, 12619, 12637, 12641, 12647, 12653, 12659, 12671, 12689, 12697, 12703, 12713, 12721, 12739,
            12743, 12757, 12763, 12781, 12791, 12799, 12809, 12821, 12823, 12829, 12841, 12853, 12889, 12893, 12899, 12907,
            12911, 12917, 12919, 12923, 12941, 12953, 12959, 12967, 12973, 12979, 12983, 13001, 13003, 13007, 13009, 13033,
            13037, 13043, 13049, 13063, 13093, 13099, 13103, 13109, 13121, 13127, 13147, 13151, 13159, 13163, 13171, 13177,
            13183, 13187, 13217, 13219, 13229, 13241, 13249, 13259, 13267, 13291, 13297, 13309, 13313, 13327, 13331, 13337,
            13339, 13367, 13381, 13397, 13399, 13411, 13417, 13421, 13441, 13451, 13457, 13463, 13469, 13477, 13487, 13499,
            13513, 13523, 13537, 13553, 13567, 13577, 13591, 13597, 13613, 13619, 13627, 13633, 13649, 13669, 13679, 13681,
            13687, 13691, 13693, 13697, 13709, 13711, 13721, 13723, 13729, 13751, 13757, 13759, 13763, 13781, 13789, 13799,
            13807, 13829, 13831, 13841, 13859, 13873, 13877, 13879, 13883, 13901, 13903, 13907, 13913, 13921, 13931, 13933,
            13963, 13967, 13997, 13999, 14009, 14011, 14029, 14033, 14051, 14057, 14071, 14081, 14083, 14087, 14107, 14143,
            14149, 14153, 14159, 14173, 14177, 14197, 14207, 14221, 14243, 14249, 14251, 14281, 14293, 14303, 14321, 14323,
            14327, 14341, 14347, 14369, 14387, 14389, 14401, 14407, 14411, 14419, 14423, 14431, 14437, 14447, 14449, 14461,
            14479, 14489, 14503, 14519, 14533, 14537, 14543, 14549, 14551, 14557, 14561, 14563, 14591, 14593, 14621, 14627,
            14629, 14633, 14639, 14653, 14657, 14669, 14683, 14699, 14713, 14717, 14723, 14731, 14737, 14741, 14747, 14753,
            14759, 14767, 14771, 14779, 14783, 14797, 14813, 14821, 14827, 14831, 14843, 14851, 14867, 14869, 14879, 14887,
            14891, 14897, 14923, 14929, 14939, 14947, 14951, 14957, 14969, 14983, 15013, 15017, 15031, 15053, 15061, 15073,
            15077, 15083, 15091, 15101, 15107, 15121, 15131, 15137, 15139, 15149, 15161, 15173, 15187, 15193, 15199, 15217,
            15227, 15233, 15241, 15259, 15263, 15269, 15271, 15277, 15287, 15289, 15299, 15307, 15313, 15319, 15329, 15331,
            15349, 15359, 15361, 15373, 15377, 15383, 15391, 15401, 15413, 15427, 15439, 15443, 15451, 15461, 15467, 15473,
            15493, 15497, 15511, 15527, 15541, 15551, 15559, 15569, 15581, 15583, 15601, 15607, 15619, 15629, 15641, 15643,
            15647, 15649, 15661, 15667, 15671, 15679, 15683, 15727, 15731, 15733, 15737, 15739, 15749, 15761, 15767, 15773,
            15787, 15791, 15797, 15803, 15809, 15817, 15823, 15859, 15877, 15881, 15887, 15889, 15901, 15907, 15913, 15919,
            15923, 15937, 15959, 15971, 15973, 15991, 16001, 16007, 16033, 16057, 16061, 16063, 16067, 16069, 16073, 16087,
            16091, 16097, 16103, 16111, 16127, 16139, 16141, 16183, 16187, 16189, 16193, 16217, 16223, 16229, 16231, 16249,
            16253, 16267, 16273, 16301, 16319, 16333, 16339, 16349, 16361, 16363, 16369, 16381, 16411, 16417, 16421, 16427,
            16433, 16447, 16451, 16453, 16477, 16481, 16487, 16493, 16519, 16529, 16547, 16553, 16561, 16567, 16573, 16603,
            16607, 16619, 16631, 16633, 16649, 16651, 16657, 16661, 16673, 16691, 16693, 16699, 16703, 16729, 16741, 16747,
            16759, 16763, 16787, 16811, 16823, 16829, 16831, 16843, 16871, 16879, 16883, 16889, 16901, 16903, 16921, 16927,
            16931, 16937, 16943, 16963, 16979, 16981, 16987, 16993, 17011, 17021, 17027, 17029, 17033, 17041, 17047, 17053,
            17077, 17093, 17099, 17107, 17117, 17123, 17137, 17159, 17167, 17183, 17189, 17191, 17203, 17207, 17209, 17231,
            17239, 17257, 17291, 17293, 17299, 17317, 17321, 17327, 17333, 17341, 17351, 17359, 17377, 17383, 17387, 17389,
            17393, 17401, 17417, 17419, 17431, 17443, 17449, 17467, 17471, 17477, 17483, 17489, 17491, 17497, 17509, 17519,
            17539, 17551, 17569, 17573, 17579, 17581, 17597, 17599, 17609, 17623, 17627, 17657, 17659, 17669, 17681, 17683,
            17707, 17713, 17729, 17737, 17747, 17749, 17761, 17783, 17789, 17791, 17807, 17827, 17837, 17839, 17851, 17863,
        };
        // clang-format on

        thread_local std::random_device dev;
        thread_local std::mt19937 rng(dev());
        thread_local std::uniform_int_distribution<std::mt19937::result_type> dist(0, std::numeric_limits<uint8_t>::max());

        template <typename word_t>
        struct DecimalMagic
        {
            static constexpr word_t number1 = []() { // inverse of 5 mod 2^bits_in_word
                if constexpr (std::is_same_v<uint64_t, word_t>)
                    return 0xcccccccccccccccdULL;
                else if constexpr (std::is_same_v<uint32_t, word_t>)
                    return 0xcccccccdULL;
                else if constexpr (std::is_same_v<uint16_t, word_t>)
                    return 0xcccdULL;
                else
                    static_assert(false, "unsupported wordtype!");
            }();
            static constexpr word_t number2 = MPA_SHIFTBASE << (BitsInWord<word_t>::value - 1);
        };

        struct EGCDFlags
        {
            size_t r0_flags = 0;
            size_t s0_flags = 0;
            size_t t0_flags = 0;
            size_t location_encoding = 0;
        };
    }

    template <typename word_t>
    class Integer
    {
#if defined(__SIZEOF_INT128__) && !defined(_MSC_VER)
        static_assert(std::is_same_v<word_t, uint64_t> || std::is_same_v<word_t, uint32_t> || std::is_same_v<word_t, uint16_t>,
                      "Only wordtypes uint16_t, uint32_t and uint64_t are supported on your platform.");
#else
        static_assert(std::is_same_v<word_t, uint32_t> || std::is_same_v<word_t, uint16_t>,
                      "Only wordtypes uint16_t and uint32_t are supported on your platform.");
#endif
        using signed_word_t = std::make_signed_t<word_t>;

    public:
        ~Integer() noexcept
        {
            has_ownership() ? free(words) : (void)0;
        }

        Integer() noexcept : words(nullptr), flags(0)
        {
        }

        Integer(word_t *wordptr, const size_t flagz) noexcept : words(wordptr), flags(flagz)
        {
        }

        Integer(const signed_word_t n) noexcept : words(nullptr), flags((n < 0) | 0b10)
        {
            words = allocate_words<word_t>(1), *words = n < 0 ? -n : n;
        }

        Integer(const Integer &other) noexcept : words(nullptr), flags(0)
        {
            const size_t wordcount = other.get_word_count();
            words = allocate_words<word_t>(wordcount);
            copy_words(words, other.words, wordcount);
            flags = other.is_negative() | 0b10 | ((wordcount - 1) << 2U);
        }

        Integer(Integer &&other) noexcept : words(other.words), flags(other.flags)
        {
            other.flags &= OWNERSHIP_OFF; // clear the ownership
        }

        Integer(std::vector<word_t> &word_vector, bool sign) noexcept
            : words(word_vector.data()),
              flags(sign | (find_head(word_vector.data(), word_vector.size() - 1) << 2U)) // the vector owns its buffer
        {
        }

        word_t get_word(const size_t index) const noexcept
        {
            return index <= get_head() ? words[index] : 0;
        }

        bool get_bit(const size_t index) const noexcept
        {
            return ((index / BitsInWord<word_t>::value) <= get_head())
                       ? words[index / BitsInWord<word_t>::value] & (MPA_SHIFTBASE << (index & (BitsInWord<word_t>::value - 1)))
                       : 0;
        }

        bool has_ownership() const noexcept
        {
            return flags & 0b10;
        }

        size_t get_head() const noexcept
        {
            return flags >> 2U;
        }

        bool is_negative() const noexcept
        {
            return flags & 1;
        }

        size_t get_word_count() const noexcept
        {
            return get_head() + 1;
        }

        size_t get_bit_count() const noexcept
        {
            return get_word_count() * BitsInWord<word_t>::value - get_leading_zero_bits(words[get_head()]);
        }

        bool is_zero() const noexcept
        {
            return !words[get_head()];
        }

        explicit operator bool() const noexcept
        {
            return !is_zero();
        }

        bool is_odd() const noexcept
        {
            return words[0] & 1;
        }

        bool is_even() const noexcept
        {
            return !is_odd();
        }

        std::string to_hex() const noexcept
        {
            std::stringstream ss;
            ss << (!is_negative() ? "0x" : "-0x");
            ss << std::hex << words[get_head()];
            for (size_t i = get_head() - 1; i < get_head(); --i)
                ss << std::setfill('0') << std::setw(sizeof(word_t) * 2) << std::hex << words[i];
            return ss.str();
        }

        std::string to_binary() const noexcept
        {
            if (is_zero())
                return "0b0";
            std::stringstream ss;
            ss << (!is_negative() ? "0b" : "-0b");
            size_t k = BitsInWord<word_t>::value - 1 - get_leading_zero_bits(words[get_head()]);
            for (size_t j = k; j < k + 1; --j)
                ss << get_bit((get_head() * BitsInWord<word_t>::value) + j);
            for (size_t i = get_head() - 1; i < get_head(); --i)
            {
                for (size_t j = BitsInWord<word_t>::value - 1; j < BitsInWord<word_t>::value; --j)
                    ss << get_bit((i * BitsInWord<word_t>::value) + j);
            }
            return ss.str();
        }

        std::string to_decimal() const noexcept
        {
            static constexpr word_t magic1 = DecimalMagic<word_t>::number1;
            static constexpr word_t magic2 = DecimalMagic<word_t>::number2;
            if (is_zero())
                return "0";
            std::vector<uint32_t> digits;
            size_t tmp_head = get_head();
            const size_t tmp_size = tmp_head + 1;
            word_t *tmp = allocate_words<word_t>(tmp_size);
            copy_words(tmp, words, tmp_size);
            while (tmp_head || tmp[tmp_head])
            {
                word_t remainder = 0;
                for (size_t i = tmp_head; i < tmp_head + 1; --i)
                {
                    const word_t dividend_high = remainder;
                    const word_t dividend_low = tmp[i];
                    remainder = ((dividend_low % 10U) + 6 * (dividend_high % 10U)) % 10U;
                    const word_t rhs =
                        ((word_t)(dividend_low - remainder) >> 1U) + (dividend_high != 0) *
                                                                         ((dividend_low < remainder && !(dividend_high & 1U)) +
                                                                          (dividend_low >= remainder && (dividend_high & 1U))) *
                                                                         magic2;
                    if constexpr (std::is_same_v<uint16_t, word_t>)
                        tmp[i] = magic1 * (uint32_t)rhs; // avoid potential ub caused by promotion :)
                    else
                        tmp[i] = magic1 * rhs;
                }
                digits.push_back(remainder), tmp_head = find_head(tmp, tmp_size - 1);
            }
            const size_t digits_size = digits.size();
            std::string out_str;
            const size_t i = is_negative();
            out_str.resize(digits_size + i);
            out_str[0] = i ? '-' : 0;
            for (size_t j = 0; j < digits_size; ++j)
                out_str[j + i] = digits[digits_size - 1 - j] + '0';
            return free(tmp), out_str;
        }

        Integer &operator=(const Integer &N) noexcept
        {
            if (words == N.words) // handle self assignment
                return *this;
            const size_t N_wordcount = 1 + N.get_head();
            bool N_sign_bit = N.is_negative();
            if (words && (N.get_head() <= get_head())) // if new words fit in old buffer -> overwrite
            {
                clear_words(words, get_word_count());
                copy_words(words, N.words, N_wordcount);
                return flags = N_sign_bit | (has_ownership() << 1U) | ((N_wordcount - 1) << 2U), *this;
            }
            if (has_ownership()) // release our buffer if we have ownership and new words don't fit in old buffer
                free(words);
            words = allocate_words<word_t>(N_wordcount);
            copy_words(words, N.words, N_wordcount);
            return flags = N_sign_bit | 0b10 | ((N_wordcount - 1) << 2U), *this;
        }

        Integer &operator=(Integer &&N) noexcept
        {
            if (words == N.words) // handle self assignment
                return *this;
            if (has_ownership()) // release our buffer if we have ownership
                free(words);
            words = N.words;
            flags = N.flags;
            return N.flags &= OWNERSHIP_OFF, *this; // clear (ownership) flags for N
        }

        Integer operator<<(const size_t shift) const noexcept
        {
            if (!shift || is_zero())
                return *this;
            const size_t words_shift = shift / BitsInWord<word_t>::value;
            const size_t wordcount = get_head() + words_shift + 2;
            word_t *out_words = allocate_words<word_t>(wordcount);
            const size_t out_head =
                shift_left_by_words_and_bits(words, get_head(), shift & (BitsInWord<word_t>::value - 1), words_shift, out_words);
            return Integer(out_words, is_negative() | 0b10 | (out_head << 2U));
        }

        Integer operator>>(const size_t shift) const noexcept
        {
            if (!shift)
                return *this;
            const size_t shift_words = shift / BitsInWord<word_t>::value;
            if (get_word_count() <= shift_words)
                return Integer(0);
            const size_t shift_bits = shift & (BitsInWord<word_t>::value - 1);
            const size_t wc = get_word_count() - shift_words;
            word_t *out_ptr = allocate_words<word_t>(wc);
            copy_words(out_ptr, words + shift_words, wc);
            size_t out_head = wc - 1;
            if (shift_bits)
            {
                word_t c = 0;
                for (size_t i = out_head; i <= out_head; --i)
                {
                    const word_t tmp = out_ptr[i];
                    out_ptr[i] = (tmp >> shift_bits) | c;
                    c = (tmp & ((MPA_SHIFTBASE << shift_bits) - 1)) << (BitsInWord<word_t>::value - shift_bits);
                }
                if (out_head && !out_ptr[out_head])
                    out_head -= 1;
            }
            return Integer(out_ptr, is_negative() | 0b10 | (out_head << 2U));
        }

        Integer operator-() const noexcept
        {
            Integer out = *this;
            if (is_zero())
                return out;
            return out.flags = (!is_negative()) | (flags & SIGN_OFF), out;
        }

        Integer &operator++() noexcept
        {
            return operator+=(1), *this;
        }

        Integer operator++(int) noexcept
        {
            Integer out = *this;
            return operator+=(1), out;
        }

        Integer &operator--() noexcept
        {
            return operator-=(1), *this;
        }

        Integer operator--(int) noexcept
        {
            Integer out = *this;
            return operator-=(1), out;
        }

        Integer &operator+=(const Integer &other) noexcept
        {
            return *this = *this + other, *this;
        }

        Integer &operator-=(const Integer &other) noexcept
        {
            return *this = *this - other, *this;
        }

        Integer &operator-=(const signed_word_t r) noexcept
        {
            if (r >= 0)
            { // r is non-negative
                const word_t r_local = r;
                if (!is_negative())
                { // we are non-negative
                    if (get_head() || words[0] >= r_local)
                    {
                        inplace_decrement(words, &r_local, 1);
                        const size_t new_head = find_head(words, get_head());
                        return flags = (flags & 0b11) | (new_head << 2U), *this;
                    }
                    // head is 0 and words[0] < r_local;
                    words[0] = r_local - words[0];
                    return flags = 1 | flags, *this; // set sign bit
                }
                // we are negative
                word_t carry = words[0] > std::numeric_limits<word_t>::max() - r_local;
                words[0] += r_local;
                if (!carry)
                    return *this;
                size_t i = 1;
                for (i = 1; carry && i <= get_head(); ++i)
                    words[i] += (carry = words[i] == std::numeric_limits<word_t>::max(), 1);
                if (carry && i == get_word_count())
                {
                    word_t *new_words = allocate_words<word_t>(i + 1);
                    copy_words(new_words, words, i);
                    new_words[i] = 1;
                    if (has_ownership())
                        free(words);
                    words = new_words;
                    flags = (flags & 0b1) | 0b10 | (i << 2U); // we allocated memory, we now own it
                }
                return *this;
            }
            // r is negative
            const word_t r_local = -r;
            if (!is_negative())
            { // we are non-negative
                word_t carry = words[0] > std::numeric_limits<word_t>::max() - r_local;
                words[0] += r_local;
                if (!carry)
                    return *this;
                size_t i = 1;
                for (i = 1; carry && i <= get_head(); ++i)
                    words[i] += (carry = words[i] == std::numeric_limits<word_t>::max(), 1);
                if (carry && i == get_word_count())
                {
                    word_t *new_words = allocate_words<word_t>(i + 1);
                    copy_words(new_words, words, i);
                    new_words[i] = 1;
                    if (has_ownership())
                        free(words);
                    words = new_words;
                    flags = (flags & 0b1) | 0b10 | (i << 2U); // we allocated memory, we now own it
                }
                return *this;
            }
            // we are negative
            if (get_head() || words[0] > r_local)
            {
                inplace_decrement(words, &r_local, 1);
                const size_t new_head = find_head(words, get_head());
                return flags = (flags & 0b11) | (new_head << 2U), *this;
            }
            // head is 0 and words[0] <= r_local;
            words[0] = r_local - words[0];
            return flags &= SIGN_OFF, *this; // clear sign bit
        }

        Integer &operator+=(const signed_word_t r) noexcept
        {
            return *this -= -r, *this;
        }

        Integer &operator*=(const Integer &other) noexcept
        {
            return *this = *this * other, *this;
        }

        Integer &operator/=(const Integer &r) noexcept
        {
            const size_t l_head = get_head();
            const size_t r_head = r.get_head();
            const size_t K = l_head + 5;
            if (r_head > l_head)
                return (words[0] = 0, flags = has_ownership() << 1U, *this);
            word_t *workspace;
            const bool require_allocation = 4 * K > BufferSize<word_t>::divmod;
            workspace = !require_allocation ? clear_words((word_t *)divmod_buffer, 4 * K), (word_t *)divmod_buffer
                                            : allocate_words<word_t>(4 * K);
            word_t *quot_ptr = workspace + 3 * K;
            size_t quot_flags = divmod(words, l_head, r.words, r_head, quot_ptr, workspace, K);
            const size_t new_head = quot_flags >> 2U;
            copy_words(words, quot_ptr, new_head + 1);
            const bool quotient_is_zero = !new_head && !(*words);
            const bool quotient_is_negative = !(is_negative() == r.is_negative() || quotient_is_zero);
            flags = quotient_is_negative | (has_ownership() << 1U) | (new_head << 2U);
            return require_allocation ? free(workspace) : (void)0, *this;
        }

        Integer &operator%=(const Integer &other) noexcept
        {
            return *this = *this % other, *this;
        }

        Integer &operator<<=(const size_t shift) noexcept
        {
            return *this = *this << shift, *this;
        }

        Integer &operator>>=(const size_t shift) noexcept
        {
            if (!shift)
                return *this;
            const size_t shift_words = shift / BitsInWord<word_t>::value;
            if (get_word_count() <= shift_words)
                return (flags = has_ownership() << 1U, words[0] = 0, *this);
            const size_t wc = get_word_count() - shift_words;
            move_words(words, words + shift_words, wc);
            const size_t shift_bits = shift & (BitsInWord<word_t>::value - 1);
            size_t out_head = wc - 1;
            if (shift_bits)
            {
                word_t c = 0;
                for (size_t i = out_head; i <= out_head; --i)
                {
                    const word_t tmp = words[i];
                    words[i] = (tmp >> shift_bits) | c;
                    c = (tmp & ((MPA_SHIFTBASE << shift_bits) - 1)) << (BitsInWord<word_t>::value - shift_bits);
                }
                if (out_head && !words[out_head])
                    out_head -= 1;
            }
            if (shift_words)
                clear_words(&words[out_head + 1], shift_words);
            return flags = is_negative() | (has_ownership() << 1U) | (out_head << 2U), *this;
        }

        explicit Integer(const std::string &input_string) noexcept : words(nullptr), flags(0)
        {
            const bool sign_bit = input_string[0] == '-';
            const std::string format = sign_bit ? input_string.substr(1, 2) : input_string.substr(0, 2);
            size_t words_size;
            if (format == "0x" || format == "0b") // hex format or binary format
            {
                word_t base = format == "0x" ? 16 : 2;
                if (!validate_input_string(input_string, base))
                {
                    std::cerr << "WARNING: input string contains invalid characters. constructing 0 by default.\n";
                    *this = 0;
                    return;
                }
                const std::string effective_ =
                    input_string.substr(0, 1) == "-" ? input_string.substr(3) : input_string.substr(2);
                const size_t string_size = effective_.size();
                const size_t characters_per_word = base == 16 ? BitsInWord<word_t>::value / 4 : BitsInWord<word_t>::value;
                words_size = string_size & (characters_per_word - 1) ? (string_size / characters_per_word) + 1
                                                                     : string_size / characters_per_word;
                words = allocate_words<word_t>(words_size);
                size_t current_pos = string_size - 1;
                size_t word_index = words_size - 1;
                while (true)
                {
                    if (current_pos >= characters_per_word)
                    {
                        std::string current_chunk =
                            effective_.substr(current_pos - characters_per_word + 1, characters_per_word);
                        words[words_size - 1 - word_index] = std::stoull(current_chunk, nullptr, base);
                        current_pos -= characters_per_word;
                    }
                    else
                    {
                        std::string current_chunk = effective_.substr(0, current_pos + 1);
                        words[words_size - 1 - word_index] = std::stoull(current_chunk, nullptr, base);
                        break;
                    }
                    word_index -= 1;
                }
            }
            else // implicit decimal format
            {
                if (!validate_input_string(input_string, 10))
                {
                    std::cerr << "WARNING: input string contains invalid characters. constructing 0 by default.\n";
                    *this = 0;
                    return;
                }
                const std::string effective_ = input_string.substr(0, 1) == "-" ? input_string.substr(1) : input_string;
                const size_t string_size = effective_.size();
                const size_t needed_words_size = string_size / (BitsInWord<word_t>::value / 4) + 2; // upper bound based on hex case
                word_t *local_workspace = allocate_words<word_t>(3 * needed_words_size);
                word_t *word_ptr = allocate_words<word_t>(needed_words_size);
                word_t *tmp2_ptr = local_workspace;
                word_t *tmp3_ptr = local_workspace + needed_words_size;
                word_t *base_ptr = local_workspace + 2 * needed_words_size;
                word_t _digit = effective_[string_size - 1] - '0';
                *base_ptr = 1;
                *word_ptr = _digit;
                Integer future_this(word_ptr, 0);
                Integer base(base_ptr, 0);
                for (size_t i = 1; i < string_size; ++i)
                {
                    clear_words(tmp2_ptr, needed_words_size);
                    multiply_by_word<word_t>(10, base_ptr, base.get_word_count(), tmp2_ptr);
                    copy_words(base_ptr, tmp2_ptr, needed_words_size);
                    if (base.get_word_count() < needed_words_size && base_ptr[base.get_word_count()])
                        base.flags = base.get_word_count() << 2U;
                    _digit = effective_[string_size - 1 - i] - '0';
                    if (!_digit)
                        continue;
                    clear_words(tmp2_ptr, needed_words_size);
                    multiply_by_word(_digit, base_ptr, base.get_word_count(), tmp2_ptr);
                    size_t tmp2_head = find_head(tmp2_ptr, needed_words_size - 1);
                    copy_words(tmp3_ptr, word_ptr, needed_words_size);
                    clear_words(word_ptr, needed_words_size);
                    add_words(tmp2_ptr, tmp3_ptr, tmp2_head + 1, future_this.get_word_count(), word_ptr);
                    future_this.flags = word_ptr[tmp2_head + 1] ? (tmp2_head + 1) << 2U : tmp2_head << 2U;
                }
                free(local_workspace);
                words = future_this.words;
                words_size = future_this.get_word_count();
            }
            // handle leading zeroes
            flags = sign_bit | 0b10 | (find_head(words, words_size - 1) << 2U);
        }

        friend std::ostream &operator<<(std::ostream &os, const Integer &n) noexcept
        {
            os << n.to_hex();
            return os;
        }

        friend bool operator==(const Integer &l, const Integer &r) noexcept
        {
            if (!l.words)
                return !r.words;
            if (!r.words)
                return false;
            return l.is_negative() == r.is_negative() && l.get_head() == r.get_head() &&
                   !memcmp(l.words, r.words, l.get_word_count() * sizeof(word_t));
        }

        friend bool operator!=(const Integer &l, const Integer &r) noexcept
        {
            return !(l == r);
        }

        friend bool operator>(const Integer &l, const Integer &r) noexcept
        { // clang-format off
            if (!l.is_negative() && r.is_negative())
                return true;
            if (l.is_negative() && !r.is_negative())
                return false;
            if (!l.is_negative() && !r.is_negative())
            {
                if (l.get_head() > r.get_head())
                    return true;
                if (l.get_head() < r.get_head())
                    return false;
                size_t i = 0;
                for (i = l.get_head(); i < l.get_word_count() && l.words[i] == r.words[i]; --i);
                return i < l.get_word_count() ? l.words[i] > r.words[i] : false;
            }
            if (l.get_head() > r.get_head())
                return false;
            if (l.get_head() < r.get_head())
                return true;
            size_t i = 0;
            for (i = l.get_head(); i < l.get_word_count() && l.words[i] == r.words[i]; --i);
            return i < l.get_word_count() ? l.words[i] < r.words[i] : false;
        } // clang-format on

        friend bool operator>=(const Integer &l, const Integer &r) noexcept
        {
            return l == r || l > r;
        }

        friend bool operator<=(const Integer &l, const Integer &r) noexcept
        {
            return !(l > r);
        }

        friend bool operator<(const Integer &l, const Integer &r) noexcept
        {
            return (l != r) && (l <= r);
        }

        friend Integer operator^(const Integer &l, const Integer &r) noexcept
        {
            const Integer &bigger = l.get_word_count() > r.get_word_count() ? l : r;
            const Integer &smaller = l.get_word_count() > r.get_word_count() ? r : l;
            word_t *outwords = allocate_words<word_t>(bigger.get_word_count());
            size_t outhead = 0;
            for (size_t i = 0; i < smaller.get_word_count(); ++i)
                outhead = (outwords[i] = l.words[i] ^ r.words[i]) ? i : outhead;
            for (size_t i = smaller.get_word_count(); i < bigger.get_word_count(); ++i)
                outhead = (outwords[i] = bigger.words[i]) ? i : outhead;
            return Integer(outwords, 0b10 | (outhead << 2U));
        }

        friend Integer operator|(const Integer &l, const Integer &r) noexcept
        {
            const Integer &bigger = l.get_word_count() > r.get_word_count() ? l : r;
            const Integer &smaller = l.get_word_count() > r.get_word_count() ? r : l;
            word_t *outwords = allocate_words<word_t>(bigger.get_word_count());
            size_t outhead = 0;
            for (size_t i = 0; i < smaller.get_word_count(); ++i)
                outhead = (outwords[i] = l.words[i] | r.words[i]) ? i : outhead;
            for (size_t i = smaller.get_word_count(); i < bigger.get_word_count(); ++i)
                outhead = (outwords[i] = bigger.words[i]) ? i : outhead;
            return Integer(outwords, 0b10 | (outhead << 2U));
        }

        friend Integer operator&(const Integer &l, const Integer &r) noexcept
        {
            const size_t min_size = l.get_word_count() > r.get_word_count() ? r.get_word_count() : l.get_word_count();
            word_t *outwords = allocate_words<word_t>(min_size);
            size_t outhead = 0;
            for (size_t i = 0; i < min_size; ++i)
                outhead = (outwords[i] = l.words[i] & r.words[i]) ? i : outhead;
            return Integer(outwords, 0b10 | (outhead << 2U));
        }

        friend Integer operator+(const Integer &l, const Integer &r) noexcept
        {
            const size_t l_head = l.get_head();
            const size_t r_head = r.get_head();
            word_t *out_words = allocate_words<word_t>(l_head > r_head ? l_head + 2 : r_head + 2);
            return Integer(out_words, 0b10 | add(l, r, out_words));
        }

        friend Integer operator-(const Integer &l, const Integer &r) noexcept
        {
            const size_t l_head = l.get_head();
            const size_t r_head = r.get_head();
            word_t *out_words = allocate_words<word_t>(l_head > r_head ? l_head + 2 : r_head + 2);
            return Integer(out_words, 0b10 | subtract(l, r, out_words));
        }

        friend Integer operator*(const Integer &l, const Integer &r) noexcept
        {
            word_t *out_words = allocate_words<word_t>(l.get_word_count() + r.get_word_count());
            return Integer(out_words, 0b10 | multiply(l, r, out_words));
        }

        friend Integer operator/(const Integer &l, const Integer &r) noexcept
        {
            word_t *out_words = allocate_words<word_t>(l.get_head() + 5);
            return Integer(out_words, 0b10 | call_divmod(l, r, out_words));
        }

        friend Integer operator%(const Integer &l, const Integer &r) noexcept
        {
            word_t *out_words = allocate_words<word_t>(l.get_head() + 5);
            if (l.is_negative())
                return r.is_negative() ? -r - Integer(out_words, 0b10 | call_divmod(l, r, out_words, true))
                                       : r - Integer(out_words, 0b10 | call_divmod(l, r, out_words, true));
            return Integer(out_words, 0b10 | call_divmod(l, r, out_words, true));
        }

        static Integer get_random(const size_t wordcount, const bool is_negative, word_t *buffer = nullptr) noexcept
        {
            word_t *out_words = buffer ? buffer : allocate_words<word_t>(wordcount);
            for (size_t i = 0; i < wordcount - 1; ++i)
            {
                out_words[i] = 0;
                for (size_t j = 0; j < sizeof(word_t); ++j)
                    out_words[i] |= dist(rng) << (8 * j);
            }
            word_t head = 0;
            for (size_t j = 0; j < sizeof(word_t); ++j)
                head |= dist(rng) << (8 * j);
            out_words[wordcount - 1] = head ? head : std::numeric_limits<word_t>::max();
            return Integer(out_words, is_negative | (buffer ? 0 : 0b10) | ((wordcount - 1) << 2U));
        }

        static Integer get_random(const Integer &limit, word_t *buffer = nullptr) noexcept
        {
            const Integer unsigned_limit(limit.words, limit.flags & SIGN_OFF_OWNERSHIP_OFF);
            const size_t limit_bit_count = limit.get_bit_count();
            size_t limit_byte_count = limit_bit_count / 8;
            const size_t bits_left_over = limit_bit_count - limit_byte_count * 8;
            word_t *out_words = buffer ? buffer : allocate_words<word_t>(limit.get_word_count());
            if (limit_byte_count <= 1)
            {
                while (true)
                {
                    out_words[0] = dist(rng);
                    Integer out(out_words, (buffer ? 0 : 0b10));
                    if (out_words[0] && out < unsigned_limit)
                        return out;
                }
            }
            limit_byte_count -= bits_left_over;
            while (true)
            {
                size_t out_head = 0;
                clear_words(out_words, limit.get_word_count());
                for (size_t i = 0; i < limit_byte_count; ++i)
                    out_head = i / sizeof(word_t), out_words[out_head] |= dist(rng) << ((i % sizeof(word_t)) * 8);
                while (!out_words[out_head])
                    out_words[out_head] = dist(rng);
                Integer out(out_words, (buffer ? 0 : 0b10) | (out_head << 2U));
                if (out < unsigned_limit)
                    return out;
            }
        }

    private:
        static size_t add(const Integer &l, const Integer &r, word_t *out_words) noexcept
        {
            const size_t l_head = l.get_head();
            const size_t r_head = r.get_head();
            const size_t bigger_head = l_head > r_head ? l_head : r_head;
            const size_t smaller_head = l_head < r_head ? l_head : r_head;
            if (l.is_negative() == r.is_negative())
            {
                DO_ADD(l, r, bigger_head, smaller_head);
            }
            else
            {
                DO_SUB(l, r, bigger_head, smaller_head);
            }
        }

        static size_t subtract(const Integer &l, const Integer &r, word_t *out_words) noexcept
        {
            const size_t l_head = l.get_head();
            const size_t r_head = r.get_head();
            const size_t bigger_head = l_head > r_head ? l_head : r_head;
            const size_t smaller_head = l_head < r_head ? l_head : r_head;
            if (l.is_negative() != r.is_negative())
            {
                DO_ADD(l, r, bigger_head, smaller_head);
            }
            else
            {
                DO_SUB(l, r, bigger_head, smaller_head);
            }
        }

        static size_t multiply(const Integer &l, const Integer &r, word_t *out_words) noexcept
        {
            const size_t lsize = l.get_word_count();
            const size_t rsize = r.get_word_count();
            (l.words == r.words && lsize == rsize) ? square_karatsuba(l.words, lsize, out_words)
                                                   : multiply_karatsuba(l.words, r.words, lsize, rsize, out_words);
            size_t out_head = find_head(out_words, lsize + rsize - 1);
            const bool out_sign = l.is_negative() != r.is_negative() && out_words[out_head];
            return out_sign | (out_head << 2U); // no ownership
        }

        static size_t call_divmod(const Integer &l, const Integer &r, word_t *out_words, bool need_remainder = false) noexcept
        {
            const size_t l_head = l.get_head();
            const size_t r_head = r.get_head();
            const size_t K = l_head + 5;
            if (!need_remainder)
            {
                if (r_head > l_head)
                    return out_words[0] = 0, 0;
                word_t *quot_ptr = out_words;
                word_t *workspace;
                const bool require_allocation = 3 * K > BufferSize<word_t>::divmod;
                workspace = !require_allocation ? clear_words((word_t *)divmod_buffer, 3 * K), (word_t *)divmod_buffer
                                                : allocate_words<word_t>(3 * K);
                size_t quot_flags = divmod(l.words, l_head, r.words, r_head, quot_ptr, workspace, K, need_remainder);
                const bool quotient_is_zero = !(quot_flags >> 2U) && !(*quot_ptr);
                const bool quotient_is_negative = !(l.is_negative() == r.is_negative() || quotient_is_zero);
                return require_allocation ? free(workspace) : (void)0, quotient_is_negative | quot_flags;
            }
            word_t *remainder_ptr = out_words;
            word_t *workspace;
            const bool require_allocation = 3 * K > BufferSize<word_t>::divmod;
            workspace = !require_allocation ? clear_words((word_t *)divmod_buffer, 3 * K), (word_t *)divmod_buffer
                                            : allocate_words<word_t>(3 * K);
            const size_t remainder_flags = divmod(l.words, l_head, r.words, r_head, remainder_ptr, workspace, K, need_remainder);
            return require_allocation ? free(workspace) : (void)0, remainder_flags;
        }

    public:
        word_t *words;
        size_t flags;
    };

#define MUL_OP(l, r)                                                 \
    const size_t l_size = l.get_word_count();                        \
    const size_t r_size = r.get_word_count();                        \
    copy_words(stash_ptr, l.words, l_size);                          \
    clear_words(l.words, l_size + r_size);                           \
    multiply_karatsuba(stash_ptr, r.words, l_size, r_size, l.words); \
    l.flags = find_head(l.words, l_size + r_size - 1) << 2U

#define SQUARE_OP(l)                              \
    const size_t l_size = l.get_word_count();     \
    copy_words(stash_ptr, l.words, l_size);       \
    clear_words(l.words, 2 * l_size);             \
    square_karatsuba(stash_ptr, l_size, l.words); \
    l.flags = find_head(l.words, 2 * l_size - 1) << 2U

    template <typename word_t>
    Integer<word_t> power(const Integer<word_t> &base, size_t exponent) noexcept
    {
        if (!exponent)
            return Integer<word_t>(1);
        const size_t prodsize = base.get_word_count() * exponent;
        word_t *local_workspace = allocate_words<word_t>(prodsize * 2);
        word_t *p_ptr = allocate_words<word_t>(prodsize);
        word_t *q_ptr = local_workspace;
        word_t *stash_ptr = local_workspace + prodsize;
        copy_words(p_ptr, base.words, base.get_word_count());
        copy_words(q_ptr, base.words, base.get_word_count());
        Integer<word_t> p(p_ptr, base.flags);
        Integer<word_t> q(q_ptr, base.flags & OWNERSHIP_OFF);
        size_t j = get_trailing_zero_bits(exponent);
        exponent >>= j;
        const word_t p_is_negative = p.is_negative();
        while (exponent >= 2)
        {
            exponent >>= 1U;
            SQUARE_OP(q);
            if (exponent & 1)
            {
                MUL_OP(p, q);
            }
        }
        while (j--)
        {
            SQUARE_OP(p);
        }
        return free(local_workspace), p.flags = p_is_negative | 0b10 | p.flags, p;
    }

    template <typename word_t>
    void egcd(const Integer<word_t> &x, const Integer<word_t> &y, Integer<word_t> *r = nullptr, Integer<word_t> *s = nullptr,
              Integer<word_t> *t = nullptr, word_t *workspace = nullptr, EGCDFlags *flags_ptr = nullptr) noexcept
    {
        const size_t max_size = 1 + (x.get_head() > y.get_head() ? x.get_head() : y.get_head());
        word_t *local_workspace = !workspace ? allocate_words<word_t>(max_size * 2 * 8 + max_size + 4) : workspace;

        const size_t offset = 2 * max_size;
        word_t *r0_ptr = local_workspace;
        word_t *r1_ptr = r0_ptr + offset;
        copy_words(r0_ptr, x.words, x.get_word_count());
        copy_words(r1_ptr, y.words, y.get_word_count());
        Integer<word_t> r0(r0_ptr, x.get_head() << 2U);
        Integer<word_t> r1(r1_ptr, y.get_head() << 2U);
        word_t *s0_ptr = r1_ptr + offset;
        word_t *s1_ptr = s0_ptr + offset;
        *s0_ptr = 1;
        Integer<word_t> s0(s0_ptr, 0);
        Integer<word_t> s1(s1_ptr, 0);
        word_t *t0_ptr = s1_ptr + offset;
        word_t *t1_ptr = t0_ptr + offset;
        *t1_ptr = 1;
        Integer<word_t> t0(t0_ptr, 0);
        Integer<word_t> t1(t1_ptr, 0);
        word_t *tmp_ptr = t1_ptr + offset;
        word_t *tmp_prod_ptr = tmp_ptr + offset;
        Integer<word_t> tmp(tmp_ptr, 0);
        Integer<word_t> tmp_prod(tmp_prod_ptr, 0);
        word_t *quot_ptr = tmp_prod_ptr + offset;
        Integer<word_t> q(quot_ptr, 0);

        auto swap_integers = [](Integer<word_t> &l, Integer<word_t> &r)
        {
            word_t *stash_ptr = l.words;
            const size_t stash_flags = l.flags;
            l.words = r.words;
            l.flags = r.flags;
            r.words = stash_ptr;
            r.flags = stash_flags;
        };

        auto euklid_step = [&swap_integers, &tmp, &tmp_prod, &q](Integer<word_t> &x0, Integer<word_t> &x1)
        {
            copy_words(tmp.words, x0.words, x0.get_word_count());
            tmp.flags = x0.flags;
            const size_t tmp_head = tmp.get_head();
            const bool tmp_is_negative = tmp.is_negative();
            clear_words(tmp_prod.words, q.get_word_count() + x1.get_word_count());
            multiply_karatsuba(q.words, x1.words, q.get_word_count(), x1.get_word_count(), tmp_prod.words);
            const bool tmp_prod_is_negative = q.is_negative() != x1.is_negative();
            const size_t tmp_prod_head = find_head(tmp_prod.words, q.get_head() + x1.get_word_count());
            tmp_prod.flags = tmp_prod_is_negative | (tmp_prod_head << 2U);
            swap_integers(x0, x1);
            if (tmp_is_negative == tmp_prod_is_negative) // x1 = |tmp| - |tmp_prod| or x1 = -|tmp| + |tmp_prod|
            {
                const word_t *ptr_selector[2] = {tmp.words, tmp_prod.words};
                const size_t head_selector[2] = {tmp_head, tmp_prod_head};
                const bool l_geq_r = l_abs_geq_r_abs(ptr_selector[tmp_is_negative], ptr_selector[!tmp_is_negative],
                                                     head_selector[tmp_is_negative], head_selector[!tmp_is_negative]);
                size_t x1_head;
                subtract_words(ptr_selector[tmp_is_negative ? l_geq_r : !l_geq_r],
                               ptr_selector[tmp_is_negative ? !l_geq_r : l_geq_r],
                               head_selector[tmp_is_negative ? l_geq_r : !l_geq_r],
                               head_selector[tmp_is_negative ? !l_geq_r : l_geq_r], x1.words, x1_head);
                x1.flags = (!l_geq_r) | (x1_head << 2U);
            }
            else // x1 = -|tmp| - |tmp_prod| or x1 = |tmp| + |tmp_prod|
            {
                const bool check = tmp_head >= tmp_prod_head;
                const size_t bigger_head = check ? tmp_head : tmp_prod_head;
                add_words(check ? tmp.words : tmp_prod.words, check ? tmp_prod.words : tmp.words, bigger_head + 1,
                          check ? tmp_prod_head + 1 : tmp_head + 1, x1.words);
                const size_t x1_head = x1.words[bigger_head + 1] ? bigger_head + 1 : bigger_head;
                x1.flags = (!(!tmp_is_negative && tmp_prod_is_negative)) | (x1_head << 2U);
            }
        };

        if (r1 > r0)
        { // avoid pointless first division
            swap_integers(r0, r1);
            swap_integers(s0, s1);
            swap_integers(t0, t1);
        }

        while (!r1.is_zero())
        {
            const size_t r0_head = r0.get_head();
            const size_t r1_head = r1.get_head();
            const size_t K = r0_head + 5;
            if (r1_head > r0_head || r1 > r0)
                q.flags = (*quot_ptr = 0, 0);
            else
            {
                clear_words(quot_ptr, K);
                word_t *divmod_workspace;
                const bool require_allocation = 3 * K > BufferSize<word_t>::divmod;
                divmod_workspace = !require_allocation ? clear_words((word_t *)divmod_buffer, 3 * K), (word_t *)divmod_buffer
                                                       : allocate_words<word_t>(3 * K);
                const size_t quot_flags = divmod(r0.words, r0_head, r1.words, r1_head, quot_ptr, divmod_workspace, K);
                q.flags = (r0.is_negative() != r1.is_negative()) | (quot_flags & SIGN_OFF_OWNERSHIP_OFF);
                require_allocation ? free(divmod_workspace) : (void)0;
            }
            euklid_step(r0, r1);
            euklid_step(s0, s1);
            euklid_step(t0, t1);
        }

        // ensure that the gcd is non-negative
        if (r0.is_negative())
        {
            r0.flags = r0.flags >> 2U;
            s0.flags = (!s0.is_negative()) | (s0.get_head() << 2U);
            t0.flags = (!t0.is_negative()) | (t0.get_head() << 2U);
        }

        if (r)
            *r = r0;
        if (s)
            *s = s0;
        if (t)
            *t = t0;

        if (!workspace)
            free(local_workspace);
        else if (flags_ptr)
        {
            const word_t location_encoding =
                (r0.words == r0_ptr ? 0 : 1) |
                ((s0.words == s0_ptr ? 0 : 1) << 1U) |
                ((t0.words == t0_ptr ? 0 : 1) << 2U);
            flags_ptr->r0_flags = r0.flags;
            flags_ptr->s0_flags = s0.flags;
            flags_ptr->t0_flags = t0.flags;
            flags_ptr->location_encoding = location_encoding;
        }
    }

    template <typename word_t>
    Integer<word_t> gcd(const Integer<word_t> &l, const Integer<word_t> &r) noexcept
    {
        Integer<word_t> out;
        return egcd(l, r, &out), out.flags &= SIGN_OFF, out; // ensure the result is non-negative
    }

    template <typename word_t>
    Integer<word_t> lcm(const Integer<word_t> &l, const Integer<word_t> &r) noexcept
    {
        Integer<word_t> tmp, out;
        return egcd(l, r, &tmp), out = (l * r) / tmp, out.flags &= SIGN_OFF, out; // ensure the result is non-negative
    }

#define BARRETT_OP(x, modulus)                                                                                \
    if (x.get_head() >= k - 1)                                                                                \
    {                                                                                                         \
        Integer barrett(barrett_ptr, 0);                                                                      \
        clear_words(stash_ptr, x.get_head() + 2 - k + mue_size);                                              \
        multiply_karatsuba(x.words + k - 1, mue.words, x.get_head() + 2 - k, mue_size, stash_ptr);            \
        barrett.flags = find_head(stash_ptr, x.get_head() + 1 - k + mue_size) << 2U;                          \
        if (barrett.get_head() >= k + 1)                                                                      \
        {                                                                                                     \
            barrett.flags = (barrett.get_head() - k - 1) << 2U;                                               \
            clear_words(barrett_ptr, barrett.get_word_count() + k);                                           \
            multiply_karatsuba(stash_ptr + k + 1, modulus.words, barrett.get_word_count(), k, barrett.words); \
            barrett.flags = find_head(barrett.words, barrett.get_head() + k) << 2U;                           \
            inplace_decrement(x.words, barrett.words, barrett.get_word_count());                              \
            x.flags = (find_head(x.words, x.get_head()) << 2U);                                               \
        }                                                                                                     \
    }                                                                                                         \
    inplace_decrement(x.words, modulus.words, x >= modulus ? k : 0);                                          \
    x.flags = find_head(x.words, x.get_head()) << 2U;                                                         \
    inplace_decrement(x.words, modulus.words, x >= modulus ? k : 0);                                          \
    x.flags = find_head(x.words, x.get_head()) << 2U

#define BARRETT_SQUARE(x, modulus) \
    SQUARE_OP(x);                  \
    BARRETT_OP(x, modulus)

#define BARRETT_MUL(x, y, modulus) \
    MUL_OP(x, y);                  \
    BARRETT_OP(x, modulus)

    template <typename word_t>
    Integer<word_t> modular_power(const Integer<word_t> &base, const Integer<word_t> &exponent,
                                  const Integer<word_t> &modulus) noexcept
    {
        if (exponent.is_zero()) // convention: 0^0 == 1 !
            return Integer<word_t>(1);
        if (base.is_zero())
            return Integer<word_t>(0);

        const size_t base_size = base.get_word_count();
        const size_t modulus_size = modulus.get_word_count();
        const size_t prodsize = base_size > modulus_size ? base_size * 2 + 4 : modulus_size * 2 + 4;
        const size_t barrett_size = prodsize * 2;
        const size_t exponent_size = exponent.get_word_count();
        const size_t egcd_workspace_size = exponent.is_negative() ? 4 + 2 * 8 * (prodsize - 4) / 2 + (prodsize - 4) / 2 + 4
                                                                  : 0;
        const size_t local_workspace_size =
            prodsize * 2 + exponent_size + egcd_workspace_size + barrett_size + modulus_size * 2 + 1;
        word_t *p_ptr = allocate_words<word_t>(prodsize);
        word_t *local_workspace = allocate_words<word_t>(local_workspace_size);
        word_t *q_ptr = local_workspace;
        word_t *stash_ptr = q_ptr + prodsize;
        word_t *d_ptr = stash_ptr + prodsize;
        word_t *egcd_workspace = d_ptr + exponent_size;
        word_t *barrett_ptr = egcd_workspace + egcd_workspace_size;
        word_t *mue_ptr = barrett_ptr + barrett_size;
        copy_words(d_ptr, exponent.words, exponent_size);
        mue_ptr[modulus_size * 2] = 1;
        *p_ptr = 1;

        Integer<word_t> p(p_ptr, 0);
        Integer<word_t> q(q_ptr, 0);
        Integer<word_t> d(d_ptr, exponent.flags & SIGN_OFF_OWNERSHIP_OFF);
        Integer<word_t> mue(mue_ptr, (modulus_size * 2) << 2U);
        mue /= modulus; //  mue = (base^(2*k)) / modulus
        const size_t mue_size = mue.get_word_count();
        const size_t k = modulus.get_word_count();

        const auto mod_op = [&prodsize, &modulus, &stash_ptr](Integer<word_t> &s)
        {
            const size_t l_head = s.get_head();
            const size_t r_head = modulus.get_head();
            const size_t K = l_head + 5;
            word_t *remainder_ptr = s.words;
            const size_t remainder_size = prodsize;
            copy_words(stash_ptr, s.words, s.get_word_count());
            const bool s_is_negative = s.is_negative();

            if (r_head > l_head)
            {
                if (s_is_negative)
                {
                    clear_words(remainder_ptr, remainder_size);
                    copy_words(remainder_ptr, modulus.words, r_head + 1);
                    inplace_decrement(remainder_ptr, stash_ptr, s.get_word_count());
                    s.flags = find_head(remainder_ptr, r_head) << 2U;
                }
                return;
            }
            word_t *divmod_workspace;
            const bool require_allocation = 3 * K > BufferSize<word_t>::divmod;
            divmod_workspace = !require_allocation ? clear_words((word_t *)divmod_buffer, 3 * K), (word_t *)divmod_buffer
                                                   : allocate_words<word_t>(3 * K);
            clear_words(remainder_ptr, remainder_size);
            const size_t remainder_flags =
                divmod(stash_ptr, l_head, modulus.words, r_head, remainder_ptr, divmod_workspace, K, true);
            if (s_is_negative)
            { // reuse workspace
                clear_words(divmod_workspace, r_head + 1);
                copy_words(divmod_workspace, modulus.words, r_head + 1);
                inplace_decrement(divmod_workspace, remainder_ptr, 1 + (remainder_flags >> 2U));
                clear_words(remainder_ptr, remainder_size);
                copy_words(remainder_ptr, divmod_workspace, r_head + 1);
                s.flags = find_head(remainder_ptr, r_head) << 2U;
            }
            else
                s.flags = remainder_flags & SIGN_OFF_OWNERSHIP_OFF;
            require_allocation ? free(divmod_workspace) : (void)0;
        };

        if (exponent.is_negative())
        {
            clear_words(egcd_workspace, egcd_workspace_size);
            EGCDFlags flags;
            egcd<word_t>(base, modulus, nullptr, nullptr, nullptr, egcd_workspace, &flags);
            const size_t offset = 2 * (base_size > modulus_size ? base_size : modulus_size);
            word_t *r_ptr = flags.location_encoding & 0b001 ? egcd_workspace + offset : egcd_workspace;
            word_t *s_ptr =
                flags.location_encoding & 0b010 ? egcd_workspace + 3 * offset : egcd_workspace + 2 * offset;

            Integer<word_t> r(r_ptr, flags.r0_flags & OWNERSHIP_OFF); // explicitly disable ownership to make compiler happy
            Integer<word_t> s(s_ptr, flags.s0_flags & OWNERSHIP_OFF); // explicitly disable ownership to make compiler happy
            if (!(r.get_head() == 0 && r.words[0] == 1))
                return (free(local_workspace), p.flags = 0b10, *p.words = 0, p);
            const size_t s_size = s.get_word_count();
            if (s_size >= modulus.get_word_count())
                mod_op(s);

            copy_words(q.words, s.words, s_size);
            q.flags = s.get_head() << 2U;
        }
        else
        {
            copy_words(q.words, base.words, base_size);
            q.flags = base.flags & OWNERSHIP_OFF;
            if (q.get_word_count() >= modulus.get_word_count())
                mod_op(q);
        }

        // precomputation
        const int32_t window_size = 6;
        const size_t precomp_size = MPA_SHIFTBASE << (window_size - 1);
        const bool lookup_requires_alloc = precomp_size * prodsize > BufferSize<word_t>::power;
        word_t *lookup_table = !lookup_requires_alloc ? (word_t *)power_buffer : allocate_words<word_t>(precomp_size * prodsize);
        clear_words(lookup_table, precomp_size * prodsize);
        copy_words(lookup_table, q.words, q.get_word_count());
        memcpy(lookup_table + modulus_size, &q.flags, sizeof(size_t));
        BARRETT_SQUARE(q, modulus);
        const size_t effective_base_squared_size = q.get_word_count();
        for (size_t j = 1; j < precomp_size; ++j)
        {
            word_t *src = lookup_table + (j - 1) * prodsize;
            word_t *target = lookup_table + j * prodsize;
            size_t src_size = (memcpy(&src_size, src + modulus_size, sizeof(size_t)), (src_size >> 2U) + 1);
            multiply_karatsuba(src, q.words, src_size, effective_base_squared_size, target);
            Integer<word_t> tmp(target, find_head(target, src_size + effective_base_squared_size - 1) << 2U);
            BARRETT_OP(tmp, modulus);
            memcpy(target + modulus_size, &tmp.flags, sizeof(size_t));
        }
        // main loop
        int64_t i = exponent.get_bit_count() - 1;
        while (i >= 0)
        {
            if (!(exponent.words[i / BitsInWord<word_t>::value] & (MPA_SHIFTBASE << (i & (BitsInWord<word_t>::value - 1))))) // (!exponent.get_bit(i))
            {
                BARRETT_SQUARE(p, modulus);
                i -= 1;
            }
            else
            {
                word_t window = 0;
                int64_t l = 0;
                size_t window_width = 0;
                size_t right_most_possible = window_size < i + 1 ? 0 : window_size - i - 1;
                bool found_l = false;
                for (int64_t j = right_most_possible; j < window_size; ++j)
                {
                    size_t index = i - window_size + 1 + j;
                    word_t component = (exponent.words[index / BitsInWord<word_t>::value] &
                                        (MPA_SHIFTBASE << (index & (BitsInWord<word_t>::value - 1)))) > 0; // exponent.get_bit(index)
                    if (!found_l && component)
                        found_l = (l = index, true);
                    window |= (component << window_width), window_width += (window > 0);
                }
                for (int64_t x = 0; x < i - l + 1; ++x)
                {
                    BARRETT_SQUARE(p, modulus);
                }
                word_t *looked_up_ptr = lookup_table + (window >> 1) * prodsize;
                size_t looked_up_flags;
                memcpy(&looked_up_flags, looked_up_ptr + modulus_size, sizeof(size_t));
                Integer<word_t> looked_up(looked_up_ptr, looked_up_flags);
                BARRETT_MUL(p, looked_up, modulus);
                i = l - 1;
            }
        }

        lookup_requires_alloc ? free(lookup_table), free(local_workspace) : free(local_workspace);
        return p.flags = 0b10 | (p.get_head() << 2U), p;
    }

    template <typename word_t>
    Integer<word_t> modular_inverse(const Integer<word_t> &N, const Integer<word_t> &modulus) noexcept
    {
        word_t one = 1;
        Integer<word_t> exponent(&one, 1); // <-- exponent is -1, no ownership as the buffer lives on the stack
        return modular_power<word_t>(N, exponent, modulus);
    }

    template <typename word_t>
    bool is_probably_prime(const Integer<word_t> candidate, const size_t steps = 32, word_t *workspace = nullptr) noexcept
    {
        if (!candidate.get_head() && (candidate.words[0] == 2 || candidate.words[0] == 3))
            return true; // handle some small primes first

        const size_t wordcount = candidate.get_word_count();
        const size_t max_prodsize = wordcount * 2 + 4;
        const size_t workspace_buffer_size = max_prodsize * 6 + wordcount * 3 + 1;
        word_t *workspace_buffer = workspace ? workspace : allocate_words<word_t>(workspace_buffer_size);
        word_t *p_ptr = workspace_buffer;
        word_t *q_ptr = workspace_buffer + max_prodsize;
        word_t *stash_ptr = workspace_buffer + max_prodsize * 2;
        word_t *c_ptr = workspace_buffer + max_prodsize * 3;
        word_t *barrett_ptr = workspace_buffer + max_prodsize * 4;
        word_t *candidate_buffer = workspace_buffer + max_prodsize * 6;
        word_t *mue_ptr = workspace_buffer + max_prodsize * 6 + wordcount;

        clear_words(mue_ptr, 2 * wordcount);
        mue_ptr[2 * wordcount] = 1;
        Integer<word_t> mue(mue_ptr, (2 * wordcount) << 2U);
        mue /= candidate; //  mue = (b**(2*k)) // modulus
        const size_t mue_size = mue.get_word_count();
        const size_t &k = wordcount;
        copy_words(c_ptr, candidate.words, wordcount);
        Integer<word_t> c(c_ptr, candidate.flags & OWNERSHIP_OFF);
        c -= 1;
        const int64_t base_j = get_trailing_zero_bits(c.words, c.get_head());
        const Integer<word_t> &modulus = candidate;
        const size_t modulus_size = modulus.get_word_count();
        const Integer<word_t> limit = candidate - 2;

        // prepare lookup tables for sliding window exponentiation
        const int32_t window_size = 6;
        const size_t precomp_size = MPA_SHIFTBASE << (window_size - 1);
        const bool lookup_requires_alloc = precomp_size * max_prodsize > BufferSize<word_t>::power;
        word_t *lookup_table = !lookup_requires_alloc ? (word_t *)power_buffer : allocate_words<word_t>(precomp_size * max_prodsize);
        const size_t exponent_bitcount = c.get_bit_count();
        std::vector<size_t> exponent_windows;
        exponent_windows.reserve(exponent_bitcount);
        int64_t pre_bit_pos = exponent_bitcount - 1;
        while (pre_bit_pos >= base_j)
        {
            if (!(c.words[pre_bit_pos / BitsInWord<word_t>::value] & (MPA_SHIFTBASE << (pre_bit_pos & (BitsInWord<word_t>::value - 1)))))
                pre_bit_pos -= 1;
            else
            {
                size_t window = 0;
                int64_t l = 0;
                size_t window_width = 0;
                size_t right_most_possible = window_size < pre_bit_pos + 1 ? 0 : window_size - pre_bit_pos - 1;
                bool found_l = false;
                for (int64_t j = right_most_possible; j < window_size; ++j)
                {
                    size_t index = pre_bit_pos - window_size + 1 + j;
                    word_t component = (c.words[index / BitsInWord<word_t>::value] & (MPA_SHIFTBASE << (index & (BitsInWord<word_t>::value - 1)))) > 0;
                    if (!found_l && component)
                        found_l = (l = index, true);
                    window |= (component << window_width), window_width += (window > 0);
                }
                exponent_windows.push_back(window | (l << 8U)), pre_bit_pos = l - 1;
            }
        }

        const auto cleanup = [lookup_requires_alloc, workspace, workspace_buffer, lookup_table]()
        {
            !workspace ? free(workspace_buffer) : (void)0;
            lookup_requires_alloc ? free(lookup_table) : (void)0;
        };

        for (size_t i = 0; i < steps; ++i)
        {
            // get a testing candidate and prepare check
            bool miller_rabin_step_passed = false;
            Integer<word_t> a = Integer<word_t>::get_random(limit, candidate_buffer);
            *p_ptr = 1;
            *(p_ptr + 1) = 0; // just making sure
            copy_words(q_ptr, a.words, a.get_word_count());
            Integer<word_t> p(p_ptr, 0);
            Integer<word_t> q(q_ptr, a.flags & OWNERSHIP_OFF);

            // precomputation for sliding window
            clear_words(lookup_table, precomp_size * max_prodsize);
            copy_words(lookup_table, q.words, q.get_word_count());
            memcpy(lookup_table + modulus_size, &q.flags, sizeof(size_t));
            BARRETT_SQUARE(q, modulus);
            const size_t base_squared_size = q.get_word_count();
            for (size_t j = 1; j < precomp_size; ++j)
            {
                word_t *src = lookup_table + (j - 1) * max_prodsize;
                word_t *target = lookup_table + j * max_prodsize;
                size_t src_size = (memcpy(&src_size, src + modulus_size, sizeof(size_t)), (src_size >> 2U) + 1);
                multiply_karatsuba(src, q.words, src_size, base_squared_size, target);
                Integer<word_t> tmp(target, find_head(target, src_size + base_squared_size - 1) << 2U);
                BARRETT_OP(tmp, modulus);
                memcpy(target + modulus_size, &tmp.flags, sizeof(size_t));
            }

            // main loop for sliding window: computes p = a^d, where d = (c >> base_j)
            int64_t bit_pos = exponent_bitcount - 1;
            size_t window_count = 0;
            while (bit_pos >= base_j)
            {
                if (!(c.words[bit_pos / BitsInWord<word_t>::value] & (MPA_SHIFTBASE << (bit_pos & (BitsInWord<word_t>::value - 1)))))
                {
                    BARRETT_SQUARE(p, modulus);
                    bit_pos -= 1;
                }
                else
                {
                    word_t window = exponent_windows[window_count] & 0xFF;
                    int64_t l = exponent_windows[window_count] >> 8U;
                    for (int64_t x = 0; x < bit_pos - l + 1; ++x)
                    {
                        BARRETT_SQUARE(p, modulus);
                    }
                    word_t *looked_up_ptr = lookup_table + (window >> 1) * max_prodsize;
                    size_t looked_up_flags;
                    memcpy(&looked_up_flags, looked_up_ptr + modulus_size, sizeof(size_t));
                    Integer<word_t> looked_up(looked_up_ptr, looked_up_flags);
                    BARRETT_MUL(p, looked_up, modulus);
                    bit_pos = l - 1;
                    window_count += 1;
                }
            }
            if ((!p.get_head() && p.words[0] == 1) || p == c)
                miller_rabin_step_passed = true;

            // check a^(2^r*d) for 0 <= r < base_j
            size_t j = base_j;
            while (!miller_rabin_step_passed && j > 1 && (p.get_head() || p.words[0] > 1))
            {
                j -= 1;
                BARRETT_SQUARE(p, modulus);
                if (p == c)
                    miller_rabin_step_passed = true;
            }
            if (!miller_rabin_step_passed)
                return cleanup(), false;
        }
        return cleanup(), true;
    }

    template <typename word_t>
    Integer<word_t> get_random_prime(const size_t wordcount, bool verbose = false) noexcept
    {
        const size_t max_prodsize = wordcount * 2 + 4;
        const size_t workspace_buffer_size = max_prodsize * 6 + wordcount * 3 + 1;
        word_t *workspace = allocate_words<word_t>(workspace_buffer_size);
        word_t *buffer = allocate_words<word_t>(wordcount);
        Integer<word_t> p(Integer<word_t>::get_random(wordcount, false, buffer));
        const auto refresh_memory = [&p]()
        {
            for (size_t j = 0; j < sieve_size; ++j)
            {
                const word_t modulus = primes_sieve[j];
                word_t output = p.words[0] % modulus;
                const word_t base_factor = ((std::numeric_limits<word_t>::max() % modulus) + 1) % modulus;
                word_t current_base_modulus = base_factor;
                size_t i = 1;
                for (i = 1; i < p.get_head(); ++i)
                {
                    output = (output + (p.words[i] % modulus) * current_base_modulus) % modulus;
                    current_base_modulus = (current_base_modulus * base_factor) % modulus;
                }
                primes_memory[j] = (output + (p.words[i] % modulus) * current_base_modulus) % modulus;
            }
        };
        const auto prepare_p = [&p]()
        {
            p.words[0] |= 1;                                       // make sure p is odd
            p.words[p.get_word_count() - 1] |= MSB<word_t>::value; // make sure p has msb set
            word_t p_mod_3 = p.words[0] % 3;
            for (size_t j = 1; j < p.get_word_count(); ++j)
                p_mod_3 = (p_mod_3 + (p.words[j] % 3)) % 3; // we use 2^64k = (-1)^64k = 1 (mod 3)
            switch (p_mod_3)
            {
            case 0: // p = 3 mod 6
                p += 4;
                break;
            case 2: // p = 5 mod 6
                p += 2;
                break;
            default: // nothing to do if p = 1 mod 6
                break;
            }
        };
        prepare_p(), refresh_memory();
        int64_t step = 0;
        int64_t j = 0;
        word_t memory_step = 0;
        while (true)
        {
            bool composite = false;
            for (size_t i = 0; !composite && i < sieve_size; ++i)
                composite = !((primes_memory[i] + memory_step) % primes_sieve[i]);
            if (!composite)
            {
                p += step, step = 0;
                if (is_probably_prime(p, 64, workspace))
                    return verbose ? (std::cout << "iterations : " << j << "\n", free(workspace), p.flags |= 0b10, p)
                                   : (free(workspace), p.flags |= 0b10, p);
            }
            const size_t increment = !(j & 1) ? 4 : 2;
            step += increment, memory_step += increment, j++;
            if (memory_step >= std::numeric_limits<int16_t>::max()) // p grew too large: reset and try again
                p = Integer<word_t>::get_random(wordcount, false, buffer), prepare_p(), refresh_memory(), step = memory_step = j = 0;
        }
    }
}
