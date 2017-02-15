//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEWALLET_H
#define XBRIDGEWALLET_H

#include <string>
#include <vector>
#include <stdint.h>

//*****************************************************************************
//*****************************************************************************
struct WalletParam
{
    std::string                title;
    std::string                currency;
    std::string                address;
    std::string                ip;
    std::string                port;
    std::string                user;
    std::string                passwd;
    char                       addrPrefix[8];
    char                       scriptPrefix[8];
    char                       secretPrefix[8];
    std::string                taxaddr;
    unsigned int               fee;
    uint64_t                   COIN;
    uint64_t                   minTxFee;
    uint64_t                   minAmount;
    uint64_t                   dustAmount;
    std::string                method;
    bool                       isGetNewPubKeySupported;
    bool                       isImportWithNoScanSupported;

    // block time in seconds
    uint32_t                   blockTime;

    WalletParam()
        : fee(300)
        , COIN(0)
        , minAmount(0)
        , dustAmount(0)
        , isGetNewPubKeySupported(false)
        , isImportWithNoScanSupported(false)
        , blockTime(0)
    {
        memset(addrPrefix,   0, sizeof(addrPrefix));
        memset(scriptPrefix, 0, sizeof(scriptPrefix));
        memset(secretPrefix, 0, sizeof(secretPrefix));
    }
};


#endif // XBRIDGEWALLET_H
