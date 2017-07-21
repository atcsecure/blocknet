//*****************************************************************************
//*****************************************************************************

#include "xutil.h"
#include "logger.h"

#include <boost/locale.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include <sstream>
#include <string>
#include <stdio.h>
#include <stdarg.h>

#include <openssl/rand.h>

#ifndef WIN32
#include <execinfo.h>
#endif

//*****************************************************************************
//*****************************************************************************
namespace util
{

std::locale loc;

//******************************************************************************
//******************************************************************************
void init()
{
    try
    {
        loc = std::locale ("en_US.UTF8");
    }
    catch (std::runtime_error & e)
    {
        LOG() << "use default locale, " << e.what();
        loc = std::locale (loc, "", std::locale::ctype);
    }
}

//******************************************************************************
//******************************************************************************
std::wstring wide_string(std::string const &s)//, std::locale const &loc)
{
    if (s.empty())
    {
        return std::wstring();
    }

    std::ctype<wchar_t> const &facet = std::use_facet<std::ctype<wchar_t> >(loc);
    char const *first = s.c_str();
    char const *last = first + s.size();
    std::vector<wchar_t> result(s.size());

    facet.widen(first, last, &result[0]);

    return std::wstring(result.begin(), result.end());
}

//******************************************************************************
//******************************************************************************
//std::string narrow_string(std::wstring const &s, char default_char)//, std::locale const &loc, char default_char)
//{
//    if (s.empty())
//    {
//        return std::string();
//    }

//    std::ctype<wchar_t> const &facet = std::use_facet<std::ctype<wchar_t> >(loc);
//    wchar_t const *first = s.c_str();
//    wchar_t const *last = first + s.size();
//    std::vector<char> result(s.size());

//    facet.narrow(first, last, default_char, &result[0]);

//    return std::string(result.begin(), result.end());
//}

//******************************************************************************
//******************************************************************************
std::string mb_string(std::string const &s)
{
    return mb_string(wide_string(s));
}

//******************************************************************************
//******************************************************************************
std::string mb_string(std::wstring const &s)
{
    return boost::locale::conv::utf_to_utf<char>(s);
}

//*****************************************************************************
//*****************************************************************************
const std::string base64_padding[] = {"", "==","="};

//*****************************************************************************
//*****************************************************************************
std::string base64_encode(const std::vector<unsigned char> & s)
{
    return base64_encode(std::string((char *)&s[0], s.size()));
}

//*****************************************************************************
//*****************************************************************************
std::string base64_encode(const std::string& s)
{
    namespace bai = boost::archive::iterators;

    std::stringstream os;

    // convert binary values to base64 characters
    typedef bai::base64_from_binary
    // retrieve 6 bit integers from a sequence of 8 bit bytes
    <bai::transform_width<const char *, 6, 8> > base64_enc; // compose all the above operations in to a new iterator

    std::copy(base64_enc(s.c_str()), base64_enc(s.c_str() + s.size()),
            std::ostream_iterator<char>(os));

    os << base64_padding[s.size() % 3];
    return os.str();
}

//*****************************************************************************
//*****************************************************************************
std::string base64_decode(const std::string& s)
{
    try
    {
        namespace bai = boost::archive::iterators;

        std::stringstream os;

        typedef bai::transform_width<bai::binary_from_base64<const char *>, 8, 6> base64_dec;

        unsigned int size = s.size();

        // Remove the padding characters, cf. https://svn.boost.org/trac/boost/ticket/5629
        if (size && s[size - 1] == '=')
        {
            --size;
            if (size && s[size - 1] == '=')
            {
                --size;
            }
        }
        if (size == 0)
        {
            return std::string();
        }

        std::copy(base64_dec(s.data()), base64_dec(s.data() + size),
                std::ostream_iterator<char>(os));

        return os.str();
    }
    // catch (std::exception &)
    catch (...)
    {
    }
    return std::string();
}

//*****************************************************************************
//******************************************************************************
std::string to_str(const uint256 & obj)
{
    return util::base64_encode(std::string((char *)obj.begin(), 32));
}

} // namespace util
