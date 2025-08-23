/*
    COPYRIGHT: David Geis 2025
    LICENSE:   MIT
    CONTACT:   davidgeis@web.de
*/

#pragma once

#include <array>
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
#define MPA_DIVMOD_BUFFER_SIZE 2048
#endif

#ifndef MPA_POWER_BUFFER_SIZE
#define MPA_POWER_BUFFER_SIZE 2048
#endif

#ifndef MPA_KARATSUBA_BUFFER_SIZE
#define MPA_KARATSUBA_BUFFER_SIZE 2048
#endif

namespace MPA
{

    struct ExtendedGcdFlags
    {
        size_t r0_flags = 0;
        size_t s0_flags = 0;
        size_t t0_flags = 0;
        size_t location_encoding = 0;
    };

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

        using dword_t = decltype([]()
                                 {
                                     if constexpr (std::is_same_v<word_t, uint16_t>)
                                         return static_cast<uint32_t>(1);
                                     else if constexpr (std::is_same_v<word_t, uint32_t>)
                                         return static_cast<uint64_t>(1);
#if defined(__SIZEOF_INT128__) && !defined(_MSC_VER)
                                     else if constexpr (std::is_same_v<word_t, uint64_t>)
                                         return static_cast<__uint128_t>(1);
#endif
                                 }());
    public:
        static word_t *allocate_words(const size_t word_count) noexcept
        {
            word_t *buffer;
            if (!(buffer = (word_t *)(calloc(word_count, sizeof(word_t)))))
                std::cerr << "ERROR: OUT OF MEMORY, ABORT !\n", abort();
            return buffer;
        }

        static void clear_words(word_t *dst, const size_t wordcount) noexcept
        {
            memset(dst, 0, wordcount * sizeof(word_t));
        }

        static void copy_words(word_t *dst, const word_t *src, const size_t wordcount) noexcept
        {
            memcpy(dst, src, wordcount * sizeof(word_t));
        }

        static void move_words(word_t *dst, const word_t *src, const size_t wordcount) noexcept
        {
            memmove(dst, src, wordcount * sizeof(word_t));
        }

        static constexpr size_t SIGN_OFF = std::numeric_limits<size_t>::max() - 1;
        static constexpr size_t OWNERSHIP_OFF = std::numeric_limits<size_t>::max() - 2;
        static constexpr size_t SIGN_OFF_OWNERSHIP_OFF = std::numeric_limits<size_t>::max() - 3;
        static constexpr size_t bits_in_word = sizeof(word_t) << 3;
        static constexpr word_t msb = MPA_SHIFTBASE << (bits_in_word - 1);

    private:
        static constexpr size_t multable_max_wordsize = 18;
        static constexpr size_t sieve_size = 2048;
        static constexpr size_t divmod_buffer_size = MPA_DIVMOD_BUFFER_SIZE;
        static constexpr size_t power_buffer_size = MPA_POWER_BUFFER_SIZE;
        static constexpr size_t karatsuba_buffer_size = MPA_KARATSUBA_BUFFER_SIZE;
        inline static thread_local word_t divmod_buffer[divmod_buffer_size] = {0};
        inline static thread_local word_t power_buffer[power_buffer_size] = {0};
        inline static thread_local word_t karatsuba_buffer[karatsuba_buffer_size] = {0};
        inline static thread_local size_t karatsuba_buffer_offset = 0;

        struct Multable
        {
            typedef void (*mulfunction)(const word_t *l, const word_t *r, word_t *out);

            template <size_t lsize, size_t rsize>
            static void multiply_words(const word_t *l, const word_t *r, word_t *out) noexcept
            {
                for (size_t i = 0; i < rsize; ++i)
                {
                    dword_t x = 0;
                    for (size_t j = 0; j < lsize; ++j)
                        out[i + j] = (x = (dword_t)r[i] * l[j] + (x >> bits_in_word) + out[i + j]) &
                                     std::numeric_limits<word_t>::max();
                    out[i + lsize] = x >> bits_in_word;
                }
            }

            static constexpr mulfunction funcs[multable_max_wordsize * (multable_max_wordsize + 1) >> 1U] = {
                multiply_words<1, 1>,
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
            };
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
        static void multiply_karatsuba(const word_t *l, const word_t *r, const size_t lsize, const size_t rsize, word_t *out) noexcept
        {
            static constexpr uint8_t index_lookup[19] = {0, 0, 1, 3, 6, 10, 15, 21, 28, 36,
                                                         45, 55, 66, 78, 91, 105, 120, 136, 153};
            if (lsize <= multable_max_wordsize && rsize <= multable_max_wordsize)
                return lsize >= rsize ? Multable::funcs[index_lookup[lsize] + rsize - 1](l, r, out)
                                      : Multable::funcs[index_lookup[rsize] + lsize - 1](r, l, out);
            const size_t m = lsize > rsize ? lsize >> 1U : rsize >> 1U;
            const bool lsize_short = lsize <= m, rsize_short = rsize <= m;
            const size_t l_high_size = !lsize_short ? lsize - m : 0, l_low_size = !lsize_short ? m : lsize;
            const word_t *l_high = !lsize_short ? l + m : nullptr;
            const size_t r_high_size = !rsize_short ? rsize - m : 0, r_low_size = !rsize_short ? m : rsize;
            const word_t *r_high = !rsize_short ? r + m : nullptr, *l_low = l, *r_low = r;
            const bool check_l = l_low_size > l_high_size, check_r = r_low_size > r_high_size;
            size_t sum_l_size = check_l ? l_low_size + 1 : l_high_size + 1, sum_r_size = check_r ? r_low_size + 1 : r_high_size + 1;
            size_t z1_size = sum_l_size + sum_r_size;
            const size_t needed_scratch_words = z1_size << 1U;
            const bool dont_require_allocation = karatsuba_buffer_size > needed_scratch_words + karatsuba_buffer_offset;
            word_t *z0 = out, *tmp;
            word_t *z1 = dont_require_allocation ? (tmp = karatsuba_buffer + karatsuba_buffer_offset, clear_words(tmp, needed_scratch_words),
                                                    karatsuba_buffer_offset += needed_scratch_words, tmp)
                                                 : allocate_words(needed_scratch_words);
            word_t *z2 = l_high && r_high ? out + 2 * m : nullptr, *sum_l = z1 + z1_size, *sum_r = sum_l + sum_l_size;
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
        static void square_karatsuba(const word_t *l, const size_t lsize, word_t *out) noexcept
        {
            static constexpr uint8_t index_lookup[19] = {0, 0, 2, 5, 9, 14, 20, 27, 35, 44,
                                                         54, 65, 77, 90, 104, 119, 135, 152, 170};
            if (lsize <= multable_max_wordsize)
                return Multable::funcs[index_lookup[lsize]](l, l, out);
            const size_t m = lsize >> 1U, l_high_size = lsize - m, l_low_size = m;
            const word_t *l_high = l + m, *l_low = l;
            size_t sum_l_size = l_high_size + 1;
            const size_t needed_scratch_words = 4 * sum_l_size;
            const bool dont_require_allocation = karatsuba_buffer_size > needed_scratch_words + karatsuba_buffer_offset;
            word_t *z0 = out, *tmp;
            word_t *z1 = dont_require_allocation ? (tmp = karatsuba_buffer + karatsuba_buffer_offset, clear_words(tmp, needed_scratch_words),
                                                    karatsuba_buffer_offset += needed_scratch_words, tmp)
                                                 : allocate_words(needed_scratch_words);
            size_t z1_size = 2 * sum_l_size;
            word_t *z2 = out + 2 * m, *sum_l = z1 + z1_size;
            square_karatsuba(l_low, l_low_size, z0);
            const bool carry_l = add_words(l_high, l_low, l_high_size, l_low_size, sum_l);
            z1_size -= 2 * (!carry_l), sum_l_size -= !carry_l;
            square_karatsuba(sum_l, sum_l_size, z1), inplace_decrement(z1, z0, 2 * l_low_size);
            square_karatsuba(l_high, l_high_size, z2), inplace_decrement(z1, z2, 2 * l_high_size);
            inplace_increment(out + m, z1, 2 * lsize - m < z1_size ? 2 * lsize - m : z1_size);
            dont_require_allocation ? (void)(karatsuba_buffer_offset -= needed_scratch_words) : free(z1);
        }

        static size_t find_head(const word_t *l, const size_t start_point) noexcept
        {
            size_t i = start_point;
            for (i = start_point; !l[i] && i >= 1; --i);
            return i;
        }

        static word_t add_overflow(const word_t a, const word_t b, word_t &overflow) noexcept
        {
            return (overflow = a > std::numeric_limits<word_t>::max() - b, a + b);
        }

        static word_t sub_underflow(const word_t a, const word_t b, word_t &underflow) noexcept
        {
            return (underflow = a < b, a - b);
        }

        static bool add_words(const word_t *bigger, const word_t *smaller, const size_t bigger_size, const size_t smaller_size,
                              word_t *total_sum) noexcept
        {
            word_t carry = 0, current_carry;
            for (size_t i = 0; i < smaller_size; ++i)
            {
                const word_t sum = add_overflow(bigger[i], smaller[i], current_carry);
                total_sum[i] = add_overflow(sum, carry, carry);
                carry += current_carry;
            }
            for (size_t i = smaller_size; i < bigger_size; total_sum[i] = add_overflow(bigger[i], carry, carry), ++i);
            total_sum[bigger_size] = carry;
            return carry;
        }

        static void subtract_words(const word_t *bigger, const word_t *smaller, const size_t bigger_head, const size_t smaller_head,
                                   word_t *total_diff, size_t &total_diff_head) noexcept
        {
            word_t carry = 0, current_carry;
            total_diff_head = 0; // safe guard !
            for (size_t i = 0; i <= smaller_head; ++i)
            {
                const word_t diff = sub_underflow(bigger[i], smaller[i], current_carry);
                total_diff_head = (total_diff[i] = sub_underflow(diff, carry, carry)) ? i : total_diff_head;
                carry += current_carry;
            }
            for (size_t i = smaller_head + 1; i <= bigger_head; ++i)
                total_diff_head = (total_diff[i] = sub_underflow(bigger[i], carry, carry)) ? i : total_diff_head;
        }

        static bool l_abs_geq_r_abs(const word_t *l, const word_t *r, const size_t l_head, const size_t r_head) noexcept
        {
            if (l_head < r_head)
                return false;
            if (l_head > r_head)
                return true;
            size_t i = 0;
            for (i = l_head; i < l_head + 1 && l[i] == r[i]; --i);
            return i < l_head + 1 ? l[i] > r[i] : true;
        }

        #define DO_ADD(l, r, bigger_head, smaller_head)                                                                                                                 \
            add_words(bigger_head == l.get_head() ? l.words : r.words, bigger_head == l.get_head() ? r.words : l.words, bigger_head + 1, smaller_head + 1, out_words);  \
            const size_t out_head = out_words[bigger_head + 1] ? bigger_head + 1 : bigger_head;                                                                         \
            const bool out_sign = l.is_negative();                                                                                                                      \
            return out_sign | (out_head << 2U) // no ownership

        #define DO_SUB(l, r, bigger_head, smaller_head)                                                                                 \
            const bool l_geq_r = l_abs_geq_r_abs(l.words, r.words, l.get_head(), r.get_head());                                         \
            size_t out_head = 0;                                                                                                        \
            subtract_words(l_geq_r ? l.words : r.words, l_geq_r ? r.words : l.words, bigger_head, smaller_head, out_words, out_head);   \
            bool out_sign;                                                                                                              \
            if (!out_head && !out_words[0])                                                                                             \
                out_sign = 0;                                                                                                           \
            else if (!l.is_negative())                                                                                                  \
                out_sign = !l_geq_r;                                                                                                    \
            else                                                                                                                        \
                out_sign = l_geq_r;                                                                                                     \
            return out_sign | (out_head << 2U) // no ownership

        static void inplace_decrement(word_t *minuend, const word_t *subtrahend, const size_t subtrahend_size)
        {
            word_t carry = 0, current_carry;
            size_t j = 0;
            for (j = 0; j < subtrahend_size; ++j)
            {
                const word_t diff = sub_underflow(minuend[j], subtrahend[j], current_carry);
                minuend[j] = sub_underflow(diff, carry, carry);
                carry += current_carry;
            }
            for (; carry; j += (minuend[j] = sub_underflow(minuend[j], carry, carry), 1));
        }

        static void inplace_increment(word_t *final_sum, const word_t *summand, const size_t summand_size) noexcept
        {
            word_t carry = 0, current_carry;
            size_t j = 0;
            for (j = 0; j < summand_size; ++j)
            {
                word_t sum = add_overflow(final_sum[j], summand[j], current_carry);
                final_sum[j] = add_overflow(sum, carry, carry);
                carry += current_carry;
            }
            for (; carry; j += (final_sum[j] = add_overflow(final_sum[j], carry, carry), 1));
        }

        static void multiply_by_doubleword(const word_t *l, const word_t *r, size_t r_size, word_t *out) noexcept
        {
            dword_t x;
            const dword_t l_low = l[0], l_high = l[1];
            for (size_t i = 0; i < r_size; i += 2)
            {
                x = l_low * r[i] + out[i];
                out[i] = x & std::numeric_limits<word_t>::max();
                x = l_high * r[i] + (x >> bits_in_word) + out[i + 1];
                out[i + 1] = x & std::numeric_limits<word_t>::max();
                out[i + 2] = x >> bits_in_word;
                // unroll once
                x = l_low * r[i + 1] + out[i + 1];
                out[i + 1] = x & std::numeric_limits<word_t>::max();
                x = l_high * r[i + 1] + (x >> bits_in_word) + out[i + 2];
                out[i + 2] = x & std::numeric_limits<word_t>::max();
                out[i + 3] = x >> bits_in_word;
            }
        }

        static void multiply_by_word(const word_t l, const word_t *r, const size_t rsize, word_t *out) noexcept
        {
            const dword_t ll = l;
            for (size_t i = 0; i < rsize; ++i)
            {
                dword_t x = ll * r[i] + out[i];
                out[i] = x & std::numeric_limits<word_t>::max();
                out[i + 1] = x >> bits_in_word;
            }
        }

        static size_t get_leading_zero_bits(word_t A) noexcept
        {
            if (!A)
                return bits_in_word;
            size_t count = 0;
            for (; A < (MPA_SHIFTBASE << (bits_in_word - 1)); count += (A <<= 1U, 1));
            return count;
        }

        static size_t get_trailing_zero_bits(word_t A) noexcept
        {
            if (!A)
                return 0;
            size_t count = 0;
            for (; !(A & 1); count += (A >>= 1U, 1));
            return count;
        }

        static size_t get_trailing_zero_bits(const word_t *word_ptr, const size_t head) noexcept
        {
            if (!word_ptr[head])
                return 0;
            size_t count = 0;
            word_t d = 0;
            for (size_t i = 0; !d && i <= head; count += (d = word_ptr[i]) ? 0 : bits_in_word, ++i);
            count += get_trailing_zero_bits(d);
            return count;
        }

        static size_t shift_left_by_words_and_bits(word_t *in_words, size_t in_head, const size_t bits_shift, const size_t words_shift,
                                                   word_t *out_words) noexcept
        {
            if (bits_shift)
            {
                word_t c = 0;
                for (size_t i = 0; i <= in_head; ++i)
                {
                    const word_t tmp = in_words[i];
                    out_words[i] = (tmp << bits_shift) | c;
                    c = tmp >> (bits_in_word - bits_shift);
                }
                in_head += c ? (out_words[in_head + 1] = c, 1) : 0;
                move_words(out_words + words_shift, out_words, in_head + 1);
                clear_words(out_words, words_shift);
            }
            else // no need to call "clear_words" here, because we haven't written anything to out_words
                move_words(out_words + words_shift, in_words, in_head + 1);
            return in_head + words_shift;
        }

        static bool compare_words(const word_t *left, const word_t *right, const size_t size) noexcept
        {
            size_t i = size - 1;
            for (i = size - 1; i < size && left[i] == right[i]; --i);
            return i < size ? left[i] > right[i] : false;
        }

        static size_t divmod(word_t *l_words, const size_t l_head, word_t *y_words, const size_t y_head, word_t *output,
                             word_t *workspace, const size_t K, // requirement: K >= l_head + 5
                             bool need_remainder = false) noexcept
        {
            if (l_head < y_head)
                return (need_remainder) ? (copy_words(output, l_words, l_head + 1), l_head << 2U) : (*output = 0, 0);
            const auto div_two_doublewords_by_one_doubleword = [](const dword_t &AH, const dword_t &AL, const dword_t &B, word_t *q)
            {
                bool overflow;
                const dword_t overflow_barrier =
                    ((std::numeric_limits<word_t>::max()) | (dword_t)(std::numeric_limits<word_t>::max()) << bits_in_word) - B;
                const word_t b1 = B >> bits_in_word;
                dword_t q_tmp = AH / b1;
                dword_t D = q_tmp * (B & std::numeric_limits<word_t>::max());
                dword_t tmp = (AL >> bits_in_word) | ((AH - q_tmp * b1) << bits_in_word);
                if (tmp < D)
                    overflow = tmp > overflow_barrier, q_tmp -= 1, tmp += B, tmp += (!overflow && tmp < D) ? (q_tmp -= 1, B) : 0;
                const dword_t R = tmp - D;
                q[1] = q_tmp, q_tmp = R / b1;
                D = q_tmp * (B & std::numeric_limits<word_t>::max());
                tmp = (AL & std::numeric_limits<word_t>::max()) | ((R - q_tmp * b1) << bits_in_word);
                if (tmp < D)
                    overflow = tmp > overflow_barrier, q_tmp -= 1, tmp += B, q_tmp -= (!overflow && tmp < D);
                q[0] = q_tmp;
            };
            const size_t backshift =
                y_head & 1 ? get_leading_zero_bits(y_words[y_head]) : bits_in_word + get_leading_zero_bits(y_words[y_head]);
            const size_t backshift_words = backshift / bits_in_word, backshift_bits = backshift - backshift_words * bits_in_word;
            word_t *remainder_ptr = need_remainder ? output : workspace, *quot_ptr = need_remainder ? workspace : output;
            size_t n = shift_left_by_words_and_bits(l_words, l_head, backshift_bits, backshift_words, remainder_ptr) + 1;
            n += (n & 1);
            const size_t t = y_head + backshift_words + 1, nn = (n >> 1) - 1, tt = (t >> 1) - 1, offset = n - t;
            word_t *shifted_yabs_ptr = workspace + K, *remainder_correction_ptr = workspace + 2 * K;
            word_t *shifted_remainder_correction_ptr = remainder_correction_ptr + offset;
            shift_left_by_words_and_bits(y_words, y_head, backshift_bits, backshift_words, shifted_yabs_ptr + offset);
            const word_t *initial_yabs_ptr = shifted_yabs_ptr + offset;
            size_t remainder_correction_size = n, shifted_yabs_size = t + offset;
            const dword_t divisor = initial_yabs_ptr[t - 2] | ((dword_t)initial_yabs_ptr[t - 1] << bits_in_word);
            word_t y_checker_words[] = {t > 2 ? initial_yabs_ptr[t - 4] : (word_t)0, t > 2 ? initial_yabs_ptr[t - 3] : (word_t)0,
                                        initial_yabs_ptr[t - 2], initial_yabs_ptr[t - 1]};
            bool check = !compare_words(shifted_yabs_ptr, remainder_ptr, n);
            quot_ptr[offset] += check;
            inplace_decrement(remainder_ptr, shifted_yabs_ptr, check ? shifted_yabs_size : 0);
            const size_t words_to_clear = remainder_correction_size - offset + 2, loop_bound = !tt ? 1 : tt;
            size_t i;
            const auto mul_4_by_2 = Multable::funcs[7];
            for (i = nn; i > loop_bound; --i)
            {
                word_t q_words[2] = {std::numeric_limits<word_t>::max(), std::numeric_limits<word_t>::max()};
                //  get an estimate for current quotient double-word
                //  note: it will never be less than the actual value
                if ((remainder_ptr[2 * i] | ((dword_t)remainder_ptr[2 * i + 1] << bits_in_word)) != divisor)
                    div_two_doublewords_by_one_doubleword(
                        remainder_ptr[2 * i] | ((dword_t)remainder_ptr[2 * i + 1] << bits_in_word),
                        remainder_ptr[2 * i - 2] | ((dword_t)remainder_ptr[2 * i - 1] << bits_in_word), divisor, q_words);
                // first pass of adjusting the estimate
                word_t estimate_checker_words[6] = {0};
                mul_4_by_2(y_checker_words, q_words, estimate_checker_words);
                bool comp = compare_words(estimate_checker_words, remainder_ptr + 2 * i - 4, 6), underflow = q_words[0] < comp;
                q_words[0] -= comp, q_words[1] -= underflow;
                inplace_decrement(estimate_checker_words, y_checker_words, comp ? 4 : 0);
                comp = compare_words(estimate_checker_words, remainder_ptr + 2 * i - 4, 6);
                underflow = q_words[0] < comp;
                q_words[0] -= comp, q_words[1] -= underflow;
                shifted_yabs_size -= 2, shifted_yabs_ptr += 2, shifted_remainder_correction_ptr -= 2;
                clear_words(shifted_remainder_correction_ptr, words_to_clear);
                multiply_by_doubleword(q_words, initial_yabs_ptr, t, shifted_remainder_correction_ptr);
                // second pass of adjusting the estimate
                size_t j;
                for (j = 0; j < words_to_clear && remainder_ptr[remainder_correction_size - 1 - j]
                     == remainder_correction_ptr[remainder_correction_size - 1 - j]; ++j);
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
                if ((remainder_ptr[2] | ((dword_t)remainder_ptr[3] << bits_in_word)) != divisor)
                    div_two_doublewords_by_one_doubleword(remainder_ptr[2] | ((dword_t)remainder_ptr[3] << bits_in_word),
                                                          remainder_ptr[0] | ((dword_t)remainder_ptr[1] << bits_in_word),
                                                          divisor, q_words);
                // first pass of adjusting the estimate
                word_t estimate_checker_words[6] = {0};
                mul_4_by_2(y_checker_words, q_words, estimate_checker_words);
                word_t tmp_words[6] = {0, 0, remainder_ptr[0], remainder_ptr[1], remainder_ptr[2], remainder_ptr[3]};
                bool comp = compare_words(estimate_checker_words, tmp_words, 6), underflow = q_words[0] < comp;
                q_words[0] -= comp, q_words[1] -= underflow;
                inplace_decrement(estimate_checker_words, y_checker_words, comp ? 4 : 0);
                comp = compare_words(estimate_checker_words, tmp_words, 6);
                underflow = q_words[0] < comp;
                q_words[0] -= comp, q_words[1] -= underflow;
                shifted_yabs_size -= 2, shifted_yabs_ptr += 2, shifted_remainder_correction_ptr -= 2;
                clear_words(shifted_remainder_correction_ptr, words_to_clear);
                multiply_by_doubleword(q_words, initial_yabs_ptr, t, shifted_remainder_correction_ptr);
                // second pass of adjusting the estimate
                size_t j;
                for (j = 0; j < words_to_clear && remainder_ptr[remainder_correction_size - 1 - j] ==
                        remainder_correction_ptr[remainder_correction_size - 1 - j]; ++j);
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
                        c = (tmp & ((MPA_SHIFTBASE << backshift_bits) - 1)) << (bits_in_word - backshift_bits);
                    }
                    remainder_head -= (remainder_head && !remainder_ptr[remainder_head]);
                }
                return remainder_head << 2U;
            }
            return find_head(quot_ptr, offset) << 2U;
        }

        inline static thread_local uint16_t primes_memory[sieve_size] = {0};

        static constexpr std::array<uint32_t, sieve_size> primes_sieve = []()
        {
            constexpr uint16_t biggest_prime = 17863, biggest_prime_sqrt = 133;
            std::array<uint32_t, sieve_size> out{0};
            size_t idx = 0;
            out[idx++] = 2;
            for (size_t i = 3; i <= biggest_prime; ++i)
            {
                bool prime = true;
                const size_t loop_bound = i < biggest_prime_sqrt + 1 ? i : biggest_prime_sqrt + 1;
                for (size_t potential_factor = 2; prime && potential_factor < loop_bound; prime = i % potential_factor, ++potential_factor);
                if (prime)
                    out[idx++] = i;
            }
            return out;
        }();

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
            static constexpr word_t number2 = MPA_SHIFTBASE << (bits_in_word - 1);
        };

        static bool validate_input_string(const std::string &input, int base) noexcept
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

        static size_t add(const Integer &l, const Integer &r, word_t *out_words) noexcept
        {
            const size_t l_head = l.get_head(), r_head = r.get_head();
            const size_t bigger_head = l_head > r_head ? l_head : r_head, smaller_head = l_head < r_head ? l_head : r_head;
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
            const size_t l_head = l.get_head(), r_head = r.get_head();
            const size_t bigger_head = l_head > r_head ? l_head : r_head, smaller_head = l_head < r_head ? l_head : r_head;
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
            const size_t lsize = l.get_word_count(), rsize = r.get_word_count();
            (l.words == r.words && lsize == rsize) ? square_karatsuba(l.words, lsize, out_words)
                                                   : multiply_karatsuba(l.words, r.words, lsize, rsize, out_words);
            size_t out_head = find_head(out_words, lsize + rsize - 1);
            const bool out_sign = l.is_negative() != r.is_negative() && out_words[out_head];
            return out_sign | (out_head << 2U); // no ownership
        }

        static size_t call_divmod(const Integer &l, const Integer &r, word_t *out_words, bool need_remainder = false) noexcept
        {
            const size_t l_head = l.get_head(), r_head = r.get_head(), K = l_head + 5;
            if (!need_remainder)
            {
                if (r_head > l_head)
                    return out_words[0] = 0, 0;
                word_t *quot_ptr = out_words, *workspace;
                const bool require_allocation = 3 * K > divmod_buffer_size;
                workspace = !require_allocation ? clear_words(divmod_buffer, 3 * K), divmod_buffer
                                                : allocate_words(3 * K);
                size_t quot_flags = divmod(l.words, l_head, r.words, r_head, quot_ptr, workspace, K, need_remainder);
                const bool quotient_is_zero = !(quot_flags >> 2U) && !(*quot_ptr);
                const bool quotient_is_negative = !(l.is_negative() == r.is_negative() || quotient_is_zero);
                return require_allocation ? free(workspace) : (void)0, quotient_is_negative | quot_flags;
            }
            word_t *remainder_ptr = out_words, *workspace;
            const bool require_allocation = 3 * K > divmod_buffer_size;
            workspace = !require_allocation ? clear_words(divmod_buffer, 3 * K), divmod_buffer
                                            : allocate_words(3 * K);
            const size_t remainder_flags = divmod(l.words, l_head, r.words, r_head, remainder_ptr, workspace, K, need_remainder);
            return require_allocation ? free(workspace) : (void)0, remainder_flags;
        }

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
            words = allocate_words(1), *words = n < 0 ? -n : n;
        }

        Integer(const Integer &other) noexcept : words(nullptr), flags(0)
        {
            const size_t wordcount = other.get_word_count();
            words = allocate_words(wordcount);
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
            return ((index / bits_in_word) <= get_head())
                       ? words[index / bits_in_word] & (MPA_SHIFTBASE << (index & (bits_in_word - 1)))
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
            return get_word_count() * bits_in_word - get_leading_zero_bits(words[get_head()]);
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
            size_t k = bits_in_word - 1 - get_leading_zero_bits(words[get_head()]);
            for (size_t j = k; j < k + 1; ss << get_bit((get_head() * bits_in_word) + j), --j);
            for (size_t i = get_head() - 1; i < get_head(); --i)
                for (size_t j = bits_in_word - 1; j < bits_in_word; ss << get_bit((i * bits_in_word) + j), --j);
            return ss.str();
        }

        std::string to_decimal() const noexcept
        {
            if (is_zero())
                return "0";
            std::vector<uint32_t> digits;
            size_t tmp_head = get_head();
            const size_t tmp_size = tmp_head + 1;
            word_t *tmp = allocate_words(tmp_size);
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
                                                                         DecimalMagic::number2;
                    if constexpr (std::is_same_v<uint16_t, word_t>)
                        tmp[i] = DecimalMagic::number1 * (uint32_t)rhs; // avoid potential ub caused by promotion :)
                    else
                        tmp[i] = DecimalMagic::number1 * rhs;
                }
                digits.push_back(remainder), tmp_head = find_head(tmp, tmp_size - 1);
            }
            const size_t digits_size = digits.size();
            std::string out_str;
            const size_t i = is_negative();
            out_str.resize(digits_size + i);
            out_str[0] = i ? '-' : 0;
            for (size_t j = 0; j < digits_size; out_str[j + i] = digits[digits_size - 1 - j] + '0', ++j);
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
            words = allocate_words(N_wordcount);
            copy_words(words, N.words, N_wordcount);
            return flags = N_sign_bit | 0b10 | ((N_wordcount - 1) << 2U), *this;
        }

        Integer &operator=(Integer &&N) noexcept
        {
            if (words == N.words) // handle self assignment
                return *this;
            if (has_ownership()) // release our buffer if we have ownership
                free(words);
            words = N.words, flags = N.flags;
            return N.flags &= OWNERSHIP_OFF, *this; // clear (ownership) flags for N
        }

        Integer operator<<(const size_t shift) const noexcept
        {
            if (!shift || is_zero())
                return *this;
            const size_t words_shift = shift / bits_in_word, wordcount = get_head() + words_shift + 2;
            word_t *out_words = allocate_words(wordcount);
            const size_t out_head =
                shift_left_by_words_and_bits(words, get_head(), shift & (bits_in_word - 1), words_shift, out_words);
            return Integer(out_words, is_negative() | 0b10 | (out_head << 2U));
        }

        Integer operator>>(const size_t shift) const noexcept
        {
            if (!shift)
                return *this;
            const size_t shift_words = shift / bits_in_word;
            if (get_word_count() <= shift_words)
                return Integer(0);
            const size_t shift_bits = shift & (bits_in_word - 1), wc = get_word_count() - shift_words;
            word_t *out_ptr = allocate_words(wc);
            copy_words(out_ptr, words + shift_words, wc);
            size_t out_head = wc - 1;
            if (shift_bits)
            {
                word_t c = 0;
                for (size_t i = out_head; i <= out_head; --i)
                {
                    const word_t tmp = out_ptr[i];
                    out_ptr[i] = (tmp >> shift_bits) | c;
                    c = (tmp & ((MPA_SHIFTBASE << shift_bits) - 1)) << (bits_in_word - shift_bits);
                }
                out_head -= (out_head && !out_ptr[out_head]);
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
                for (i = 1; carry && i <= get_head(); words[i] += (carry = words[i] == std::numeric_limits<word_t>::max(), 1), ++i);
                if (carry && i == get_word_count())
                {
                    word_t *new_words = allocate_words(i + 1);
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
                for (i = 1; carry && i <= get_head(); words[i] += (carry = words[i] == std::numeric_limits<word_t>::max(), 1), ++i);
                if (carry && i == get_word_count())
                {
                    word_t *new_words = allocate_words(i + 1);
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
            const size_t l_head = get_head(), r_head = r.get_head(), K = l_head + 5;
            if (r_head > l_head)
                return (words[0] = 0, flags = has_ownership() << 1U, *this);
            word_t *workspace;
            const bool require_allocation = 4 * K > divmod_buffer_size;
            workspace = !require_allocation ? clear_words(divmod_buffer, 4 * K), divmod_buffer
                                            : allocate_words(4 * K);
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
            const size_t shift_words = shift / bits_in_word;
            if (get_word_count() <= shift_words)
                return (flags = has_ownership() << 1U, words[0] = 0, *this);
            const size_t wc = get_word_count() - shift_words;
            move_words(words, words + shift_words, wc);
            const size_t shift_bits = shift & (bits_in_word - 1);
            size_t out_head = wc - 1;
            if (shift_bits)
            {
                word_t c = 0;
                for (size_t i = out_head; i <= out_head; --i)
                {
                    const word_t tmp = words[i];
                    words[i] = (tmp >> shift_bits) | c;
                    c = (tmp & ((MPA_SHIFTBASE << shift_bits) - 1)) << (bits_in_word - shift_bits);
                }
                out_head -= (out_head && !words[out_head]);
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
                const size_t characters_per_word = base == 16 ? bits_in_word / 4 : bits_in_word;
                words_size = string_size & (characters_per_word - 1) ? (string_size / characters_per_word) + 1
                                                                     : string_size / characters_per_word;
                words = allocate_words(words_size);
                size_t current_pos = string_size - 1, word_index = words_size - 1;
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
                const size_t needed_words_size = string_size / (bits_in_word / 4) + 2; // upper bound based on hex case
                word_t *local_workspace = allocate_words(3 * needed_words_size);
                word_t *word_ptr = allocate_words(needed_words_size);
                word_t *tmp2_ptr = local_workspace;
                word_t *tmp3_ptr = local_workspace + needed_words_size;
                word_t *base_ptr = local_workspace + 2 * needed_words_size;
                word_t _digit = effective_[string_size - 1] - '0';
                *base_ptr = 1, *word_ptr = _digit;
                Integer future_this(word_ptr, 0), base(base_ptr, 0);
                for (size_t i = 1; i < string_size; ++i)
                {
                    clear_words(tmp2_ptr, needed_words_size);
                    multiply_by_word(10, base_ptr, base.get_word_count(), tmp2_ptr);
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
        {
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
        }

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
            word_t *outwords = allocate_words(bigger.get_word_count());
            size_t outhead = 0;
            for (size_t i = 0; i < smaller.get_word_count(); outhead = (outwords[i] = l.words[i] ^ r.words[i]) ? i : outhead, ++i);
            for (size_t i = smaller.get_word_count(); i < bigger.get_word_count(); outhead = (outwords[i] = bigger.words[i]) ? i : outhead, ++i);
            return Integer(outwords, 0b10 | (outhead << 2U));
        }

        friend Integer operator|(const Integer &l, const Integer &r) noexcept
        {
            const Integer &bigger = l.get_word_count() > r.get_word_count() ? l : r;
            const Integer &smaller = l.get_word_count() > r.get_word_count() ? r : l;
            word_t *outwords = allocate_words(bigger.get_word_count());
            size_t outhead = 0;
            for (size_t i = 0; i < smaller.get_word_count(); outhead = (outwords[i] = l.words[i] | r.words[i]) ? i : outhead, ++i);
            for (size_t i = smaller.get_word_count(); i < bigger.get_word_count(); outhead = (outwords[i] = bigger.words[i]) ? i : outhead, ++i);
            return Integer(outwords, 0b10 | (outhead << 2U));
        }

        friend Integer operator&(const Integer &l, const Integer &r) noexcept
        {
            const size_t min_size = l.get_word_count() > r.get_word_count() ? r.get_word_count() : l.get_word_count();
            word_t *outwords = allocate_words(min_size);
            size_t outhead = 0;
            for (size_t i = 0; i < min_size; outhead = (outwords[i] = l.words[i] & r.words[i]) ? i : outhead, ++i);
            return Integer(outwords, 0b10 | (outhead << 2U));
        }

        friend Integer operator+(const Integer &l, const Integer &r) noexcept
        {
            const size_t l_head = l.get_head();
            const size_t r_head = r.get_head();
            word_t *out_words = allocate_words(l_head > r_head ? l_head + 2 : r_head + 2);
            return Integer(out_words, 0b10 | add(l, r, out_words));
        }

        friend Integer operator-(const Integer &l, const Integer &r) noexcept
        {
            const size_t l_head = l.get_head();
            const size_t r_head = r.get_head();
            word_t *out_words = allocate_words(l_head > r_head ? l_head + 2 : r_head + 2);
            return Integer(out_words, 0b10 | subtract(l, r, out_words));
        }

        friend Integer operator*(const Integer &l, const Integer &r) noexcept
        {
            word_t *out_words = allocate_words(l.get_word_count() + r.get_word_count());
            return Integer(out_words, 0b10 | multiply(l, r, out_words));
        }

        friend Integer operator/(const Integer &l, const Integer &r) noexcept
        {
            word_t *out_words = allocate_words(l.get_head() + 5);
            return Integer(out_words, 0b10 | call_divmod(l, r, out_words));
        }

        friend Integer operator%(const Integer &l, const Integer &r) noexcept
        {
            word_t *out_words = allocate_words(l.get_head() + 5);
            if (l.is_negative())
                return r.is_negative() ? -r - Integer(out_words, 0b10 | call_divmod(l, r, out_words, true))
                                       : r - Integer(out_words, 0b10 | call_divmod(l, r, out_words, true));
            return Integer(out_words, 0b10 | call_divmod(l, r, out_words, true));
        }

        static Integer get_random(const size_t wordcount, const bool is_negative, word_t *buffer = nullptr) noexcept
        {
            static thread_local std::random_device dev;
            static thread_local std::mt19937 rng(dev());
            static thread_local std::uniform_int_distribution<std::mt19937::result_type> dist(0, std::numeric_limits<uint8_t>::max());
            word_t *out_words = buffer ? buffer : allocate_words(wordcount);
            for (size_t i = 0; i < wordcount - 1; ++i)
            {
                out_words[i] = 0;
                for (size_t j = 0; j < sizeof(word_t); out_words[i] |= dist(rng) << (8 * j), ++j);
            }
            word_t head = 0;
            for (size_t j = 0; j < sizeof(word_t); head |= dist(rng) << (8 * j), ++j);
            out_words[wordcount - 1] = head ? head : std::numeric_limits<word_t>::max();
            return Integer(out_words, is_negative | (buffer ? 0 : 0b10) | ((wordcount - 1) << 2U));
        }

        static Integer get_random(const Integer &limit, word_t *buffer = nullptr) noexcept
        {
            static thread_local std::random_device dev;
            static thread_local std::mt19937 rng(dev());
            static thread_local std::uniform_int_distribution<std::mt19937::result_type> dist(0, std::numeric_limits<uint8_t>::max());
            const Integer unsigned_limit(limit.words, limit.flags & SIGN_OFF_OWNERSHIP_OFF);
            const size_t limit_bit_count = limit.get_bit_count();
            size_t limit_byte_count = limit_bit_count / 8;
            const size_t bits_left_over = limit_bit_count - limit_byte_count * 8;
            word_t *out_words = buffer ? buffer : allocate_words(limit.get_word_count());
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
                for (; !out_words[out_head]; out_words[out_head] = dist(rng));
                Integer out(out_words, (buffer ? 0 : 0b10) | (out_head << 2U));
                if (out < unsigned_limit)
                    return out;
            }
        }

        word_t *words;
        size_t flags;

        template <typename T>
        friend Integer<T> power(const Integer<T> &base, size_t exponent) noexcept;

        template <typename T>
        friend void egcd(const Integer<T> &x, const Integer<T> &y, Integer<T> *r, Integer<T> *s,
                         Integer<T> *t, T *workspace, ExtendedGcdFlags *flags_ptr) noexcept;

        template <typename T>
        friend Integer<T> gcd(const Integer<T> &l, const Integer<T> &r) noexcept;

        template <typename T>
        friend Integer<T> lcm(const Integer<T> &l, const Integer<T> &r) noexcept;

        template <typename T>
        friend Integer<T> modular_power(const Integer<T> &base, const Integer<T> &exponent,
                                        const Integer<T> &modulus) noexcept;

        template <typename T>
        friend Integer<T> modular_inverse(const Integer<T> &N, const Integer<T> &modulus) noexcept;

        template <typename T>
        friend bool is_probably_prime(const Integer<T> candidate, const size_t steps, T *workspace) noexcept;

        template <typename T>
        friend Integer<T> get_random_prime(const size_t wordcount, bool verbose) noexcept;
    };

    #define MUL_OP(l, r)                                                                        \
        const size_t l_size = l.get_word_count();                                               \
        const size_t r_size = r.get_word_count();                                               \
        Integer<word_t>::copy_words(stash_ptr, l.words, l_size);                                \
        Integer<word_t>::clear_words(l.words, l_size + r_size);                                 \
        Integer<word_t>::multiply_karatsuba(stash_ptr, r.words, l_size, r_size, l.words);       \
        l.flags = Integer<word_t>::find_head(l.words, l_size + r_size - 1) << 2U

    #define SQUARE_OP(l)                                                                        \
        const size_t l_size = l.get_word_count();                                               \
        Integer<word_t>::copy_words(stash_ptr, l.words, l_size);                                \
        Integer<word_t>::clear_words(l.words, 2 * l_size);                                      \
        Integer<word_t>::square_karatsuba(stash_ptr, l_size, l.words);                          \
        l.flags = Integer<word_t>::find_head(l.words, 2 * l_size - 1) << 2U

    template <typename word_t>
    Integer<word_t> power(const Integer<word_t> &base, size_t exponent) noexcept
    {
        if (!exponent)
            return Integer<word_t>(1);
        const size_t prodsize = base.get_word_count() * exponent;
        word_t *local_workspace = Integer<word_t>::allocate_words(prodsize * 2);
        word_t *p_ptr = Integer<word_t>::allocate_words(prodsize), *q_ptr = local_workspace;
        word_t *stash_ptr = local_workspace + prodsize;
        Integer<word_t>::copy_words(p_ptr, base.words, base.get_word_count());
        Integer<word_t>::copy_words(q_ptr, base.words, base.get_word_count());
        Integer<word_t> p(p_ptr, base.flags), q(q_ptr, base.flags & Integer<word_t>::OWNERSHIP_OFF);
        size_t j = Integer<word_t>::get_trailing_zero_bits(exponent);
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
              Integer<word_t> *t = nullptr, word_t *workspace = nullptr, ExtendedGcdFlags *flags_ptr = nullptr) noexcept
    {
        const size_t max_size = 1 + (x.get_head() > y.get_head() ? x.get_head() : y.get_head());
        word_t *local_workspace = !workspace ? Integer<word_t>::allocate_words(max_size * 2 * 8 + max_size + 4) : workspace;

        const size_t offset = 2 * max_size;
        word_t *r0_ptr = local_workspace, *r1_ptr = r0_ptr + offset;
        Integer<word_t>::copy_words(r0_ptr, x.words, x.get_word_count());
        Integer<word_t>::copy_words(r1_ptr, y.words, y.get_word_count());
        Integer<word_t> r0(r0_ptr, x.get_head() << 2U), r1(r1_ptr, y.get_head() << 2U);
        word_t *s0_ptr = r1_ptr + offset, *s1_ptr = s0_ptr + offset;
        *s0_ptr = 1;
        Integer<word_t> s0(s0_ptr, 0), s1(s1_ptr, 0);
        word_t *t0_ptr = s1_ptr + offset, *t1_ptr = t0_ptr + offset;
        *t1_ptr = 1;
        Integer<word_t> t0(t0_ptr, 0), t1(t1_ptr, 0);
        word_t *tmp_ptr = t1_ptr + offset, *tmp_prod_ptr = tmp_ptr + offset;
        Integer<word_t> tmp(tmp_ptr, 0), tmp_prod(tmp_prod_ptr, 0);
        word_t *quot_ptr = tmp_prod_ptr + offset;
        Integer<word_t> q(quot_ptr, 0);

        const auto swap_integers = [](Integer<word_t> &l, Integer<word_t> &r)
        {
            word_t *stash_ptr = l.words;
            const size_t stash_flags = l.flags;
            l.words = r.words, l.flags = r.flags;
            r.words = stash_ptr, r.flags = stash_flags;
        };

        const auto euklid_step = [&swap_integers, &tmp, &tmp_prod, &q](Integer<word_t> &x0, Integer<word_t> &x1)
        {
            Integer<word_t>::copy_words(tmp.words, x0.words, x0.get_word_count());
            tmp.flags = x0.flags;
            const size_t tmp_head = tmp.get_head();
            const bool tmp_is_negative = tmp.is_negative();
            Integer<word_t>::clear_words(tmp_prod.words, q.get_word_count() + x1.get_word_count());
            Integer<word_t>::multiply_karatsuba(q.words, x1.words, q.get_word_count(), x1.get_word_count(), tmp_prod.words);
            const bool tmp_prod_is_negative = q.is_negative() != x1.is_negative();
            const size_t tmp_prod_head = Integer<word_t>::find_head(tmp_prod.words, q.get_head() + x1.get_word_count());
            tmp_prod.flags = tmp_prod_is_negative | (tmp_prod_head << 2U);
            swap_integers(x0, x1);
            if (tmp_is_negative == tmp_prod_is_negative) // x1 = |tmp| - |tmp_prod| or x1 = -|tmp| + |tmp_prod|
            {
                const word_t *ptr_selector[2] = {tmp.words, tmp_prod.words};
                const size_t head_selector[2] = {tmp_head, tmp_prod_head};
                const bool l_geq_r = Integer<word_t>::l_abs_geq_r_abs(ptr_selector[tmp_is_negative], ptr_selector[!tmp_is_negative],
                                                                      head_selector[tmp_is_negative], head_selector[!tmp_is_negative]);
                size_t x1_head;
                Integer<word_t>::subtract_words(ptr_selector[tmp_is_negative ? l_geq_r : !l_geq_r],
                                                ptr_selector[tmp_is_negative ? !l_geq_r : l_geq_r],
                                                head_selector[tmp_is_negative ? l_geq_r : !l_geq_r],
                                                head_selector[tmp_is_negative ? !l_geq_r : l_geq_r], x1.words, x1_head);
                x1.flags = (!l_geq_r) | (x1_head << 2U);
            }
            else // x1 = -|tmp| - |tmp_prod| or x1 = |tmp| + |tmp_prod|
            {
                const bool check = tmp_head >= tmp_prod_head;
                const size_t bigger_head = check ? tmp_head : tmp_prod_head;
                Integer<word_t>::add_words(check ? tmp.words : tmp_prod.words, check ? tmp_prod.words : tmp.words, bigger_head + 1,
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
            const size_t r0_head = r0.get_head(), r1_head = r1.get_head(), K = r0_head + 5;
            if (r1_head > r0_head || r1 > r0)
                q.flags = (*quot_ptr = 0, 0);
            else
            {
                Integer<word_t>::clear_words(quot_ptr, K);
                word_t *divmod_workspace;
                const bool require_allocation = 3 * K > Integer<word_t>::divmod_buffer_size;
                divmod_workspace = !require_allocation ? Integer<word_t>::clear_words(Integer<word_t>::divmod_buffer, 3 * K), Integer<word_t>::divmod_buffer
                                                       : Integer<word_t>::allocate_words(3 * K);
                const size_t quot_flags = Integer<word_t>::divmod(r0.words, r0_head, r1.words, r1_head, quot_ptr, divmod_workspace, K);
                q.flags = (r0.is_negative() != r1.is_negative()) | (quot_flags & Integer<word_t>::SIGN_OFF_OWNERSHIP_OFF);
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
        return egcd(l, r, &out), out.flags &= Integer<word_t>::SIGN_OFF, out; // ensure the result is non-negative
    }

    template <typename word_t>
    Integer<word_t> lcm(const Integer<word_t> &l, const Integer<word_t> &r) noexcept
    {
        Integer<word_t> tmp, out;
        return egcd(l, r, &tmp), out = (l * r) / tmp, out.flags &= Integer<word_t>::SIGN_OFF, out; // ensure the result is non-negative
    }

    #define BARRETT_OP(x, modulus)                                                                                                 \
        if (x.get_head() >= k - 1)                                                                                                 \
        {                                                                                                                          \
            Integer barrett(barrett_ptr, 0);                                                                                       \
            Integer<word_t>::clear_words(stash_ptr, x.get_head() + 2 - k + mue_size);                                              \
            Integer<word_t>::multiply_karatsuba(x.words + k - 1, mue.words, x.get_head() + 2 - k, mue_size, stash_ptr);            \
            barrett.flags = Integer<word_t>::find_head(stash_ptr, x.get_head() + 1 - k + mue_size) << 2U;                          \
            if (barrett.get_head() >= k + 1)                                                                                       \
            {                                                                                                                      \
                barrett.flags = (barrett.get_head() - k - 1) << 2U;                                                                \
                Integer<word_t>::clear_words(barrett_ptr, barrett.get_word_count() + k);                                           \
                Integer<word_t>::multiply_karatsuba(stash_ptr + k + 1, modulus.words, barrett.get_word_count(), k, barrett.words); \
                barrett.flags = Integer<word_t>::find_head(barrett.words, barrett.get_head() + k) << 2U;                           \
                Integer<word_t>::inplace_decrement(x.words, barrett.words, barrett.get_word_count());                              \
                x.flags = (Integer<word_t>::find_head(x.words, x.get_head()) << 2U);                                               \
            }                                                                                                                      \
        }                                                                                                                          \
        Integer<word_t>::inplace_decrement(x.words, modulus.words, x >= modulus ? k : 0);                                          \
        x.flags = Integer<word_t>::find_head(x.words, x.get_head()) << 2U;                                                         \
        Integer<word_t>::inplace_decrement(x.words, modulus.words, x >= modulus ? k : 0);                                          \
        x.flags = Integer<word_t>::find_head(x.words, x.get_head()) << 2U

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

        const size_t base_size = base.get_word_count(), modulus_size = modulus.get_word_count();
        const size_t prodsize = base_size > modulus_size ? base_size * 2 + 4 : modulus_size * 2 + 4;
        const size_t barrett_size = prodsize * 2, exponent_size = exponent.get_word_count();
        const size_t egcd_workspace_size = exponent.is_negative() ? 4 + 2 * 8 * (prodsize - 4) / 2 + (prodsize - 4) / 2 + 4
                                                                  : 0;
        const size_t local_workspace_size =
            prodsize * 2 + exponent_size + egcd_workspace_size + barrett_size + modulus_size * 2 + 1;
        word_t *p_ptr = Integer<word_t>::allocate_words(prodsize);
        word_t *local_workspace = Integer<word_t>::allocate_words(local_workspace_size);
        word_t *q_ptr = local_workspace;
        word_t *stash_ptr = q_ptr + prodsize;
        word_t *d_ptr = stash_ptr + prodsize;
        word_t *egcd_workspace = d_ptr + exponent_size;
        word_t *barrett_ptr = egcd_workspace + egcd_workspace_size;
        word_t *mue_ptr = barrett_ptr + barrett_size;
        Integer<word_t>::copy_words(d_ptr, exponent.words, exponent_size);
        mue_ptr[modulus_size * 2] = 1;
        *p_ptr = 1;

        Integer<word_t> p(p_ptr, 0), q(q_ptr, 0), d(d_ptr, exponent.flags & Integer<word_t>::SIGN_OFF_OWNERSHIP_OFF);
        Integer<word_t> mue(mue_ptr, (modulus_size * 2) << 2U);
        mue /= modulus;
        const size_t mue_size = mue.get_word_count(), k = modulus.get_word_count();

        const auto mod_op = [&prodsize, &modulus, &stash_ptr](Integer<word_t> &s)
        {
            const size_t l_head = s.get_head(), r_head = modulus.get_head(), K = l_head + 5;
            word_t *remainder_ptr = s.words;
            const size_t remainder_size = prodsize;
            Integer<word_t>::copy_words(stash_ptr, s.words, s.get_word_count());
            const bool s_is_negative = s.is_negative();

            if (r_head > l_head)
            {
                if (s_is_negative)
                {
                    Integer<word_t>::clear_words(remainder_ptr, remainder_size);
                    Integer<word_t>::copy_words(remainder_ptr, modulus.words, r_head + 1);
                    Integer<word_t>::inplace_decrement(remainder_ptr, stash_ptr, s.get_word_count());
                    s.flags = Integer<word_t>::find_head(remainder_ptr, r_head) << 2U;
                }
                return;
            }
            word_t *divmod_workspace;
            const bool require_allocation = 3 * K > Integer<word_t>::divmod_buffer_size;
            divmod_workspace = !require_allocation ? Integer<word_t>::clear_words(Integer<word_t>::divmod_buffer, 3 * K), Integer<word_t>::divmod_buffer
                                                   : Integer<word_t>::allocate_words(3 * K);
            Integer<word_t>::clear_words(remainder_ptr, remainder_size);
            const size_t remainder_flags =
                Integer<word_t>::divmod(stash_ptr, l_head, modulus.words, r_head, remainder_ptr, divmod_workspace, K, true);
            if (s_is_negative)
            { // reuse workspace
                Integer<word_t>::clear_words(divmod_workspace, r_head + 1);
                Integer<word_t>::copy_words(divmod_workspace, modulus.words, r_head + 1);
                Integer<word_t>::inplace_decrement(divmod_workspace, remainder_ptr, 1 + (remainder_flags >> 2U));
                Integer<word_t>::clear_words(remainder_ptr, remainder_size);
                Integer<word_t>::copy_words(remainder_ptr, divmod_workspace, r_head + 1);
                s.flags = Integer<word_t>::find_head(remainder_ptr, r_head) << 2U;
            }
            else
                s.flags = remainder_flags & Integer<word_t>::SIGN_OFF_OWNERSHIP_OFF;
            require_allocation ? free(divmod_workspace) : (void)0;
        };

        if (exponent.is_negative())
        {
            Integer<word_t>::clear_words(egcd_workspace, egcd_workspace_size);
            ExtendedGcdFlags flags;
            egcd<word_t>(base, modulus, nullptr, nullptr, nullptr, egcd_workspace, &flags);
            const size_t offset = 2 * (base_size > modulus_size ? base_size : modulus_size);
            word_t *r_ptr = flags.location_encoding & 0b001 ? egcd_workspace + offset : egcd_workspace;
            word_t *s_ptr =
                flags.location_encoding & 0b010 ? egcd_workspace + 3 * offset : egcd_workspace + 2 * offset;

            Integer<word_t> r(r_ptr, flags.r0_flags & Integer<word_t>::OWNERSHIP_OFF); // explicitly disable ownership to make compiler happy
            Integer<word_t> s(s_ptr, flags.s0_flags & Integer<word_t>::OWNERSHIP_OFF); // explicitly disable ownership to make compiler happy
            if (!(r.get_head() == 0 && r.words[0] == 1))
                return (free(local_workspace), p.flags = 0b10, *p.words = 0, p);
            const size_t s_size = s.get_word_count();
            if (s_size >= modulus.get_word_count())
                mod_op(s);

            Integer<word_t>::copy_words(q.words, s.words, s_size);
            q.flags = s.get_head() << 2U;
        }
        else
        {
            Integer<word_t>::copy_words(q.words, base.words, base_size);
            q.flags = base.flags & Integer<word_t>::OWNERSHIP_OFF;
            if (q.get_word_count() >= modulus.get_word_count())
                mod_op(q);
        }

        // precomputation
        const int32_t window_size = 6;
        const size_t precomp_size = MPA_SHIFTBASE << (window_size - 1);
        const bool lookup_requires_alloc = precomp_size * prodsize > Integer<word_t>::power_buffer_size;
        word_t *lookup_table = !lookup_requires_alloc ? Integer<word_t>::power_buffer : Integer<word_t>::allocate_words(precomp_size * prodsize);
        Integer<word_t>::clear_words(lookup_table, precomp_size * prodsize);
        Integer<word_t>::copy_words(lookup_table, q.words, q.get_word_count());
        memcpy(lookup_table + modulus_size, &q.flags, sizeof(size_t));
        BARRETT_SQUARE(q, modulus);
        const size_t effective_base_squared_size = q.get_word_count();
        for (size_t j = 1; j < precomp_size; ++j)
        {
            word_t *src = lookup_table + (j - 1) * prodsize, *target = lookup_table + j * prodsize;
            size_t src_size = (memcpy(&src_size, src + modulus_size, sizeof(size_t)), (src_size >> 2U) + 1);
            Integer<word_t>::multiply_karatsuba(src, q.words, src_size, effective_base_squared_size, target);
            Integer<word_t> tmp(target, Integer<word_t>::find_head(target, src_size + effective_base_squared_size - 1) << 2U);
            BARRETT_OP(tmp, modulus);
            memcpy(target + modulus_size, &tmp.flags, sizeof(size_t));
        }
        // main loop
        int64_t i = exponent.get_bit_count() - 1;
        while (i >= 0)
        {
            if (!(exponent.words[i / Integer<word_t>::bits_in_word] & (MPA_SHIFTBASE << (i & (Integer<word_t>::bits_in_word - 1))))) // (!exponent.get_bit(i))
            {
                BARRETT_SQUARE(p, modulus);
                i -= 1;
            }
            else
            {
                size_t window = 0, window_width = 0, right_most_possible = window_size < i + 1 ? 0 : window_size - i - 1;
                int64_t l = 0;
                bool found_l = false;
                for (int64_t j = right_most_possible; j < window_size; ++j)
                {
                    size_t index = i - window_size + 1 + j;
                    word_t component = (exponent.words[index / Integer<word_t>::bits_in_word] &
                                        (MPA_SHIFTBASE << (index & (Integer<word_t>::bits_in_word - 1)))) > 0; // exponent.get_bit(index)
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

        const size_t wordcount = candidate.get_word_count(), max_prodsize = wordcount * 2 + 4;
        const size_t workspace_buffer_size = max_prodsize * 6 + wordcount * 3 + 1;
        word_t *workspace_buffer = workspace ? workspace : Integer<word_t>::allocate_words(workspace_buffer_size);
        word_t *p_ptr = workspace_buffer;
        word_t *q_ptr = workspace_buffer + max_prodsize;
        word_t *stash_ptr = workspace_buffer + max_prodsize * 2;
        word_t *c_ptr = workspace_buffer + max_prodsize * 3;
        word_t *barrett_ptr = workspace_buffer + max_prodsize * 4;
        word_t *candidate_buffer = workspace_buffer + max_prodsize * 6;
        word_t *mue_ptr = workspace_buffer + max_prodsize * 6 + wordcount;

        Integer<word_t>::clear_words(mue_ptr, 2 * wordcount);
        mue_ptr[2 * wordcount] = 1;
        Integer<word_t> mue(mue_ptr, (2 * wordcount) << 2U);
        mue /= candidate;
        const size_t mue_size = mue.get_word_count();
        const size_t &k = wordcount;
        Integer<word_t>::copy_words(c_ptr, candidate.words, wordcount);
        Integer<word_t> c(c_ptr, candidate.flags & Integer<word_t>::OWNERSHIP_OFF);
        c -= 1;
        const int64_t base_j = Integer<word_t>::get_trailing_zero_bits(c.words, c.get_head());
        const Integer<word_t> &modulus = candidate;
        const size_t modulus_size = modulus.get_word_count();
        const Integer<word_t> limit = candidate - 2;

        // prepare lookup tables for sliding window exponentiation
        const int32_t window_size = 6;
        const size_t precomp_size = MPA_SHIFTBASE << (window_size - 1);
        const bool lookup_requires_alloc = precomp_size * max_prodsize > Integer<word_t>::power_buffer_size;
        word_t *lookup_table = !lookup_requires_alloc ? Integer<word_t>::power_buffer : Integer<word_t>::allocate_words(precomp_size * max_prodsize);
        const size_t exponent_bitcount = c.get_bit_count();
        std::vector<size_t> exponent_windows;
        exponent_windows.reserve(exponent_bitcount);
        int64_t pre_bit_pos = exponent_bitcount - 1;
        while (pre_bit_pos >= base_j)
        {
            if (!(c.words[pre_bit_pos / Integer<word_t>::bits_in_word] & (MPA_SHIFTBASE << (pre_bit_pos & (Integer<word_t>::bits_in_word - 1)))))
                pre_bit_pos -= 1;
            else
            {
                size_t window = 0, window_width = 0, right_most_possible = window_size < pre_bit_pos + 1 ? 0 : window_size - pre_bit_pos - 1;
                int64_t l = 0;
                bool found_l = false;
                for (int64_t j = right_most_possible; j < window_size; ++j)
                {
                    size_t index = pre_bit_pos - window_size + 1 + j;
                    word_t component = (c.words[index / Integer<word_t>::bits_in_word] & (MPA_SHIFTBASE << (index & (Integer<word_t>::bits_in_word - 1)))) > 0;
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
            Integer<word_t> a = Integer<word_t>::get_random(limit, candidate_buffer);
            *p_ptr = 1;
            *(p_ptr + 1) = 0; // just making sure
            Integer<word_t>::copy_words(q_ptr, a.words, a.get_word_count());
            Integer<word_t> p(p_ptr, 0);
            Integer<word_t> q(q_ptr, a.flags & Integer<word_t>::OWNERSHIP_OFF);

            // precomputation for sliding window
            Integer<word_t>::clear_words(lookup_table, precomp_size * max_prodsize);
            Integer<word_t>::copy_words(lookup_table, q.words, q.get_word_count());
            memcpy(lookup_table + modulus_size, &q.flags, sizeof(size_t));
            BARRETT_SQUARE(q, modulus);
            const size_t base_squared_size = q.get_word_count();
            for (size_t j = 1; j < precomp_size; ++j)
            {
                word_t *src = lookup_table + (j - 1) * max_prodsize, *target = lookup_table + j * max_prodsize;
                size_t src_size = (memcpy(&src_size, src + modulus_size, sizeof(size_t)), (src_size >> 2U) + 1);
                Integer<word_t>::multiply_karatsuba(src, q.words, src_size, base_squared_size, target);
                Integer<word_t> tmp(target, Integer<word_t>::find_head(target, src_size + base_squared_size - 1) << 2U);
                BARRETT_OP(tmp, modulus);
                memcpy(target + modulus_size, &tmp.flags, sizeof(size_t));
            }

            // main loop for sliding window: computes p = a^d, where d = (c >> base_j)
            int64_t bit_pos = exponent_bitcount - 1;
            size_t window_count = 0;
            while (bit_pos >= base_j)
            {
                if (!(c.words[bit_pos / Integer<word_t>::bits_in_word] & (MPA_SHIFTBASE << (bit_pos & (Integer<word_t>::bits_in_word - 1)))))
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
                    bit_pos = l - 1, window_count += 1;
                }
            }
            bool miller_rabin_step_passed = ((!p.get_head() && p.words[0] == 1) || p == c);

            // check a^(2^r*d) for 0 <= r < base_j
            size_t j = base_j;
            while (!miller_rabin_step_passed && j > 1 && (p.get_head() || p.words[0] > 1))
            {
                BARRETT_SQUARE(p, modulus);
                miller_rabin_step_passed |= p == c, j -= 1;
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
        word_t *workspace = Integer<word_t>::allocate_words(workspace_buffer_size);
        word_t *buffer = Integer<word_t>::allocate_words(wordcount);
        Integer<word_t> p(Integer<word_t>::get_random(wordcount, false, buffer));
        const auto refresh_memory = [&p]()
        {
            for (size_t j = 0; j < Integer<word_t>::sieve_size; ++j)
            {
                const word_t modulus = Integer<word_t>::primes_sieve[j];
                word_t output = p.words[0] % modulus;
                const word_t base_factor = ((std::numeric_limits<word_t>::max() % modulus) + 1) % modulus;
                word_t current_base_modulus = base_factor;
                size_t i = 1;
                for (i = 1; i < p.get_head(); ++i)
                {
                    output = (output + (p.words[i] % modulus) * current_base_modulus) % modulus;
                    current_base_modulus = (current_base_modulus * base_factor) % modulus;
                }
                Integer<word_t>::primes_memory[j] = (output + (p.words[i] % modulus) * current_base_modulus) % modulus;
            }
        };
        const auto prepare_p = [&p]()
        {
            p.words[0] |= 1;                                         // make sure p is odd
            p.words[p.get_word_count() - 1] |= Integer<word_t>::msb; // make sure p has msb set
            word_t p_mod_3 = p.words[0] % 3;
            for (size_t j = 1; j < p.get_word_count(); p_mod_3 = (p_mod_3 + (p.words[j] % 3)) % 3, ++j); // we use 2^64k = (-1)^64k = 1 (mod 3)
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
        int64_t step = 0, j = 0;
        word_t memory_step = 0;
        while (true)
        {
            bool composite = false;
            for (size_t i = 0; !composite && i < Integer<word_t>::sieve_size; ++i)
                composite = !((Integer<word_t>::primes_memory[i] + memory_step) % Integer<word_t>::primes_sieve[i]);
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
