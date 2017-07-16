//*****************************************************************************
//*****************************************************************************

#include "datasigner.h"
#include "serialize.h"
#include "base58.h"
#include "tinyformat.h"
#include "servicenode/activeservicenode.h"

//*****************************************************************************
//*****************************************************************************
DataSigner::DataSigner()
{

}

//*****************************************************************************
//*****************************************************************************
bool DataSigner::IsVinAssociatedWithPubkey(const CTxIn& txin, const CPubKey& pubkey)
{
    CScript payee;
    payee.SetDestination(pubkey.GetID());

    CTransaction tx;
    uint256 hash;
    if (GetTransaction(txin.prevout.hash, tx, hash))
    {
        for (const CTxOut & out : tx.vout)
        {
            if (out.nValue == SERVICENODE_AMOUNT*COIN && out.scriptPubKey == payee)
            {
                return true;
            }
        }
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool DataSigner::GetKeysFromSecret(std::string strSecret, CKey & keyRet, CPubKey & pubkeyRet)
{
    CBitcoinSecret vchSecret;
    if (!vchSecret.SetString(strSecret))
    {
        return false;
    }


    bool compressed = false;
    CSecret secret = vchSecret.GetSecret(compressed);

    keyRet.SetSecret(secret, compressed);
    pubkeyRet = keyRet.GetPubKey();

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool DataSigner::SignMessage(std::string strMessage, std::vector<unsigned char>& vchSigRet, CKey key)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    return key.SignCompact(Hash(ss.begin(), ss.end()), vchSigRet);
}

//*****************************************************************************
//*****************************************************************************
bool DataSigner::VerifyMessage(CPubKey pubkey, const std::vector<unsigned char>& vchSig, std::string strMessage, std::string& strErrorRet)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

//    CPubKey pubkeyFromSig;
//    if (!pubkeyFromSig.RecoverCompact(ss.GetHash(), vchSig))
//    {
//        strErrorRet = "Error recovering public key.";
//        return false;
//    }

    CKey key;
    if (!key.SetCompactSignature(Hash(ss.begin(), ss.end()), vchSig))
    {
        strErrorRet = "Error recovering public key.";
        return false;
    }

    if (key.GetPubKey().GetID() != pubkey.GetID())
    {
        strErrorRet = strprintf("Keys don't match: pubkey=%s, pubkeyFromSig=%s, strMessage=%s, vchSig=%s",
                    pubkey.GetID().ToString(), key.GetPubKey().GetID().ToString(), strMessage,
                    EncodeBase64(&vchSig[0], vchSig.size()));
        return false;
    }

    return true;
}
