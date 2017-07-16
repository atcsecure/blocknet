// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SERVICENODE_H
#define SERVICENODE_H

#include "serialize.h"
#include "key.h"
#include "main.h"
#include "net.h"
// #include "spork.h"
// #include "timedata.h"

class CServicenode;
class CServicenodeBroadcast;
class CServicenodePing;

static const int SERVICENODE_CHECK_SECONDS               =   5;
static const int SERVICENODE_MIN_MNB_SECONDS             =   5 * 60;
static const int SERVICENODE_MIN_MNP_SECONDS             =  10 * 60;
static const int SERVICENODE_EXPIRATION_SECONDS          =  65 * 60;
static const int SERVICENODE_WATCHDOG_MAX_SECONDS        = 120 * 60;
static const int SERVICENODE_NEW_START_REQUIRED_SECONDS  = 180 * 60;

static const int SERVICENODE_POSE_BAN_MAX_SCORE          = 5;
//
// The Servicenode Ping Class : Contains a different serialize method for sending pings from servicenodes throughout the network
//

class CServicenodePing
{
public:
    CTxIn vin;
    uint256 blockHash;
    int64_t sigTime; //mnb message times
    std::vector<unsigned char> vchSig;
    //removed stop

    CServicenodePing() :
        vin(),
        blockHash(),
        sigTime(0),
        vchSig()
        {}

    CServicenodePing(const CTxIn & vinNew);

    IMPLEMENT_SERIALIZE(
        READWRITE(vin);
        READWRITE(blockHash);
        READWRITE(sigTime);
        READWRITE(vchSig);
    )

    void swap(CServicenodePing& first, CServicenodePing& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.blockHash, second.blockHash);
        swap(first.sigTime, second.sigTime);
        swap(first.vchSig, second.vchSig);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << sigTime;
        return ss.GetHash();
    }

    bool IsExpired() { return GetTime() - sigTime > SERVICENODE_NEW_START_REQUIRED_SECONDS; }

    bool Sign(const CKey & keyServicenode, const CPubKey & pubKeyServicenode);
    bool CheckSignature(CPubKey& pubKeyServicenode, int &nDos);
    bool SimpleCheck(int& nDos);
    bool CheckAndUpdate(CServicenode* pmn, bool fFromNewBroadcast, int& nDos);
    void Relay();

    CServicenodePing& operator=(CServicenodePing from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CServicenodePing& a, const CServicenodePing& b)
    {
        return a.vin == b.vin && a.blockHash == b.blockHash;
    }
    friend bool operator!=(const CServicenodePing& a, const CServicenodePing& b)
    {
        return !(a == b);
    }

};

struct servicenode_info_t
{
    servicenode_info_t()
        : vin(),
          addr(),
          pubKeyCollateralAddress(),
          pubKeyServicenode(),
          sigTime(0),
          nLastDsq(0),
          nTimeLastChecked(0),
          nTimeLastPaid(0),
          nTimeLastWatchdogVote(0),
          nTimeLastPing(0),
          nActiveState(0),
          nProtocolVersion(0),
          fInfoValid(false)
        {}

    CTxIn vin;
    CService addr;
    CPubKey pubKeyCollateralAddress;
    CPubKey pubKeyServicenode;
    int64_t sigTime; //mnb message time
    int64_t nLastDsq; //the dsq count from the last dsq broadcast of this node
    int64_t nTimeLastChecked;
    int64_t nTimeLastPaid;
    int64_t nTimeLastWatchdogVote;
    int64_t nTimeLastPing;
    int nActiveState;
    int nProtocolVersion;
    bool fInfoValid;
};

//
// The Servicenode Class. It contains the input of the servicenode amount, signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//
class CServicenode
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

public:
    enum state {
        SERVICENODE_PRE_ENABLED,
        SERVICENODE_ENABLED,
        SERVICENODE_EXPIRED,
        SERVICENODE_OUTPOINT_SPENT,
        SERVICENODE_UPDATE_REQUIRED,
        SERVICENODE_WATCHDOG_EXPIRED,
        SERVICENODE_NEW_START_REQUIRED,
        SERVICENODE_POSE_BAN
    };

    CTxIn vin;
    CService addr;
    CPubKey pubKeyCollateralAddress;
    CPubKey pubKeyServicenode;
    CServicenodePing lastPing;
    std::vector<unsigned char> vchSig;
    int64_t sigTime; //mnb message time
    int64_t nLastDsq; //the dsq count from the last dsq broadcast of this node
    int64_t nTimeLastChecked;
    int64_t nTimeLastPaid;
    int64_t nTimeLastWatchdogVote;
    int nActiveState;
    int nCacheCollateralBlock;
    int nBlockLastPaid;
    int nProtocolVersion;
    int nPoSeBanScore;
    int nPoSeBanHeight;
    bool fAllowMixingTx;
    bool fUnitTest;

    // KEEP TRACK OF GOVERNANCE ITEMS EACH SERVICENODE HAS VOTE UPON FOR RECALCULATION
    std::map<uint256, int> mapGovernanceObjectsVotedOn;

    CServicenode();
    CServicenode(const CServicenode& other);
    CServicenode(const CServicenodeBroadcast& mnb);
    CServicenode(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyServicenodeNew, int nProtocolVersionIn);

    IMPLEMENT_SERIALIZE(
        LOCK(cs);
        READWRITE(vin);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyServicenode);
        READWRITE(lastPing);
        READWRITE(vchSig);
        READWRITE(sigTime);
        READWRITE(nLastDsq);
        READWRITE(nTimeLastChecked);
        READWRITE(nTimeLastPaid);
        READWRITE(nTimeLastWatchdogVote);
        READWRITE(nActiveState);
        READWRITE(nCacheCollateralBlock);
        READWRITE(nBlockLastPaid);
        READWRITE(nProtocolVersion);
        READWRITE(nPoSeBanScore);
        READWRITE(nPoSeBanHeight);
        READWRITE(fAllowMixingTx);
        READWRITE(fUnitTest);
        READWRITE(mapGovernanceObjectsVotedOn);
    )

    void swap(CServicenode& first, CServicenode& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.addr, second.addr);
        swap(first.pubKeyCollateralAddress, second.pubKeyCollateralAddress);
        swap(first.pubKeyServicenode, second.pubKeyServicenode);
        swap(first.lastPing, second.lastPing);
        swap(first.vchSig, second.vchSig);
        swap(first.sigTime, second.sigTime);
        swap(first.nLastDsq, second.nLastDsq);
        swap(first.nTimeLastChecked, second.nTimeLastChecked);
        swap(first.nTimeLastPaid, second.nTimeLastPaid);
        swap(first.nTimeLastWatchdogVote, second.nTimeLastWatchdogVote);
        swap(first.nActiveState, second.nActiveState);
        swap(first.nCacheCollateralBlock, second.nCacheCollateralBlock);
        swap(first.nBlockLastPaid, second.nBlockLastPaid);
        swap(first.nProtocolVersion, second.nProtocolVersion);
        swap(first.nPoSeBanScore, second.nPoSeBanScore);
        swap(first.nPoSeBanHeight, second.nPoSeBanHeight);
        swap(first.fAllowMixingTx, second.fAllowMixingTx);
        swap(first.fUnitTest, second.fUnitTest);
        swap(first.mapGovernanceObjectsVotedOn, second.mapGovernanceObjectsVotedOn);
    }

    // CALCULATE A RANK AGAINST OF GIVEN BLOCK
    arith_uint256 CalculateScore(const uint256& blockHash);

    bool UpdateFromNewBroadcast(CServicenodeBroadcast& mnb);

    void Check(bool fForce = false);

    bool IsBroadcastedWithin(int nSeconds) { return GetAdjustedTime() - sigTime < nSeconds; }

    bool IsPingedWithin(int nSeconds, int64_t nTimeToCheckAt = -1)
    {
        if(lastPing == CServicenodePing()) return false;

        if(nTimeToCheckAt == -1) {
            nTimeToCheckAt = GetAdjustedTime();
        }
        return nTimeToCheckAt - lastPing.sigTime < nSeconds;
    }

    bool IsEnabled() { return nActiveState == SERVICENODE_ENABLED; }
    bool IsPreEnabled() { return nActiveState == SERVICENODE_PRE_ENABLED; }
    bool IsPoSeBanned() { return nActiveState == SERVICENODE_POSE_BAN; }
    // NOTE: this one relies on nPoSeBanScore, not on nActiveState as everything else here
    bool IsPoSeVerified() { return nPoSeBanScore <= -SERVICENODE_POSE_BAN_MAX_SCORE; }
    bool IsExpired() { return nActiveState == SERVICENODE_EXPIRED; }
    bool IsOutpointSpent() { return nActiveState == SERVICENODE_OUTPOINT_SPENT; }
    bool IsUpdateRequired() { return nActiveState == SERVICENODE_UPDATE_REQUIRED; }
    bool IsWatchdogExpired() { return nActiveState == SERVICENODE_WATCHDOG_EXPIRED; }
    bool IsNewStartRequired() { return nActiveState == SERVICENODE_NEW_START_REQUIRED; }

    static bool IsValidStateForAutoStart(int nActiveStateIn)
    {
        return  nActiveStateIn == SERVICENODE_ENABLED ||
                nActiveStateIn == SERVICENODE_PRE_ENABLED ||
                nActiveStateIn == SERVICENODE_EXPIRED ||
                nActiveStateIn == SERVICENODE_WATCHDOG_EXPIRED;
    }

    bool IsValidForPayment()
    {
        if(nActiveState == SERVICENODE_ENABLED) {
            return true;
        }
//        if(!sporkManager.IsSporkActive(SPORK_14_REQUIRE_SENTINEL_FLAG) &&
//           (nActiveState == SERVICENODE_WATCHDOG_EXPIRED)) {
//            return true;
//        }

        return false;
    }

    bool IsValidNetAddr();
    static bool IsValidNetAddr(CService addrIn);

    void IncreasePoSeBanScore() { if(nPoSeBanScore < SERVICENODE_POSE_BAN_MAX_SCORE) nPoSeBanScore++; }
    void DecreasePoSeBanScore() { if(nPoSeBanScore > -SERVICENODE_POSE_BAN_MAX_SCORE) nPoSeBanScore--; }

    servicenode_info_t GetInfo();

    static std::string StateToString(int nStateIn);
    std::string GetStateString() const;
    std::string GetStatus() const;

    int GetCollateralAge();

    int GetLastPaidTime() { return nTimeLastPaid; }
    int GetLastPaidBlock() { return nBlockLastPaid; }
    void UpdateLastPaid(const CBlockIndex *pindex, int nMaxBlocksToScanBack);

    // KEEP TRACK OF EACH GOVERNANCE ITEM INCASE THIS NODE GOES OFFLINE, SO WE CAN RECALC THEIR STATUS
    void AddGovernanceVote(uint256 nGovernanceObjectHash);
    // RECALCULATE CACHED STATUS FLAGS FOR ALL AFFECTED OBJECTS
    void FlagGovernanceItemsAsDirty();

    void RemoveGovernanceObject(uint256 nGovernanceObjectHash);

    void UpdateWatchdogVoteTime();

    CServicenode& operator=(CServicenode from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CServicenode& a, const CServicenode& b)
    {
        return a.vin == b.vin;
    }
    friend bool operator!=(const CServicenode& a, const CServicenode& b)
    {
        return !(a.vin == b.vin);
    }

};


//
// The Servicenode Broadcast Class : Contains a different serialize method for sending servicenodes through the network
//

class CServicenodeBroadcast : public CServicenode
{
public:

    bool fRecovery;

    CServicenodeBroadcast() : CServicenode(), fRecovery(false) {}
    CServicenodeBroadcast(const CServicenode& mn) : CServicenode(mn), fRecovery(false) {}
    CServicenodeBroadcast(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyServicenodeNew, int nProtocolVersionIn) :
        CServicenode(addrNew, vinNew, pubKeyCollateralAddressNew, pubKeyServicenodeNew, nProtocolVersionIn), fRecovery(false) {}

    IMPLEMENT_SERIALIZE(
        READWRITE(vin);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyServicenode);
        READWRITE(vchSig);
        READWRITE(sigTime);
        READWRITE(nProtocolVersion);
        READWRITE(lastPing);
    )

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << pubKeyCollateralAddress;
        ss << sigTime;
        return ss.GetHash();
    }

    /// Create Servicenode broadcast, needs to be relayed manually after that
    static bool Create(const CTxIn & txin, const CService & service,
                       const CKey & keyCollateralAddressNew,
                       const CPubKey & pubKeyCollateralAddressNew,
                       const CKey & keyServicenodeNew,
                       const CPubKey & pubKeyServicenodeNew,
                       std::string & strErrorRet, CServicenodeBroadcast & mnbRet);
    static bool Create(const std::string & strService, const std::string & strKeyServicenode,
                       const std::string & strTxHash, const std::string & strOutputIndex,
                       std::string & strErrorRet, CServicenodeBroadcast & mnbRet,
                       const bool fOffline = false);

    bool SimpleCheck(int& nDos);
    bool Update(CServicenode* pmn, int& nDos);
    bool CheckOutpoint(int& nDos);

    bool Sign(const CKey & keyCollateralAddress);
    bool CheckSignature(int& nDos);
    void Relay();
};

class CServicenodeVerification
{
public:
    CTxIn vin1;
    CTxIn vin2;
    CService addr;
    int nonce;
    int nBlockHeight;
    std::vector<unsigned char> vchSig1;
    std::vector<unsigned char> vchSig2;

    CServicenodeVerification() :
        vin1(),
        vin2(),
        addr(),
        nonce(0),
        nBlockHeight(0),
        vchSig1(),
        vchSig2()
        {}

    CServicenodeVerification(CService addr, int nonce, int nBlockHeight) :
        vin1(),
        vin2(),
        addr(addr),
        nonce(nonce),
        nBlockHeight(nBlockHeight),
        vchSig1(),
        vchSig2()
        {}

    IMPLEMENT_SERIALIZE(
        READWRITE(vin1);
        READWRITE(vin2);
        READWRITE(addr);
        READWRITE(nonce);
        READWRITE(nBlockHeight);
        READWRITE(vchSig1);
        READWRITE(vchSig2);
    )

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin1;
        ss << vin2;
        ss << addr;
        ss << nonce;
        ss << nBlockHeight;
        return ss.GetHash();
    }

    void Relay() const
    {
        CInv inv(MSG_SERVICENODE_VERIFY, GetHash());
        RelayInventory(inv);
    }
};

#endif
