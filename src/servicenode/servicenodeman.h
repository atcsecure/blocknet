// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SERVICENODEMAN_H
#define SERVICENODEMAN_H

#include "serialize.h"
#include "servicenode.h"
#include "sync.h"

using namespace std;

class CServicenodeMan;

extern CServicenodeMan mnodeman;

/**
 * Provides a forward and reverse index between MN vin's and integers.
 *
 * This mapping is normally add-only and is expected to be permanent
 * It is only rebuilt if the size of the index exceeds the expected maximum number
 * of MN's and the current number of known MN's.
 *
 * The external interface to this index is provided via delegation by CServicenodeMan
 */
class CServicenodeIndex
{
public: // Types
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

    typedef std::map<int,CTxIn> rindex_m_t;

    typedef rindex_m_t::iterator rindex_m_it;

    typedef rindex_m_t::const_iterator rindex_m_cit;

private:
    int                  nSize;

    index_m_t            mapIndex;

    rindex_m_t           mapReverseIndex;

public:
    CServicenodeIndex();

    int GetSize() const {
        return nSize;
    }

    /// Retrieve servicenode vin by index
    bool Get(int nIndex, CTxIn& vinServicenode) const;

    /// Get index of a servicenode vin
    int GetServicenodeIndex(const CTxIn& vinServicenode) const;

    void AddServicenodeVIN(const CTxIn& vinServicenode);

    void Clear();

private:
    void RebuildIndex();

public:
    IMPLEMENT_SERIALIZE(
        READWRITE(mapIndex);
        if (fRead) {
            const_cast<CServicenodeIndex *>(this)->RebuildIndex();
        }
    )

};

class CServicenodeMan
{
public:
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

private:
    static const int MAX_EXPECTED_INDEX_SIZE = 30000;

    /// Only allow 1 index rebuild per hour
    static const int64_t MIN_INDEX_REBUILD_TIME = 3600;

    static const std::string SERIALIZATION_VERSION_STRING;

    static const int DSEG_UPDATE_SECONDS        = 3 * 60 * 60;

    static const int LAST_PAID_SCAN_BLOCKS      = 100;

    static const int MIN_POSE_PROTO_VERSION     = 60027;
    static const int MAX_POSE_CONNECTIONS       = 10;
    static const int MAX_POSE_RANK              = 10;
    static const int MAX_POSE_BLOCKS            = 10;

    static const int MNB_RECOVERY_QUORUM_TOTAL      = 10;
    static const int MNB_RECOVERY_QUORUM_REQUIRED   = 6;
    static const int MNB_RECOVERY_MAX_ASK_ENTRIES   = 10;
    static const int MNB_RECOVERY_WAIT_SECONDS      = 60;
    static const int MNB_RECOVERY_RETRY_SECONDS     = 3 * 60 * 60;


    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    // map to hold all MNs
    std::vector<CServicenode> vServicenodes;
    // who's asked for the Servicenode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForServicenodeList;
    // who we asked for the Servicenode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForServicenodeList;
    // which Servicenodes we've asked for
    std::map<COutPoint, std::map<CNetAddr, int64_t> > mWeAskedForServicenodeListEntry;
    // who we asked for the servicenode verification
    std::map<CNetAddr, CServicenodeVerification> mWeAskedForVerification;

    // these maps are used for servicenode recovery from SERVICENODE_NEW_START_REQUIRED state
    std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > > mMnbRecoveryRequests;
    std::map<uint256, std::vector<CServicenodeBroadcast> > mMnbRecoveryGoodReplies;
    std::list< std::pair<CService, uint256> > listScheduledMnbRequestConnections;

    int64_t nLastIndexRebuildTime;

    CServicenodeIndex indexServicenodes;

    CServicenodeIndex indexServicenodesOld;

    /// Set when index has been rebuilt, clear when read
    bool fIndexRebuilt;

    /// Set when servicenodes are added, cleared when CGovernanceManager is notified
    bool fServicenodesAdded;

    /// Set when servicenodes are removed, cleared when CGovernanceManager is notified
    bool fServicenodesRemoved;

    std::vector<uint256> vecDirtyGovernanceObjectHashes;

    int64_t nLastWatchdogVoteTime;

    friend class CServicenodeSync;

public:
    // Keep track of all broadcasts I've seen
    std::map<uint256, std::pair<int64_t, CServicenodeBroadcast> > mapSeenServicenodeBroadcast;
    // Keep track of all pings I've seen
    std::map<uint256, CServicenodePing> mapSeenServicenodePing;
    // Keep track of all verifications I've seen
    std::map<uint256, CServicenodeVerification> mapSeenServicenodeVerification;
    // keep track of dsq count to prevent servicenodes from gaming darksend queue
    int64_t nDsqCount;


    CServicenodeMan();

    /// Add an entry
    bool Add(CServicenode &mn);

    /// Ask (source) node for mnb
    void AskForMN(CNode *pnode, const CTxIn &vin);
    void AskForMnb(CNode *pnode, const uint256 &hash);

    /// Check all Servicenodes
    void Check();

    /// Check all Servicenodes and remove inactive
    void CheckAndRemove();

    /// Clear Servicenode vector
    void Clear();

    /// Count Servicenodes filtered by nProtocolVersion.
    /// Servicenode nProtocolVersion should match or be above the one specified in param here.
    int CountServicenodes(int nProtocolVersion = -1);
    /// Count enabled Servicenodes filtered by nProtocolVersion.
    /// Servicenode nProtocolVersion should match or be above the one specified in param here.
    int CountEnabled(int nProtocolVersion = -1);

    /// Count Servicenodes by network type - NET_IPV4, NET_IPV6, NET_TOR
    // int CountByIP(int nNetworkType);

    void DsegUpdate(CNode* pnode);

    /// Find an entry
    CServicenode* Find(const CScript &payee);
    CServicenode* Find(const CTxIn& vin);
    CServicenode* Find(const CPubKey& pubKeyServicenode);

    /// Versions of Find that are safe to use from outside the class
    bool Get(const CPubKey& pubKeyServicenode, CServicenode& servicenode);
    bool Get(const CTxIn& vin, CServicenode& servicenode);

    /// Retrieve servicenode vin by index
    bool Get(int nIndex, CTxIn& vinServicenode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexServicenodes.Get(nIndex, vinServicenode);
    }

    bool GetIndexRebuiltFlag() {
        LOCK(cs);
        return fIndexRebuilt;
    }

    /// Get index of a servicenode vin
    int GetServicenodeIndex(const CTxIn& vinServicenode) {
        LOCK(cs);
        return indexServicenodes.GetServicenodeIndex(vinServicenode);
    }

    /// Get old index of a servicenode vin
    int GetServicenodeIndexOld(const CTxIn& vinServicenode) {
        LOCK(cs);
        return indexServicenodesOld.GetServicenodeIndex(vinServicenode);
    }

    /// Get servicenode VIN for an old index value
    bool GetServicenodeVinForIndexOld(int nServicenodeIndex, CTxIn& vinServicenodeOut) {
        LOCK(cs);
        return indexServicenodesOld.Get(nServicenodeIndex, vinServicenodeOut);
    }

    /// Get index of a servicenode vin, returning rebuild flag
    int GetServicenodeIndex(const CTxIn& vinServicenode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexServicenodes.GetServicenodeIndex(vinServicenode);
    }

    void ClearOldServicenodeIndex() {
        LOCK(cs);
        indexServicenodesOld.Clear();
        fIndexRebuilt = false;
    }

    bool Has(const CTxIn& vin);

    servicenode_info_t GetServicenodeInfo(const CTxIn& vin);

    servicenode_info_t GetServicenodeInfo(const CPubKey& pubKeyServicenode);

    /// Find an entry in the servicenode list that is next to be paid
    CServicenode* GetNextServicenodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount);
    /// Same as above but use current block height
    CServicenode* GetNextServicenodeInQueueForPayment(bool fFilterSigTime, int& nCount);

    /// Find a random entry
    CServicenode* FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion = -1);

    std::vector<CServicenode> GetFullServicenodeVector() { return vServicenodes; }

    std::vector<std::pair<int, CServicenode> > GetServicenodeRanks(int nBlockHeight = -1, int nMinProtocol=0);
    int GetServicenodeRank(const CTxIn &vin, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);
    CServicenode* GetServicenodeByRank(int nRank, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);

    void ProcessServicenodeConnections();
    std::pair<CService, std::set<uint256> > PopScheduledMnbRequestConnection();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void DoFullVerificationStep();
    void CheckSameAddr();
    bool SendVerifyRequest(const CAddress& addr, const std::vector<CServicenode*>& vSortedByAddr);
    void SendVerifyReply(CNode* pnode, CServicenodeVerification& mnv);
    void ProcessVerifyReply(CNode* pnode, CServicenodeVerification& mnv);
    void ProcessVerifyBroadcast(CNode* pnode, const CServicenodeVerification& mnv);

    /// Return the number of (unique) Servicenodes
    int size() { return vServicenodes.size(); }

    std::string ToString() const;

    /// Update servicenode list and maps using provided CServicenodeBroadcast
    void UpdateServicenodeList(CServicenodeBroadcast mnb);
    /// Perform complete check and only then update list and maps
    bool CheckMnbAndUpdateServicenodeList(CNode* pfrom, CServicenodeBroadcast mnb, int& nDos);
    bool IsMnbRecoveryRequested(const uint256& hash) { return mMnbRecoveryRequests.count(hash); }

    void UpdateLastPaid();

    void CheckAndRebuildServicenodeIndex();

    void AddDirtyGovernanceObjectHash(const uint256& nHash)
    {
        LOCK(cs);
        vecDirtyGovernanceObjectHashes.push_back(nHash);
    }

    std::vector<uint256> GetAndClearDirtyGovernanceObjectHashes()
    {
        LOCK(cs);
        std::vector<uint256> vecTmp = vecDirtyGovernanceObjectHashes;
        vecDirtyGovernanceObjectHashes.clear();
        return vecTmp;;
    }

    bool IsWatchdogActive();
    void UpdateWatchdogVoteTime(const CTxIn& vin);
    bool AddGovernanceVote(const CTxIn& vin, uint256 nGovernanceObjectHash);
    void RemoveGovernanceObject(uint256 nGovernanceObjectHash);

    void CheckServicenode(const CTxIn& vin, bool fForce = false);
    void CheckServicenode(const CPubKey& pubKeyServicenode, bool fForce = false);

    int GetServicenodeState(const CTxIn& vin);
    int GetServicenodeState(const CPubKey& pubKeyServicenode);

    bool IsServicenodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt = -1);
    void SetServicenodeLastPing(const CTxIn& vin, const CServicenodePing& mnp);

    void UpdatedBlockTip(const CBlockIndex *pindex);

    /**
     * Called to notify CGovernanceManager that the servicenode index has been updated.
     * Must be called while not holding the CServicenodeMan::cs mutex
     */
    void NotifyServicenodeUpdates();

public:
    IMPLEMENT_SERIALIZE(
        LOCK(cs);
        std::string strVersion;
        if (fRead) {
            READWRITE(strVersion);
        }
        else {
            strVersion = SERIALIZATION_VERSION_STRING;
            READWRITE(strVersion);
        }

        READWRITE(vServicenodes);
        READWRITE(mAskedUsForServicenodeList);
        READWRITE(mWeAskedForServicenodeList);
        READWRITE(mWeAskedForServicenodeListEntry);
        READWRITE(mMnbRecoveryRequests);
        READWRITE(mMnbRecoveryGoodReplies);
        READWRITE(nLastWatchdogVoteTime);
        READWRITE(nDsqCount);

        READWRITE(mapSeenServicenodeBroadcast);
        READWRITE(mapSeenServicenodePing);
        READWRITE(indexServicenodes);
        if (fRead && (strVersion != SERIALIZATION_VERSION_STRING)) {
            const_cast<CServicenodeMan *>(this)->Clear();
        }
    )
};

#endif
