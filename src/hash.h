// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_HASH_H
#define BITCOIN_HASH_H

#include "uint256.h"
#include "serialize.h"

#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <vector>

#include "crypto/sph_blake.h"
#include "crypto/sph_bmw.h"
#include "crypto/sph_groestl.h"
#include "crypto/sph_jh.h"
#include "crypto/sph_keccak.h"
#include "crypto/sph_skein.h"
#include "crypto/sph_luffa.h"
#include "crypto/sph_cubehash.h"
#include "crypto/sph_shavite.h"
#include "crypto/sph_simd.h"
#include "crypto/sph_echo.h"

//template<typename T1>
//inline uint256 Hash(const T1 pbegin, const T1 pend)
//{
//    static unsigned char pblank[1];
//    uint256 hash1;
//    SHA256((pbegin == pend ? pblank : (unsigned char*)&pbegin[0]), (pend - pbegin) * sizeof(pbegin[0]), (unsigned char*)&hash1);
//    uint256 hash2;
//    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
//    return hash2;
//}

//class CHashWriter
//{
//private:
//    SHA256_CTX ctx;

//public:
//    int nType;
//    int nVersion;

//    void Init() {
//        SHA256_Init(&ctx);
//    }

//    CHashWriter(int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn) {
//        Init();
//    }

//    CHashWriter& write(const char *pch, size_t size) {
//        SHA256_Update(&ctx, pch, size);
//        return (*this);
//    }

//    // invalidates the object
//    uint256 GetHash() {
//        uint256 hash1;
//        SHA256_Final((unsigned char*)&hash1, &ctx);
//        uint256 hash2;
//        SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
//        return hash2;
//    }

//    template<typename T>
//    CHashWriter& operator<<(const T& obj) {
//        // Serialize to this stream
//        ::Serialize(*this, obj, nType, nVersion);
//        return (*this);
//    }
//};


//template<typename T1, typename T2>
//inline uint256 Hash(const T1 p1begin, const T1 p1end,
//                    const T2 p2begin, const T2 p2end)
//{
//    static unsigned char pblank[1];
//    uint256 hash1;
//    SHA256_CTX ctx;
//    SHA256_Init(&ctx);
//    SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
//    SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
//    SHA256_Final((unsigned char*)&hash1, &ctx);
//    uint256 hash2;
//    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
//    return hash2;
//}

//template<typename T1, typename T2, typename T3>
//inline uint256 Hash(const T1 p1begin, const T1 p1end,
//                    const T2 p2begin, const T2 p2end,
//                    const T3 p3begin, const T3 p3end)
//{
//    static unsigned char pblank[1];
//    uint256 hash1;
//    SHA256_CTX ctx;
//    SHA256_Init(&ctx);
//    SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
//    SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
//    SHA256_Update(&ctx, (p3begin == p3end ? pblank : (unsigned char*)&p3begin[0]), (p3end - p3begin) * sizeof(p3begin[0]));
//    SHA256_Final((unsigned char*)&hash1, &ctx);
//    uint256 hash2;
//    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
//    return hash2;
//}

//template<typename T>
//uint256 SerializeHash(const T& obj, int nType=SER_GETHASH, int nVersion=PROTOCOL_VERSION)
//{
//    CHashWriter ss(nType, nVersion);
//    ss << obj;
//    return ss.GetHash();
//}

//template<typename T1>
//inline uint160 Hash160(const T1 pbegin, const T1 pend)
//{
//    static unsigned char pblank[1];
//    uint256 hash1;
//    SHA256((pbegin == pend ? pblank : (unsigned char*)&pbegin[0]), (pend - pbegin) * sizeof(pbegin[0]), (unsigned char*)&hash1);
//    uint160 hash2;
//    RIPEMD160((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
//    return hash2;
//}

//inline uint160 Hash160(const std::vector<unsigned char>& vch)
//{
//    return Hash160(vch.begin(), vch.end());
//}

unsigned int MurmurHash3(unsigned int nHashSeed, const std::vector<unsigned char>& vDataToHash);

/* ----------- Dash Hash ------------------------------------------------ */
template<typename T1>
inline uint256 HashX11(const T1 pbegin, const T1 pend)

{
    sph_blake512_context     ctx_blake;
    sph_bmw512_context       ctx_bmw;
    sph_groestl512_context   ctx_groestl;
    sph_jh512_context        ctx_jh;
    sph_keccak512_context    ctx_keccak;
    sph_skein512_context     ctx_skein;
    sph_luffa512_context     ctx_luffa;
    sph_cubehash512_context  ctx_cubehash;
    sph_shavite512_context   ctx_shavite;
    sph_simd512_context      ctx_simd;
    sph_echo512_context      ctx_echo;
    static unsigned char pblank[1];

    uint512 hash[11];

    sph_blake512_init(&ctx_blake);
    sph_blake512 (&ctx_blake, (pbegin == pend ? pblank : static_cast<const void*>(&pbegin[0])), (pend - pbegin) * sizeof(pbegin[0]));
    sph_blake512_close(&ctx_blake, static_cast<void*>(&hash[0]));

    sph_bmw512_init(&ctx_bmw);
    sph_bmw512 (&ctx_bmw, static_cast<const void*>(&hash[0]), 64);
    sph_bmw512_close(&ctx_bmw, static_cast<void*>(&hash[1]));

    sph_groestl512_init(&ctx_groestl);
    sph_groestl512 (&ctx_groestl, static_cast<const void*>(&hash[1]), 64);
    sph_groestl512_close(&ctx_groestl, static_cast<void*>(&hash[2]));

    sph_skein512_init(&ctx_skein);
    sph_skein512 (&ctx_skein, static_cast<const void*>(&hash[2]), 64);
    sph_skein512_close(&ctx_skein, static_cast<void*>(&hash[3]));

    sph_jh512_init(&ctx_jh);
    sph_jh512 (&ctx_jh, static_cast<const void*>(&hash[3]), 64);
    sph_jh512_close(&ctx_jh, static_cast<void*>(&hash[4]));

    sph_keccak512_init(&ctx_keccak);
    sph_keccak512 (&ctx_keccak, static_cast<const void*>(&hash[4]), 64);
    sph_keccak512_close(&ctx_keccak, static_cast<void*>(&hash[5]));

    sph_luffa512_init(&ctx_luffa);
    sph_luffa512 (&ctx_luffa, static_cast<void*>(&hash[5]), 64);
    sph_luffa512_close(&ctx_luffa, static_cast<void*>(&hash[6]));

    sph_cubehash512_init(&ctx_cubehash);
    sph_cubehash512 (&ctx_cubehash, static_cast<const void*>(&hash[6]), 64);
    sph_cubehash512_close(&ctx_cubehash, static_cast<void*>(&hash[7]));

    sph_shavite512_init(&ctx_shavite);
    sph_shavite512(&ctx_shavite, static_cast<const void*>(&hash[7]), 64);
    sph_shavite512_close(&ctx_shavite, static_cast<void*>(&hash[8]));

    sph_simd512_init(&ctx_simd);
    sph_simd512 (&ctx_simd, static_cast<const void*>(&hash[8]), 64);
    sph_simd512_close(&ctx_simd, static_cast<void*>(&hash[9]));

    sph_echo512_init(&ctx_echo);
    sph_echo512 (&ctx_echo, static_cast<const void*>(&hash[9]), 64);
    sph_echo512_close(&ctx_echo, static_cast<void*>(&hash[10]));

    return hash[10].trim256();
}

#endif
