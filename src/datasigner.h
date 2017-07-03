//*****************************************************************************
//*****************************************************************************

#ifndef DATASIGNER_H
#define DATASIGNER_H

#include "main.h"
#include "key.h"

#include <string>

//*****************************************************************************
/** Helper object for signing and checking signatures
 */
//*****************************************************************************
class DataSigner
{
public:
    DataSigner();

public:
    /// Is the input associated with this public key? (and there is 1000 DASH - checking if valid masternode)
    static bool IsVinAssociatedWithPubkey(const CTxIn& vin, const CPubKey& pubkey);
    /// Set the private/public key values, returns true if successful
    static bool GetKeysFromSecret(std::string strSecret, CKey& keyRet, CPubKey& pubkeyRet);
    /// Sign the message, returns true if successful
    static bool SignMessage(std::string strMessage, std::vector<unsigned char>& vchSigRet, CKey key);
    /// Verify the message, returns true if succcessful
    static bool VerifyMessage(CPubKey pubkey, const std::vector<unsigned char>& vchSig, std::string strMessage, std::string& strErrorRet);
};

#endif // DATASIGNER_H
