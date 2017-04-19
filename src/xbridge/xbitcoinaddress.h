//*****************************************************************************
//*****************************************************************************

#ifndef XBITCOINADDRESS_H
#define XBITCOINADDRESS_H

//*****************************************************************************
//*****************************************************************************
#include "base58.h"

namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
class XBitcoinAddress : public CBase58Data
{
public:
    enum
    {
        PUBKEY_ADDRESS = 75,  // XCurrency: address begin with 'X'
        SCRIPT_ADDRESS = 8,
        PUBKEY_ADDRESS_TEST = 111,
        SCRIPT_ADDRESS_TEST = 196,
    };

    bool Set(const CKeyID &id, const char prefix) {
        // SetData(fTestNet ? PUBKEY_ADDRESS_TEST : PUBKEY_ADDRESS, &id, 20);
        SetData(prefix, &id, 20);
        return true;
    }

    bool Set(const CScriptID &id, const char prefix) {
        // SetData(fTestNet ? SCRIPT_ADDRESS_TEST : SCRIPT_ADDRESS, &id, 20);
        SetData(prefix, &id, 20);
        return true;
    }

//    bool Set(const CTxDestination &dest)
//    {
//        return boost::apply_visitor(CBitcoinAddressVisitor(this), dest);
//    }

    bool IsValid() const
    {
        unsigned int nExpectedSize = 20;
        return vchData.size() == nExpectedSize;

        bool fExpectTestNet = false;
        switch(nVersion)
        {
            case PUBKEY_ADDRESS:
                nExpectedSize = 20; // Hash of public key
                fExpectTestNet = false;
                break;
            case SCRIPT_ADDRESS:
                nExpectedSize = 20; // Hash of CScript
                fExpectTestNet = false;
                break;

            case PUBKEY_ADDRESS_TEST:
                nExpectedSize = 20;
                fExpectTestNet = true;
                break;
            case SCRIPT_ADDRESS_TEST:
                nExpectedSize = 20;
                fExpectTestNet = true;
                break;

            default:
                return false;
        }
        return fExpectTestNet == fTestNet && vchData.size() == nExpectedSize;
    }

    XBitcoinAddress()
    {
    }

//    CBitcoinAddress(const CTxDestination &dest)
//    {
//        Set(dest);
//    }

    XBitcoinAddress(const std::string& strAddress)
    {
        SetString(strAddress);
    }

    XBitcoinAddress(const char* pszAddress)
    {
        SetString(pszAddress);
    }

    /*CTxDestination*/
    CKeyID Get() const {
        if (!IsValid())
            // return CNoDestination();
            return CKeyID();

        uint160 id;
        memcpy(&id, &vchData[0], 20);
        return CKeyID(id);

//        switch (nVersion) {
//        case PUBKEY_ADDRESS:
//        case PUBKEY_ADDRESS_TEST: {
//            uint160 id;
//            memcpy(&id, &vchData[0], 20);
//            return CKeyID(id);
//        }
//        case SCRIPT_ADDRESS:
//        case SCRIPT_ADDRESS_TEST: {
//            uint160 id;
//            memcpy(&id, &vchData[0], 20);
//            return CScriptID(id);
//        }
//        }
//        return CNoDestination();
    }

    bool GetKeyID(CKeyID &keyID) const {
        if (!IsValid())
            return false;

        uint160 id;
        memcpy(&id, &vchData[0], 20);
        keyID = CKeyID(id);
        return true;

        switch (nVersion) {
        case PUBKEY_ADDRESS:
        case PUBKEY_ADDRESS_TEST: {
            uint160 id;
            memcpy(&id, &vchData[0], 20);
            keyID = CKeyID(id);
            return true;
        }
        default: return false;
        }
    }

    bool IsScript() const {
        if (!IsValid())
            return false;
        switch (nVersion) {
        case SCRIPT_ADDRESS:
        case SCRIPT_ADDRESS_TEST: {
            return true;
        }
        default: return false;
        }
    }
};

} // namespace

#endif // XBITCOINADDRESS_H
