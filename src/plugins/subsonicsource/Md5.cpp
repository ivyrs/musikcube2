//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "Md5.h"
#include <cstdint>

namespace musik { namespace core { namespace subsonic {

    using u32 = uint32_t;
    using u64 = uint64_t;

    static inline u32 leftRotate(u32 x, u32 c) {
        return (x << c) | (x >> (32 - c));
    }

    std::string Md5Hex(const std::string& input) {
        static const u32 s[64] = {
            7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
            5, 9,14,20, 5, 9,14,20, 5, 9,14,20, 5, 9,14,20,
            4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
            6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
        };

        static const u32 K[64] = {
            0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,
            0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
            0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
            0x6b901122,0xfd987193,0xa679438e,0x49b40821,
            0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,
            0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
            0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,
            0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
            0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
            0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
            0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,
            0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
            0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,
            0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
            0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
            0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
        };

        u32 a0 = 0x67452301;
        u32 b0 = 0xefcdab89;
        u32 c0 = 0x98badcfe;
        u32 d0 = 0x10325476;

        std::string msg(input);
        const u64 originalLenBits = static_cast<u64>(msg.size()) * 8;

        msg.push_back(static_cast<char>(0x80));
        while ((msg.size() % 64) != 56) {
            msg.push_back(static_cast<char>(0x00));
        }

        for (int i = 0; i < 8; i++) {
            msg.push_back(static_cast<char>((originalLenBits >> (8 * i)) & 0xff));
        }

        for (size_t chunkStart = 0; chunkStart < msg.size(); chunkStart += 64) {
            u32 M[16];
            for (int i = 0; i < 16; i++) {
                M[i] =
                    (static_cast<u32>(static_cast<unsigned char>(msg[chunkStart + i * 4 + 0]))) |
                    (static_cast<u32>(static_cast<unsigned char>(msg[chunkStart + i * 4 + 1])) << 8) |
                    (static_cast<u32>(static_cast<unsigned char>(msg[chunkStart + i * 4 + 2])) << 16) |
                    (static_cast<u32>(static_cast<unsigned char>(msg[chunkStart + i * 4 + 3])) << 24);
            }

            u32 A = a0, B = b0, C = c0, D = d0;

            for (u32 i = 0; i < 64; i++) {
                u32 F;
                u32 g;

                if (i < 16) {
                    F = (B & C) | (~B & D);
                    g = i;
                }
                else if (i < 32) {
                    F = (D & B) | (~D & C);
                    g = (5 * i + 1) % 16;
                }
                else if (i < 48) {
                    F = B ^ C ^ D;
                    g = (3 * i + 5) % 16;
                }
                else {
                    F = C ^ (B | ~D);
                    g = (7 * i) % 16;
                }

                F = F + A + K[i] + M[g];
                A = D;
                D = C;
                C = B;
                B = B + leftRotate(F, s[i]);
            }

            a0 += A;
            b0 += B;
            c0 += C;
            d0 += D;
        }

        unsigned char digest[16];
        const u32 words[4] = { a0, b0, c0, d0 };
        for (int i = 0; i < 4; i++) {
            digest[i * 4 + 0] = static_cast<unsigned char>((words[i] >> 0) & 0xff);
            digest[i * 4 + 1] = static_cast<unsigned char>((words[i] >> 8) & 0xff);
            digest[i * 4 + 2] = static_cast<unsigned char>((words[i] >> 16) & 0xff);
            digest[i * 4 + 3] = static_cast<unsigned char>((words[i] >> 24) & 0xff);
        }

        static const char* hex = "0123456789abcdef";
        std::string result;
        result.reserve(32);
        for (int i = 0; i < 16; i++) {
            result.push_back(hex[(digest[i] >> 4) & 0xf]);
            result.push_back(hex[digest[i] & 0xf]);
        }
        return result;
    }

} } }
