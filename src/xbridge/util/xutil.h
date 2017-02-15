//*****************************************************************************
//*****************************************************************************

#ifndef UTIL_H
#define UTIL_H

#include "uint256.h"
#include "serialize.h"

#include "logger.h"
#include "../crypto/ripemd160.h"
#include "../crypto/sha256.h"

#include <string>

#include <openssl/sha.h>
#include <openssl/ripemd.h>

#define BEGIN(a)            ((char*)&(a))
#define END(a)              ((char*)&((&(a))[1]))

//*****************************************************************************
//*****************************************************************************
namespace util
{
    void init();

    std::wstring wide_string(std::string const & s);//, std::locale const &loc);
    // std::string narrow_string(std::wstring const &s, char default_char = '?');//, std::locale const &loc, char default_char = '?');

    std::string mb_string(std::string const & s);
    std::string mb_string(std::wstring const & s);

    std::string base64_encode(const std::vector<unsigned char> & s);
    std::string base64_encode(const std::string & s);
    std::string base64_decode(const std::string & s);

    std::string to_str(const uint256 & obj);

    // TODO implement
    // std::vector<unsigned char> strToAddress(const std::string & addr);

} // namespace


//std::string real_strprintf(const std::string &format, int dummy, ...);
//#define strprintf(format, ...) real_strprintf(format, 0, __VA_ARGS__)

//std::string HexStr(const std::vector<unsigned char>& vch, bool fSpaces=false);

//std::vector<unsigned char> ParseHex(const char* psz);
//std::vector<unsigned char> ParseHex(const std::string& str);

/** A hasher class for Bitcoin's 256-bit hash (double SHA-256). */
class CHash256 {
private:
    CSHA256 sha;
public:
    static const size_t OUTPUT_SIZE = CSHA256::OUTPUT_SIZE;

    void Finalize(unsigned char hash[OUTPUT_SIZE]) {
        unsigned char buf[sha.OUTPUT_SIZE];
        sha.Finalize(buf);
        sha.Reset().Write(buf, sha.OUTPUT_SIZE).Finalize(hash);
    }

    CHash256& Write(const unsigned char *data, size_t len) {
        sha.Write(data, len);
        return *this;
    }

    CHash256& Reset() {
        sha.Reset();
        return *this;
    }
};

/** A hasher class for Bitcoin's 160-bit hash (SHA-256 + RIPEMD-160). */
class CHash160 {
private:
    CSHA256 sha;
public:
    static const size_t OUTPUT_SIZE = CRIPEMD160::OUTPUT_SIZE;

    void Finalize(unsigned char hash[OUTPUT_SIZE]) {
        unsigned char buf[sha.OUTPUT_SIZE];
        sha.Finalize(buf);
        CRIPEMD160().Write(buf, sha.OUTPUT_SIZE).Finalize(hash);
    }

    CHash160& Write(const unsigned char *data, size_t len) {
        sha.Write(data, len);
        return *this;
    }

    CHash160& Reset() {
        sha.Reset();
        return *this;
    }
};

/** Compute the 160-bit hash an object. */
template<typename T1>
inline uint160 Hash160(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1] = {};
    uint160 result;
    CHash160().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
              .Finalize((unsigned char*)&result);
    return result;
}

#endif // UTIL_H
