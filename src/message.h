//*****************************************************************************
//*****************************************************************************

#ifndef MESSAGE_H
#define MESSAGE_H

#include "uint256.h"
#include "serialize.h"
#include "sync.h"
#include "key.h"

#include <vector>
#include <string>
#include <set>

#include <boost/thread/recursive_mutex.hpp>

class CNode;

//*****************************************************************************
//*****************************************************************************
struct Message
{
    enum
    {
        maxMessageSize = 1024,
        signatureSize = 65,

        // length of header, 4 + 1 + 8 + 20 + 16 + 33 + 32 + 4 +4
        headerLength   = 122,

        // length of encrypted header
        encryptedHeaderLength = 1+20+65+4
    };

    std::string from;
    std::string to;
    std::string date;
    std::string text;
    std::vector<unsigned char> signature;

    // generated key for encription
    std::vector<unsigned char> publicRKey;
    // encrypted data
    std::vector<unsigned char> encryptedData;
    // message auth code
    std::vector<unsigned char> mac;
    //
    std::vector<unsigned char> iv;
    //
    time_t timestamp;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(from);
        READWRITE(to);
        READWRITE(date);
        READWRITE(text);
        // READWRITE(signature);
        READWRITE(publicRKey);
        READWRITE(encryptedData);
        READWRITE(mac);
        READWRITE(iv);
        try { nSerSize += ::SerReadWrite(s, timestamp, nType, nVersion, ser_action); }
        catch (std::exception &) { }
    )


    Message() { timestamp = std::time(0); }
    Message(const Message & other) { *this = other; }

    Message & operator = (const Message & other)
    {
        from          = other.from;
        to            = other.to;
        date          = other.date;
        text          = other.text;
        signature     = other.signature;
        publicRKey    = other.publicRKey;
        encryptedData = other.encryptedData;
        mac           = other.mac;
        iv            = other.iv;
        timestamp     = other.timestamp;
        return *this;
    }

    bool operator < (const Message & other) const
    {
        return date < other.date;
    }

    // full hash
    uint256 getNetworkHash() const;
    // hash without timestamp
    uint256 getHash() const;
    // hash without date
    uint256 getStaticHash() const;

    bool appliesToMe() const;

    time_t getTime() const;
    bool isExpired() const;

    bool send();
    bool process(bool & isForMe);

    static bool processReceived(const uint256 & hash);

    bool relayTo(CNode * pnode) const;
    bool broadcast() const;

    bool sign(CKey & key);
    // bool verify(std::vector<unsigned char> sig);

    bool encrypt(const CPubKey & senderPubKey);
    bool decrypt(const CKey & receiverKey,
                 bool & isMessageForMy,
                 CPubKey & senderPubKey);

    bool isEmpty() const;

private:
    static boost::recursive_mutex m_knownMessagesLocker;
    static std::set<uint256>      m_knownMessages;
};

//*****************************************************************************
//*****************************************************************************
class MessageCrypter
{
private:
    unsigned char chKey[32];
    unsigned char chIV[16];
    bool fKeySet;
public:

    MessageCrypter()
    {
        // Try to keep the key data out of swap (and be a bit over-careful to keep the IV that we don't even use out of swap)
        // Note that this does nothing about suspend-to-disk (which will put all our key data on disk)
        // Note as well that at no point in this program is any attempt made to prevent stealing of keys by reading the memory of the running process.
        LockedPageManager::instance.LockRange(&chKey[0], sizeof chKey);
        LockedPageManager::instance.LockRange(&chIV[0], sizeof chIV);
        fKeySet = false;
    }

    ~MessageCrypter()
    {
        // clean key
        memset(&chKey, 0, sizeof chKey);
        memset(&chIV, 0, sizeof chIV);
        fKeySet = false;

        LockedPageManager::instance.UnlockRange(&chKey[0], sizeof chKey);
        LockedPageManager::instance.UnlockRange(&chIV[0], sizeof chIV);
    }

    bool SetKey(const std::vector<unsigned char>& vchNewKey, unsigned char* chNewIV);
    bool Encrypt(unsigned char* chPlaintext, uint32_t nPlain, std::vector<unsigned char> &vchCiphertext);
    bool Decrypt(unsigned char* chCiphertext, uint32_t nCipher, std::vector<unsigned char>& vchPlaintext);
};

#endif // MESSAGE_H
