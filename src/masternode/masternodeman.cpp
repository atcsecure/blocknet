// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activemasternode.h"
#include "addrman.h"
// #include "darksend.h"
// #include "governance.h"
#include "masternode-payments.h"
#include "masternode-sync.h"
#include "masternodeman.h"
#include "netfulfilledman.h"
#include "util.h"

/** Masternode manager */
CMasternodeMan mnodeman;

const std::string CMasternodeMan::SERIALIZATION_VERSION_STRING = "CMasternodeMan-Version-4";

struct CompareLastPaidBlock
{
    bool operator()(const std::pair<int, CMasternode*>& t1,
                    const std::pair<int, CMasternode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

struct CompareScoreMN
{
    bool operator()(const std::pair<int64_t, CMasternode*>& t1,
                    const std::pair<int64_t, CMasternode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

CMasternodeIndex::CMasternodeIndex()
    : nSize(0),
      mapIndex(),
      mapReverseIndex()
{}

bool CMasternodeIndex::Get(int nIndex, CTxIn& vinMasternode) const
{
    rindex_m_cit it = mapReverseIndex.find(nIndex);
    if(it == mapReverseIndex.end()) {
        return false;
    }
    vinMasternode = it->second;
    return true;
}

int CMasternodeIndex::GetMasternodeIndex(const CTxIn& vinMasternode) const
{
    index_m_cit it = mapIndex.find(vinMasternode);
    if(it == mapIndex.end()) {
        return -1;
    }
    return it->second;
}

void CMasternodeIndex::AddMasternodeVIN(const CTxIn& vinMasternode)
{
    index_m_it it = mapIndex.find(vinMasternode);
    if(it != mapIndex.end()) {
        return;
    }
    int nNextIndex = nSize;
    mapIndex[vinMasternode] = nNextIndex;
    mapReverseIndex[nNextIndex] = vinMasternode;
    ++nSize;
}

void CMasternodeIndex::Clear()
{
    mapIndex.clear();
    mapReverseIndex.clear();
    nSize = 0;
}
struct CompareByAddr

{
    bool operator()(const CMasternode* t1,
                    const CMasternode* t2) const
    {
        return t1->addr < t2->addr;
    }
};

void CMasternodeIndex::RebuildIndex()
{
    nSize = mapIndex.size();
    for(index_m_it it = mapIndex.begin(); it != mapIndex.end(); ++it) {
        mapReverseIndex[it->second] = it->first;
    }
}

CMasternodeMan::CMasternodeMan()
: cs(),
  vMasternodes(),
  mAskedUsForMasternodeList(),
  mWeAskedForMasternodeList(),
  mWeAskedForMasternodeListEntry(),
  mWeAskedForVerification(),
  mMnbRecoveryRequests(),
  mMnbRecoveryGoodReplies(),
  listScheduledMnbRequestConnections(),
  nLastIndexRebuildTime(0),
  indexMasternodes(),
  indexMasternodesOld(),
  fIndexRebuilt(false),
  fMasternodesAdded(false),
  fMasternodesRemoved(false),
  vecDirtyGovernanceObjectHashes(),
  nLastWatchdogVoteTime(0),
  mapSeenMasternodeBroadcast(),
  mapSeenMasternodePing(),
  nDsqCount(0)
{}

bool CMasternodeMan::Add(CMasternode &mn)
{
    LOCK(cs);

    CMasternode *pmn = Find(mn.vin);
    if (pmn == NULL) {
        printf("CMasternodeMan::Add -- Adding new Masternode: addr=%s, %i now\n", mn.addr.ToString().c_str(), size() + 1);
        vMasternodes.push_back(mn);
        indexMasternodes.AddMasternodeVIN(mn.vin);
        fMasternodesAdded = true;
        return true;
    }

    return false;
}

void CMasternodeMan::AskForMN(CNode* pnode, const CTxIn &vin)
{
    if(!pnode) return;

    LOCK(cs);

    std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it1 = mWeAskedForMasternodeListEntry.find(vin.prevout);
    if (it1 != mWeAskedForMasternodeListEntry.end()) {
        std::map<CNetAddr, int64_t>::iterator it2 = it1->second.find(pnode->addr);
        if (it2 != it1->second.end()) {
            if (GetTime() < it2->second) {
                // we've asked recently, should not repeat too often or we could get banned
                return;
            }
            // we asked this node for this outpoint but it's ok to ask again already
            printf("CMasternodeMan::AskForMN -- Asking same peer %s for missing masternode entry again: %s\n", pnode->addr.ToString().c_str(), vin.prevout.ToString().c_str());
        } else {
            // we already asked for this outpoint but not this node
            printf("CMasternodeMan::AskForMN -- Asking new peer %s for missing masternode entry: %s\n", pnode->addr.ToString().c_str(), vin.prevout.ToString().c_str());
        }
    } else {
        // we never asked any node for this outpoint
        printf("CMasternodeMan::AskForMN -- Asking peer %s for missing masternode entry for the first time: %s\n", pnode->addr.ToString().c_str(), vin.prevout.ToString().c_str());
    }
    mWeAskedForMasternodeListEntry[vin.prevout][pnode->addr] = GetTime() + DSEG_UPDATE_SECONDS;

    pnode->PushMessage(NetMsgType::DSEG, vin);
}

void CMasternodeMan::Check()
{
    LOCK(cs);

    printf("CMasternodeMan::Check -- nLastWatchdogVoteTime=%d, IsWatchdogActive()=%d\n", nLastWatchdogVoteTime, IsWatchdogActive());

    for (CMasternode & mn : vMasternodes) {
        mn.Check();
    }
}

void CMasternodeMan::CheckAndRemove()
{
    if(!masternodeSync.IsMasternodeListSynced())
    {
        return;
    }

    printf("CMasternodeMan::CheckAndRemove\n");

    {
        // Need LOCK2 here to ensure consistent locking order because code below locks cs_main
        // in CheckMnbAndUpdateMasternodeList()
        LOCK2(cs_main, cs);

        Check();

        // Remove spent masternodes, prepare structures and make requests to reasure the state of inactive ones
        std::vector<CMasternode>::iterator it = vMasternodes.begin();
        std::vector<std::pair<int, CMasternode> > vecMasternodeRanks;
        // ask for up to MNB_RECOVERY_MAX_ASK_ENTRIES masternode entries at a time
        int nAskForMnbRecovery = MNB_RECOVERY_MAX_ASK_ENTRIES;
        while(it != vMasternodes.end())
        {
            CMasternodeBroadcast mnb = CMasternodeBroadcast(*it);
            uint256 hash = mnb.GetHash();
            // If collateral was spent ...
            if ((*it).IsOutpointSpent())
            {
                printf("CMasternodeMan::CheckAndRemove -- Removing Masternode: %s  addr=%s  %i now\n", (*it).GetStateString().c_str(), (*it).addr.ToString().c_str(), size() - 1);

                // erase all of the broadcasts we've seen from this txin, ...
                mapSeenMasternodeBroadcast.erase(hash);
                mWeAskedForMasternodeListEntry.erase((*it).vin.prevout);

                // and finally remove it from the list
                it->FlagGovernanceItemsAsDirty();
                it = vMasternodes.erase(it);
                fMasternodesRemoved = true;
            }
            else
            {
                bool fAsk = pCurrentBlockIndex &&
                            (nAskForMnbRecovery > 0) &&
                            masternodeSync.IsSynced() &&
                            it->IsNewStartRequired() &&
                            !IsMnbRecoveryRequested(hash);
                if(fAsk)
                {
                    // this mn is in a non-recoverable state and we haven't asked other nodes yet
                    std::set<CNetAddr> setRequested;
                    // calulate only once and only when it's needed
                    if(vecMasternodeRanks.empty())
                    {
                        int nRandomBlockHeight = GetRandInt(pCurrentBlockIndex->nHeight);
                        vecMasternodeRanks = GetMasternodeRanks(nRandomBlockHeight);
                    }
                    bool fAskedForMnbRecovery = false;
                    // ask first MNB_RECOVERY_QUORUM_TOTAL masternodes we can connect to and we haven't asked recently
                    for(int i = 0; setRequested.size() < MNB_RECOVERY_QUORUM_TOTAL && i < (int)vecMasternodeRanks.size(); i++)
                    {
                        // avoid banning
                        if(mWeAskedForMasternodeListEntry.count(it->vin.prevout) && mWeAskedForMasternodeListEntry[it->vin.prevout].count(vecMasternodeRanks[i].second.addr))
                        {
                            continue;
                        }
                        // didn't ask recently, ok to ask now
                        CService addr = vecMasternodeRanks[i].second.addr;
                        setRequested.insert(addr);
                        listScheduledMnbRequestConnections.push_back(std::make_pair(addr, hash));
                        fAskedForMnbRecovery = true;
                    }
                    if(fAskedForMnbRecovery)
                    {
                        printf("CMasternodeMan::CheckAndRemove -- Recovery initiated, masternode=%s\n", it->vin.prevout.ToString().c_str());
                        nAskForMnbRecovery--;
                    }
                    // wait for mnb recovery replies for MNB_RECOVERY_WAIT_SECONDS seconds
                    mMnbRecoveryRequests[hash] = std::make_pair(GetTime() + MNB_RECOVERY_WAIT_SECONDS, setRequested);
                }
                ++it;
            }
        }

        // proces replies for MASTERNODE_NEW_START_REQUIRED masternodes
        printf("CMasternodeMan::CheckAndRemove -- mMnbRecoveryGoodReplies size=%d\n", (int)mMnbRecoveryGoodReplies.size());
        std::map<uint256, std::vector<CMasternodeBroadcast> >::iterator itMnbReplies = mMnbRecoveryGoodReplies.begin();
        while(itMnbReplies != mMnbRecoveryGoodReplies.end())
        {
            if(mMnbRecoveryRequests[itMnbReplies->first].first < GetTime())
            {
                // all nodes we asked should have replied now
                if(itMnbReplies->second.size() >= MNB_RECOVERY_QUORUM_REQUIRED)
                {
                    // majority of nodes we asked agrees that this mn doesn't require new mnb, reprocess one of new mnbs
                    printf("CMasternodeMan::CheckAndRemove -- reprocessing mnb, masternode=%s\n", itMnbReplies->second[0].vin.prevout.ToString().c_str());
                    // mapSeenMasternodeBroadcast.erase(itMnbReplies->first);
                    int nDos;
                    itMnbReplies->second[0].fRecovery = true;
                    CheckMnbAndUpdateMasternodeList(NULL, itMnbReplies->second[0], nDos);
                }
                printf("CMasternodeMan::CheckAndRemove -- removing mnb recovery reply, masternode=%s, size=%d\n", itMnbReplies->second[0].vin.prevout.ToString().c_str(), (int)itMnbReplies->second.size());
                mMnbRecoveryGoodReplies.erase(itMnbReplies++);
            }
            else
            {
                ++itMnbReplies;
            }
        }
    }

    {
        // no need for cm_main below
        LOCK(cs);

        std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > >::iterator itMnbRequest = mMnbRecoveryRequests.begin();
        while(itMnbRequest != mMnbRecoveryRequests.end())
        {
            // Allow this mnb to be re-verified again after MNB_RECOVERY_RETRY_SECONDS seconds
            // if mn is still in MASTERNODE_NEW_START_REQUIRED state.
            if(GetTime() - itMnbRequest->second.first > MNB_RECOVERY_RETRY_SECONDS)
            {
                mMnbRecoveryRequests.erase(itMnbRequest++);
            }
            else
            {
                ++itMnbRequest;
            }
        }

        // check who's asked for the Masternode list
        std::map<CNetAddr, int64_t>::iterator it1 = mAskedUsForMasternodeList.begin();
        while(it1 != mAskedUsForMasternodeList.end())
        {
            if((*it1).second < GetTime())
            {
                mAskedUsForMasternodeList.erase(it1++);
            }
            else
            {
                ++it1;
            }
        }

        // check who we asked for the Masternode list
        it1 = mWeAskedForMasternodeList.begin();
        while(it1 != mWeAskedForMasternodeList.end())
        {
            if((*it1).second < GetTime())
            {
                mWeAskedForMasternodeList.erase(it1++);
            }
            else
            {
                ++it1;
            }
        }

        // check which Masternodes we've asked for
        std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it2 = mWeAskedForMasternodeListEntry.begin();
        while(it2 != mWeAskedForMasternodeListEntry.end())
        {
            std::map<CNetAddr, int64_t>::iterator it3 = it2->second.begin();
            while(it3 != it2->second.end())
            {
                if(it3->second < GetTime())
                {
                    it2->second.erase(it3++);
                }
                else
                {
                    ++it3;
                }
            }
            if(it2->second.empty())
            {
                mWeAskedForMasternodeListEntry.erase(it2++);
            }
            else
            {
                ++it2;
            }
        }

        std::map<CNetAddr, CMasternodeVerification>::iterator it3 = mWeAskedForVerification.begin();
        while(it3 != mWeAskedForVerification.end())
        {
            if(it3->second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS)
            {
                mWeAskedForVerification.erase(it3++);
            }
            else
            {
                ++it3;
            }
        }

        // NOTE: do not expire mapSeenMasternodeBroadcast entries here, clean them on mnb updates!

        // remove expired mapSeenMasternodePing
        std::map<uint256, CMasternodePing>::iterator it4 = mapSeenMasternodePing.begin();
        while(it4 != mapSeenMasternodePing.end())
        {
            if((*it4).second.IsExpired())
            {
                printf("CMasternodeMan::CheckAndRemove -- Removing expired Masternode ping: hash=%s\n", (*it4).second.GetHash().ToString().c_str());
                mapSeenMasternodePing.erase(it4++);
            }
            else
            {
                ++it4;
            }
        }

        // remove expired mapSeenMasternodeVerification
        std::map<uint256, CMasternodeVerification>::iterator itv2 = mapSeenMasternodeVerification.begin();
        while(itv2 != mapSeenMasternodeVerification.end())
        {
            if((*itv2).second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS)
            {
                printf("CMasternodeMan::CheckAndRemove -- Removing expired Masternode verification: hash=%s\n", (*itv2).first.ToString().c_str());
                mapSeenMasternodeVerification.erase(itv2++);
            }
            else
            {
                ++itv2;
            }
        }

        printf("CMasternodeMan::CheckAndRemove -- %s\n", ToString().c_str());

        if(fMasternodesRemoved)
        {
            CheckAndRebuildMasternodeIndex();
        }
    }

    if(fMasternodesRemoved)
    {
        NotifyMasternodeUpdates();
    }
}

void CMasternodeMan::Clear()
{
    LOCK(cs);
    vMasternodes.clear();
    mAskedUsForMasternodeList.clear();
    mWeAskedForMasternodeList.clear();
    mWeAskedForMasternodeListEntry.clear();
    mapSeenMasternodeBroadcast.clear();
    mapSeenMasternodePing.clear();
    nDsqCount = 0;
    nLastWatchdogVoteTime = 0;
    indexMasternodes.Clear();
    indexMasternodesOld.Clear();
}

int CMasternodeMan::CountMasternodes(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinMasternodePaymentsProto() : nProtocolVersion;

    for (CMasternode & mn : vMasternodes)
    {
        if(mn.nProtocolVersion < nProtocolVersion) continue;
        nCount++;
    }

    return nCount;
}

int CMasternodeMan::CountEnabled(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinMasternodePaymentsProto() : nProtocolVersion;

    for (CMasternode & mn : vMasternodes)
    {
        if(mn.nProtocolVersion < nProtocolVersion || !mn.IsEnabled()) continue;
        nCount++;
    }

    return nCount;
}

/* Only IPv4 masternodes are allowed in 12.1, saving this for later
int CMasternodeMan::CountByIP(int nNetworkType)
{
    LOCK(cs);
    int nNodeCount = 0;

    BOOST_FOREACH(CMasternode& mn, vMasternodes)
        if ((nNetworkType == NET_IPV4 && mn.addr.IsIPv4()) ||
            (nNetworkType == NET_TOR  && mn.addr.IsTor())  ||
            (nNetworkType == NET_IPV6 && mn.addr.IsIPv6())) {
                nNodeCount++;
        }

    return nNodeCount;
}
*/

void CMasternodeMan::DsegUpdate(CNode* pnode)
{
    LOCK(cs);

    if (!fTestNet)
    {
        if (!(pnode->addr.IsRFC1918() || pnode->addr.IsLocal()))
        {
            std::map<CNetAddr, int64_t>::iterator it = mWeAskedForMasternodeList.find(pnode->addr);
            if (it != mWeAskedForMasternodeList.end() && GetTime() < (*it).second)
            {
                printf("CMasternodeMan::DsegUpdate -- we already asked %s for the list; skipping...\n", pnode->addr.ToString().c_str());
                return;
            }
        }
    }
    
    pnode->PushMessage(NetMsgType::DSEG, CTxIn());
    int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
    mWeAskedForMasternodeList[pnode->addr] = askAgain;

    printf("CMasternodeMan::DsegUpdate -- asked %s for the list\n", pnode->addr.ToString().c_str());
}

CMasternode* CMasternodeMan::Find(const CScript &payee)
{
    LOCK(cs);

    for (CMasternode & mn : vMasternodes)
    {
        CScript tmp;
        tmp.SetDestination(mn.pubKeyCollateralAddress.GetID());
        if (tmp == payee)
        {
            return &mn;
        }
    }
    return NULL;
}

CMasternode* CMasternodeMan::Find(const CTxIn &vin)
{
    LOCK(cs);

    for (CMasternode & mn : vMasternodes)
    {
        if(mn.vin.prevout == vin.prevout)
        {
            return &mn;
        }
    }
    return NULL;
}

CMasternode* CMasternodeMan::Find(const CPubKey &pubKeyMasternode)
{
    LOCK(cs);

    for (CMasternode & mn : vMasternodes)
    {
        if(mn.pubKeyMasternode == pubKeyMasternode)
        {
            return &mn;
        }
    }
    return NULL;
}

bool CMasternodeMan::Get(const CPubKey& pubKeyMasternode, CMasternode& masternode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CMasternode* pMN = Find(pubKeyMasternode);
    if(!pMN)  {
        return false;
    }
    masternode = *pMN;
    return true;
}

bool CMasternodeMan::Get(const CTxIn& vin, CMasternode& masternode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CMasternode* pMN = Find(vin);
    if(!pMN)  {
        return false;
    }
    masternode = *pMN;
    return true;
}

masternode_info_t CMasternodeMan::GetMasternodeInfo(const CTxIn& vin)
{
    masternode_info_t info;
    LOCK(cs);
    CMasternode* pMN = Find(vin);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

masternode_info_t CMasternodeMan::GetMasternodeInfo(const CPubKey& pubKeyMasternode)
{
    masternode_info_t info;
    LOCK(cs);
    CMasternode* pMN = Find(pubKeyMasternode);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

bool CMasternodeMan::Has(const CTxIn& vin)
{
    LOCK(cs);
    CMasternode* pMN = Find(vin);
    return (pMN != NULL);
}

//
// Deterministically select the oldest/best masternode to pay on the network
//
CMasternode* CMasternodeMan::GetNextMasternodeInQueueForPayment(bool fFilterSigTime, int& nCount)
{
    if(!pCurrentBlockIndex) {
        nCount = 0;
        return NULL;
    }
    return GetNextMasternodeInQueueForPayment(pCurrentBlockIndex->nHeight, fFilterSigTime, nCount);
}

CMasternode* CMasternodeMan::GetNextMasternodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount)
{
    // Need LOCK2 here to ensure consistent locking order because the GetBlockHash call below locks cs_main
    LOCK2(cs_main,cs);

    CMasternode *pBestMasternode = NULL;
    std::vector<std::pair<int, CMasternode*> > vecMasternodeLastPaid;

    /*
        Make a vector with all of the last paid times
    */

    int nMnCount = CountEnabled();
    for (CMasternode & mn : vMasternodes)
    {
        if(!mn.IsValidForPayment())
        {
            continue;
        }

        // //check protocol version
        if(mn.nProtocolVersion < mnpayments.GetMinMasternodePaymentsProto())
        {
            continue;
        }

        //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
        if(mnpayments.IsScheduled(mn, nBlockHeight))
        {
            continue;
        }

        //it's too new, wait for a cycle
        if(fFilterSigTime && mn.sigTime + (nMnCount*2.6*60) > GetAdjustedTime())
        {
            continue;
        }

        //make sure it has at least as many confirmations as there are masternodes
        if(mn.GetCollateralAge() < nMnCount)
        {
            continue;
        }

        vecMasternodeLastPaid.push_back(std::make_pair(mn.GetLastPaidBlock(), &mn));
    }

    nCount = (int)vecMasternodeLastPaid.size();

    //when the network is in the process of upgrading, don't penalize nodes that recently restarted
    if(fFilterSigTime && nCount < nMnCount/3)
    {
        return GetNextMasternodeInQueueForPayment(nBlockHeight, false, nCount);
    }

    // Sort them low to high
    sort(vecMasternodeLastPaid.begin(), vecMasternodeLastPaid.end(), CompareLastPaidBlock());

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight - 101))
    {
        printf("CMasternode::GetNextMasternodeInQueueForPayment -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight - 101);
        return NULL;
    }

    // Look at 1/10 of the oldest nodes (by last payment), calculate their scores and pay the best one
    //  -- This doesn't look at who is being paid in the +8-10 blocks, allowing for double payments very rarely
    //  -- 1/100 payments should be a double payment on mainnet - (1/(3000/10))*2
    //  -- (chance per block * chances before IsScheduled will fire)
    int nTenthNetwork = nMnCount/10;
    int nCountTenth = 0;
    arith_uint256 nHighest = 0;
    for (PAIRTYPE(int, CMasternode*)& s : vecMasternodeLastPaid)
    {
        arith_uint256 nScore = s.second->CalculateScore(blockHash);
        if(nScore > nHighest)
        {
            nHighest = nScore;
            pBestMasternode = s.second;
        }
        nCountTenth++;
        if(nCountTenth >= nTenthNetwork)
        {
            break;
        }
    }
    return pBestMasternode;
}

CMasternode* CMasternodeMan::FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion)
{
    LOCK(cs);

    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinMasternodePaymentsProto() : nProtocolVersion;

    int nCountEnabled = CountEnabled(nProtocolVersion);
    int nCountNotExcluded = nCountEnabled - vecToExclude.size();

    printf("CMasternodeMan::FindRandomNotInVec -- %d enabled masternodes, %d masternodes to choose from\n", nCountEnabled, nCountNotExcluded);
    if(nCountNotExcluded < 1)
    {
        return nullptr;
    }

    // fill a vector of pointers
    std::vector<CMasternode*> vpMasternodesShuffled;
    for (CMasternode & mn : vMasternodes)
    {
        vpMasternodesShuffled.push_back(&mn);
    }

    InsecureRand insecureRand;
    // shuffle pointers
    std::random_shuffle(vpMasternodesShuffled.begin(), vpMasternodesShuffled.end(), insecureRand);
    bool fExclude;

    // loop through
    for (CMasternode * pmn : vpMasternodesShuffled)
    {
        if(pmn->nProtocolVersion < nProtocolVersion || !pmn->IsEnabled())
        {
            continue;
        }
        fExclude = false;
        for (const CTxIn & txinToExclude : vecToExclude)
        {
            if(pmn->vin.prevout == txinToExclude.prevout)
            {
                fExclude = true;
                break;
            }
        }
        if(fExclude)
        {
            continue;
        }
        // found the one not in vecToExclude
        printf("CMasternodeMan::FindRandomNotInVec -- found, masternode=%s\n", pmn->vin.prevout.ToString().c_str());
        return pmn;
    }

    printf("CMasternodeMan::FindRandomNotInVec -- failed\n");
    return nullptr;
}

int CMasternodeMan::GetMasternodeRank(const CTxIn& vin, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CMasternode*> > vecMasternodeScores;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight))
    {
        return -1;
    }

    LOCK(cs);

    // scan for winner
    for (CMasternode & mn : vMasternodes)
    {
        if(mn.nProtocolVersion < nMinProtocol)
        {
            continue;
        }
        if(fOnlyActive)
        {
            if(!mn.IsEnabled())
            {
                continue;
            }
        }
        else
        {
            if(!mn.IsValidForPayment())
            {
                continue;
            }
        }
        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecMasternodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecMasternodeScores.rbegin(), vecMasternodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    for (PAIRTYPE(int64_t, CMasternode*) & scorePair : vecMasternodeScores)
    {
        nRank++;
        if(scorePair.second->vin.prevout == vin.prevout)
        {
            return nRank;
        }
    }

    return -1;
}

std::vector<std::pair<int, CMasternode> > CMasternodeMan::GetMasternodeRanks(int nBlockHeight, int nMinProtocol)
{
    std::vector<std::pair<int64_t, CMasternode*> > vecMasternodeScores;
    std::vector<std::pair<int, CMasternode> > vecMasternodeRanks;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight))
    {
        return vecMasternodeRanks;
    }

    LOCK(cs);

    // scan for winner
    for (CMasternode & mn : vMasternodes)
    {
        if(mn.nProtocolVersion < nMinProtocol || !mn.IsEnabled())
        {
            continue;
        }

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecMasternodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecMasternodeScores.rbegin(), vecMasternodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    for (PAIRTYPE(int64_t, CMasternode*) & s : vecMasternodeScores)
    {
        nRank++;
        vecMasternodeRanks.push_back(std::make_pair(nRank, *s.second));
    }

    return vecMasternodeRanks;
}

CMasternode* CMasternodeMan::GetMasternodeByRank(int nRank, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CMasternode*> > vecMasternodeScores;

    LOCK(cs);

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight))
    {
        printf("CMasternode::GetMasternodeByRank -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight);
        return nullptr;
    }

    // Fill scores
    for (CMasternode & mn : vMasternodes)
    {
        if(mn.nProtocolVersion < nMinProtocol)
        {
            continue;
        }

        if(fOnlyActive && !mn.IsEnabled())
        {
            continue;
        }

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecMasternodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecMasternodeScores.rbegin(), vecMasternodeScores.rend(), CompareScoreMN());

    int rank = 0;
    for (PAIRTYPE(int64_t, CMasternode*) & s : vecMasternodeScores)
    {
        rank++;
        if(rank == nRank)
        {
            return s.second;
        }
    }

    return nullptr;
}

void CMasternodeMan::ProcessMasternodeConnections()
{
    //we don't care about this for regtest
//    if(Params().NetworkIDString() == CBaseChainParams::REGTEST) return;

    LOCK(cs_vNodes);
    for (CNode * pnode : vNodes)
    {
        if(pnode->fMasternode)
        {
//            if(darkSendPool.pSubmittedToMasternode != NULL && pnode->addr == darkSendPool.pSubmittedToMasternode->addr)
//            {
//                continue;
//            }
            printf("Closing Masternode connection: addr=%s\n", pnode->addr.ToString().c_str());
            pnode->fDisconnect = true;
        }
    }
}

std::pair<CService, std::set<uint256> > CMasternodeMan::PopScheduledMnbRequestConnection()
{
    LOCK(cs);
    if(listScheduledMnbRequestConnections.empty()) {
        return std::make_pair(CService(), std::set<uint256>());
    }

    std::set<uint256> setResult;

    listScheduledMnbRequestConnections.sort();
    std::pair<CService, uint256> pairFront = listScheduledMnbRequestConnections.front();

    // squash hashes from requests with the same CService as the first one into setResult
    std::list< std::pair<CService, uint256> >::iterator it = listScheduledMnbRequestConnections.begin();
    while(it != listScheduledMnbRequestConnections.end()) {
        if(pairFront.first == it->first) {
            setResult.insert(it->second);
            it = listScheduledMnbRequestConnections.erase(it);
        } else {
            // since list is sorted now, we can be sure that there is no more hashes left
            // to ask for from this addr
            break;
        }
    }
    return std::make_pair(pairFront.first, setResult);
}


void CMasternodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if(!masternodeSync.IsBlockchainSynced())
    {
        return;
    }

    if (strCommand == NetMsgType::MNANNOUNCE)
    {
        //Masternode Broadcast

        CMasternodeBroadcast mnb;
        vRecv >> mnb;

        // pfrom->setAskFor.erase(mnb.GetHash());

        printf("MNANNOUNCE -- Masternode announce, masternode=%s\n", mnb.vin.prevout.ToString().c_str());

        int nDos = 0;

        if (CheckMnbAndUpdateMasternodeList(pfrom, mnb, nDos))
        {
            // use announced Masternode as a peer
            addrman.Add(CAddress(mnb.addr), pfrom->addr, 2*60*60);
        }
        else if(nDos > 0)
        {
            pfrom->Misbehaving(nDos);
        }

        if(fMasternodesAdded)
        {
            NotifyMasternodeUpdates();
        }
    }

    else if (strCommand == NetMsgType::MNPING)
    {
        //Masternode Ping

        CMasternodePing mnp;
        vRecv >> mnp;

        uint256 nHash = mnp.GetHash();

        // pfrom->setAskFor.erase(nHash);

        printf("MNPING -- Masternode ping, masternode=%s\n", mnp.vin.prevout.ToString().c_str());

        // Need LOCK2 here to ensure consistent locking order because the CheckAndUpdate call below locks cs_main
        LOCK2(cs_main, cs);

        if(mapSeenMasternodePing.count(nHash))
        {
            return; //seen
        }

        mapSeenMasternodePing.insert(std::make_pair(nHash, mnp));

        printf("MNPING -- Masternode ping, masternode=%s new\n", mnp.vin.prevout.ToString().c_str());

        // see if we have this Masternode
        CMasternode* pmn = mnodeman.Find(mnp.vin);

        // too late, new MNANNOUNCE is required
        if(pmn && pmn->IsNewStartRequired())
        {
            return;
        }

        int nDos = 0;
        if(mnp.CheckAndUpdate(pmn, false, nDos))
        {
            return;
        }

        if(nDos > 0)
        {
            // if anything significant failed, mark that node
            pfrom->Misbehaving(nDos);
        }
        else if(pmn != NULL)
        {
            // nothing significant failed, mn is a known one too
            return;
        }

        // something significant is broken or mn is unknown,
        // we might have to ask for a masternode entry once
        AskForMN(pfrom, mnp.vin);

    }

    else if (strCommand == NetMsgType::DSEG)
    {
        // Get Masternode list or specific entry

        // Ignore such requests until we are fully synced.
        // We could start processing this after masternode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!masternodeSync.IsSynced())
        {
            return;
        }

        CTxIn vin;
        vRecv >> vin;

        printf("DSEG -- Masternode list, masternode=%s\n", vin.prevout.ToString().c_str());

        LOCK(cs);

        if(vin == CTxIn())
        {
            //only should ask for this once

            //local network
            bool isLocal = (pfrom->addr.IsRFC1918() || pfrom->addr.IsLocal());

            if(!isLocal && !fTestNet)
            {
                std::map<CNetAddr, int64_t>::iterator i = mAskedUsForMasternodeList.find(pfrom->addr);
                if (i != mAskedUsForMasternodeList.end())
                {
                    int64_t t = (*i).second;
                    if (GetTime() < t)
                    {
                        pfrom->Misbehaving(34);
                        printf("DSEG -- peer already asked me for the list, peer=%s\n", pfrom->addr.ToString().c_str());
                        return;
                    }
                }
                int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
                mAskedUsForMasternodeList[pfrom->addr] = askAgain;
            }
        } //else, asking for a specific node which is ok

        int nInvCount = 0;

        for (CMasternode & mn : vMasternodes)
        {
            if (vin != CTxIn() && vin != mn.vin)
            {
                continue; // asked for specific vin but we are not there yet
            }

            if (mn.addr.IsRFC1918() || mn.addr.IsLocal())
            {
                continue; // do not send local network masternode
            }

            if (mn.IsUpdateRequired())
            {
                continue; // do not send outdated masternodes
            }

            printf("DSEG -- Sending Masternode entry: masternode=%s  addr=%s\n", mn.vin.prevout.ToString().c_str(), mn.addr.ToString().c_str());

            CMasternodeBroadcast mnb = CMasternodeBroadcast(mn);
            uint256 hash = mnb.GetHash();
            pfrom->PushInventory(CInv(MSG_MASTERNODE_ANNOUNCE, hash));
            pfrom->PushInventory(CInv(MSG_MASTERNODE_PING, mn.lastPing.GetHash()));
            nInvCount++;

            if (!mapSeenMasternodeBroadcast.count(hash))
            {
                mapSeenMasternodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));
            }

            if (vin == mn.vin)
            {
                printf("DSEG -- Sent 1 Masternode inv to peer %s\n", pfrom->addr.ToString().c_str());
                return;
            }
        }

        if(vin == CTxIn())
        {
            pfrom->PushMessage(NetMsgType::SYNCSTATUSCOUNT, MASTERNODE_SYNC_LIST, nInvCount);
            printf("DSEG -- Sent %d Masternode invs to peer %s\n", nInvCount, pfrom->addr.ToString().c_str());
            return;
        }

        // smth weird happen - someone asked us for vin we have no idea about?
        printf("DSEG -- No invs sent to peer %s\n", pfrom->addr.ToString().c_str());

    }

    else if (strCommand == NetMsgType::MNVERIFY)
    {
        // Masternode Verify

        // Need LOCK2 here to ensure consistent locking order because the all functions below call GetBlockHash which locks cs_main
        LOCK2(cs_main, cs);

        CMasternodeVerification mnv;
        vRecv >> mnv;

        if(mnv.vchSig1.empty())
        {
            // CASE 1: someone asked me to verify myself /IP we are using/
            SendVerifyReply(pfrom, mnv);
        }
        else if (mnv.vchSig2.empty())
        {
            // CASE 2: we _probably_ got verification we requested from some masternode
            ProcessVerifyReply(pfrom, mnv);
        }
        else
        {
            // CASE 3: we _probably_ got verification broadcast signed by some masternode which verified another one
            ProcessVerifyBroadcast(pfrom, mnv);
        }
    }
}

// Verification of masternodes via unique direct requests.

void CMasternodeMan::DoFullVerificationStep()
{
    if(activeMasternode.vin == CTxIn())
    {
        return;
    }

    if(!masternodeSync.IsSynced())
    {
        return;
    }

    std::vector<std::pair<int, CMasternode> > vecMasternodeRanks = GetMasternodeRanks(pCurrentBlockIndex->nHeight - 1, MIN_POSE_PROTO_VERSION);

    // Need LOCK2 here to ensure consistent locking order because the SendVerifyRequest call below locks cs_main
    // through GetHeight() signal in ConnectNode
    LOCK2(cs_main, cs);

    int nCount = 0;

    int nMyRank = -1;
    int nRanksTotal = (int)vecMasternodeRanks.size();

    // send verify requests only if we are in top MAX_POSE_RANK
    std::vector<std::pair<int, CMasternode> >::iterator it = vecMasternodeRanks.begin();
    while(it != vecMasternodeRanks.end())
    {
        if(it->first > MAX_POSE_RANK)
        {
            printf("CMasternodeMan::DoFullVerificationStep -- Must be in top %d to send verify request\n",
                        (int)MAX_POSE_RANK);
            return;
        }

        if(it->second.vin == activeMasternode.vin)
        {
            nMyRank = it->first;
            printf("masternode", "CMasternodeMan::DoFullVerificationStep -- Found self at rank %d/%d, verifying up to %d masternodes\n",
                        nMyRank, nRanksTotal, (int)MAX_POSE_CONNECTIONS);
            break;
        }
        ++it;
    }

    // edge case: list is too short and this masternode is not enabled
    if(nMyRank == -1)
    {
        return;
    }

    // send verify requests to up to MAX_POSE_CONNECTIONS masternodes
    // starting from MAX_POSE_RANK + nMyRank and using MAX_POSE_CONNECTIONS as a step
    int nOffset = MAX_POSE_RANK + nMyRank - 1;
    if(nOffset >= (int)vecMasternodeRanks.size())
    {
        return;
    }

    std::vector<CMasternode*> vSortedByAddr;
    for (CMasternode & mn : vMasternodes)
    {
        vSortedByAddr.push_back(&mn);
    }

    sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

    it = vecMasternodeRanks.begin() + nOffset;
    while(it != vecMasternodeRanks.end())
    {
        if(it->second.IsPoSeVerified() || it->second.IsPoSeBanned())
        {
            printf("CMasternodeMan::DoFullVerificationStep -- Already %s%s%s masternode %s address %s, skipping...\n",
                        it->second.IsPoSeVerified() ? "verified" : "",
                        it->second.IsPoSeVerified() && it->second.IsPoSeBanned() ? " and " : "",
                        it->second.IsPoSeBanned() ? "banned" : "",
                        it->second.vin.prevout.ToString().c_str(), it->second.addr.ToString().c_str());
            nOffset += MAX_POSE_CONNECTIONS;
            if(nOffset >= (int)vecMasternodeRanks.size())
            {
                break;
            }
            it += MAX_POSE_CONNECTIONS;
            continue;
        }
        printf("CMasternodeMan::DoFullVerificationStep -- Verifying masternode %s rank %d/%d address %s\n",
                    it->second.vin.prevout.ToString().c_str(), it->first, nRanksTotal, it->second.addr.ToString().c_str());
        if(SendVerifyRequest((CAddress)it->second.addr, vSortedByAddr))
        {
            nCount++;
            if(nCount >= MAX_POSE_CONNECTIONS)
            {
                break;
            }
        }
        nOffset += MAX_POSE_CONNECTIONS;
        if(nOffset >= (int)vecMasternodeRanks.size())
        {
            break;
        }
        it += MAX_POSE_CONNECTIONS;
    }

    printf("CMasternodeMan::DoFullVerificationStep -- Sent verification requests to %d masternodes\n", nCount);
}

// This function tries to find masternodes with the same addr,
// find a verified one and ban all the other. If there are many nodes
// with the same addr but none of them is verified yet, then none of them are banned.
// It could take many times to run this before most of the duplicate nodes are banned.

void CMasternodeMan::CheckSameAddr()
{
    if(!masternodeSync.IsSynced() || vMasternodes.empty())
    {
        return;
    }

    std::vector<CMasternode*> vBan;
    std::vector<CMasternode*> vSortedByAddr;

    {
        LOCK(cs);

        CMasternode* pprevMasternode = NULL;
        CMasternode* pverifiedMasternode = NULL;

        for (CMasternode&  mn : vMasternodes)
        {
            vSortedByAddr.push_back(&mn);
        }

        sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

        for (CMasternode * pmn : vSortedByAddr)
        {
            // check only (pre)enabled masternodes
            if(!pmn->IsEnabled() && !pmn->IsPreEnabled())
            {
                continue;
            }
            // initial step
            if(!pprevMasternode)
            {
                pprevMasternode = pmn;
                pverifiedMasternode = pmn->IsPoSeVerified() ? pmn : NULL;
                continue;
            }
            // second+ step
            if(pmn->addr == pprevMasternode->addr)
            {
                if(pverifiedMasternode)
                {
                    // another masternode with the same ip is verified, ban this one
                    vBan.push_back(pmn);
                }
                else if(pmn->IsPoSeVerified())
                {
                    // this masternode with the same ip is verified, ban previous one
                    vBan.push_back(pprevMasternode);
                    // and keep a reference to be able to ban following masternodes with the same ip
                    pverifiedMasternode = pmn;
                }
            }
            else
            {
                pverifiedMasternode = pmn->IsPoSeVerified() ? pmn : NULL;
            }
            pprevMasternode = pmn;
        }
    }

    // ban duplicates
    for (CMasternode * pmn : vBan)
    {
        printf("CMasternodeMan::CheckSameAddr -- increasing PoSe ban score for masternode %s\n", pmn->vin.prevout.ToString().c_str());
        pmn->IncreasePoSeBanScore();
    }
}

bool CMasternodeMan::SendVerifyRequest(const CAddress& addr, const std::vector<CMasternode*>& vSortedByAddr)
{
    if(netfulfilledman.HasFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request"))
    {
        // we already asked for verification, not a good idea to do this too often, skip it
        printf("CMasternodeMan::SendVerifyRequest -- too many requests, skipping... addr=%s\n", addr.ToString().c_str());
        return false;
    }

    CNode* pnode = ConnectNode(addr, NULL, true);
    if(pnode == NULL)
    {
        printf("CMasternodeMan::SendVerifyRequest -- can't connect to node to verify it, addr=%s\n", addr.ToString().c_str());
        return false;
    }

    netfulfilledman.AddFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request");
    // use random nonce, store it and require node to reply with correct one later
    CMasternodeVerification mnv(addr, GetRandInt(999999), pCurrentBlockIndex->nHeight - 1);
    mWeAskedForVerification[addr] = mnv;
    printf("CMasternodeMan::SendVerifyRequest -- verifying node using nonce %d addr=%s\n", mnv.nonce, addr.ToString().c_str());
    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);

    return true;
}

void CMasternodeMan::SendVerifyReply(CNode* pnode, CMasternodeVerification& mnv)
{
    // only masternodes can sign this, why would someone ask regular node?
    if(!fMasterNode)
    {
        // do not ban, malicious node might be using my IP
        // and trying to confuse the node which tries to verify it
        return;
    }

    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply"))
    {
        // peer should not ask us that often
        printf("MasternodeMan::SendVerifyReply -- ERROR: peer already asked me recently, peer=%s\n", pnode->addr.ToString().c_str());
        pnode->Misbehaving(20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight))
    {
        printf("MasternodeMan::SendVerifyReply -- can't get block hash for unknown block height %d, peer=%s\n", mnv.nBlockHeight, pnode->addr.ToString().c_str());
        return;
    }

    std::string strMessage = strprintf("%s%d%s", activeMasternode.service.ToString(), mnv.nonce, blockHash.ToString());

    // TODO implementation
    // if(!darkSendSigner.SignMessage(strMessage, mnv.vchSig1, activeMasternode.keyMasternode))
    if (true)
    {
        printf("MasternodeMan::SendVerifyReply -- SignMessage() failed\n");
        return;
    }

    std::string strError;

    // TODO implementation
    // if(!darkSendSigner.VerifyMessage(activeMasternode.pubKeyMasternode, mnv.vchSig1, strMessage, strError))
    if (true)
    {
        printf("MasternodeMan::SendVerifyReply -- VerifyMessage() failed, error: %s\n", strError.c_str());
        return;
    }

    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);
    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply");
}

void CMasternodeMan::ProcessVerifyReply(CNode* pnode, CMasternodeVerification& mnv)
{
    std::string strError;

    // did we even ask for it? if that's the case we should have matching fulfilled request
    if(!netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request"))
    {
        printf("CMasternodeMan::ProcessVerifyReply -- ERROR: we didn't ask for verification of %s\n", pnode->addr.ToString().c_str());
        pnode->Misbehaving(20);
        return;
    }

    // Received nonce for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nonce != mnv.nonce)
    {
        printf("CMasternodeMan::ProcessVerifyReply -- ERROR: wrong nounce: requested=%d, received=%d, peer=%s\n",
                    mWeAskedForVerification[pnode->addr].nonce, mnv.nonce, pnode->addr.ToString().c_str());
        pnode->Misbehaving(20);
        return;
    }

    // Received nBlockHeight for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nBlockHeight != mnv.nBlockHeight)
    {
        printf("CMasternodeMan::ProcessVerifyReply -- ERROR: wrong nBlockHeight: requested=%d, received=%d, peer=%s\n",
                    mWeAskedForVerification[pnode->addr].nBlockHeight, mnv.nBlockHeight, pnode->addr.ToString().c_str());
        pnode->Misbehaving(20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight))
    {
        // this shouldn't happen...
        printf("MasternodeMan::ProcessVerifyReply -- can't get block hash for unknown block height %d, peer=%s\n", mnv.nBlockHeight, pnode->addr.ToString().c_str());
        return;
    }

    // we already verified this address, why node is spamming?
    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done"))
    {
        printf("CMasternodeMan::ProcessVerifyReply -- ERROR: already verified %s recently\n", pnode->addr.ToString().c_str());
        pnode->Misbehaving(20);
        return;
    }

    {
        LOCK(cs);

        CMasternode* prealMasternode = NULL;
        std::vector<CMasternode*> vpMasternodesToBan;
        std::vector<CMasternode>::iterator it = vMasternodes.begin();
        std::string strMessage1 = strprintf("%s%d%s", pnode->addr.ToString(), mnv.nonce, blockHash.ToString());
        while(it != vMasternodes.end())
        {
            if((CAddress)it->addr == pnode->addr)
            {
                // TODO implementation
                // if(darkSendSigner.VerifyMessage(it->pubKeyMasternode, mnv.vchSig1, strMessage1, strError))
                if (false)
                {
                    // found it!
                    prealMasternode = &(*it);
                    if(!it->IsPoSeVerified())
                    {
                        it->DecreasePoSeBanScore();
                    }
                    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done");

                    // we can only broadcast it if we are an activated masternode
                    if(activeMasternode.vin == CTxIn())
                    {
                        continue;
                    }
                    // update ...
                    mnv.addr = it->addr;
                    mnv.vin1 = it->vin;
                    mnv.vin2 = activeMasternode.vin;
                    std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                            mnv.vin1.prevout.ToString(), mnv.vin2.prevout.ToString());
                    // ... and sign it
                    // TODO implementation
                    // if(!darkSendSigner.SignMessage(strMessage2, mnv.vchSig2, activeMasternode.keyMasternode))
                    if (true)
                    {
                        printf("MasternodeMan::ProcessVerifyReply -- SignMessage() failed\n");
                        return;
                    }

                    std::string strError;

                    // TODO implementation
                    // if(!darkSendSigner.VerifyMessage(activeMasternode.pubKeyMasternode, mnv.vchSig2, strMessage2, strError))
                    if (true)
                    {
                        printf("MasternodeMan::ProcessVerifyReply -- VerifyMessage() failed, error: %s\n", strError.c_str());
                        return;
                    }

                    mWeAskedForVerification[pnode->addr] = mnv;
                    mnv.Relay();

                }
                else
                {
                    vpMasternodesToBan.push_back(&(*it));
                }
            }
            ++it;
        }
        // no real masternode found?...
        if(!prealMasternode)
        {
            // this should never be the case normally,
            // only if someone is trying to game the system in some way or smth like that
            printf("CMasternodeMan::ProcessVerifyReply -- ERROR: no real masternode found for addr %s\n", pnode->addr.ToString().c_str());
            pnode->Misbehaving(20);
            return;
        }
        printf("CMasternodeMan::ProcessVerifyReply -- verified real masternode %s for addr %s\n",
                    prealMasternode->vin.prevout.ToString().c_str(), pnode->addr.ToString().c_str());
        // increase ban score for everyone else
        for (CMasternode * pmn : vpMasternodesToBan)
        {
            pmn->IncreasePoSeBanScore();
            printf("CMasternodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        prealMasternode->vin.prevout.ToString().c_str(), pnode->addr.ToString().c_str(), pmn->nPoSeBanScore);
        }
        printf("CMasternodeMan::ProcessVerifyBroadcast -- PoSe score increased for %d fake masternodes, addr %s\n",
                    (int)vpMasternodesToBan.size(), pnode->addr.ToString().c_str());
    }
}

void CMasternodeMan::ProcessVerifyBroadcast(CNode* pnode, const CMasternodeVerification& mnv)
{
    std::string strError;

    if(mapSeenMasternodeVerification.find(mnv.GetHash()) != mapSeenMasternodeVerification.end())
    {
        // we already have one
        return;
    }
    mapSeenMasternodeVerification[mnv.GetHash()] = mnv;

    // we don't care about history
    if(mnv.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS)
    {
        printf("MasternodeMan::ProcessVerifyBroadcast -- Outdated: current block %d, verification block %d, peer=%s\n",
                    pCurrentBlockIndex->nHeight, mnv.nBlockHeight, pnode->addr.ToString().c_str());
        return;
    }

    if(mnv.vin1.prevout == mnv.vin2.prevout)
    {
        printf("masternode", "MasternodeMan::ProcessVerifyBroadcast -- ERROR: same vins %s, peer=%s\n",
                    mnv.vin1.prevout.ToString().c_str(), pnode->addr.ToString().c_str());
        // that was NOT a good idea to cheat and verify itself,
        // ban the node we received such message from
        pnode->Misbehaving(100);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight))
    {
        // this shouldn't happen...
        printf("MasternodeMan::ProcessVerifyBroadcast -- Can't get block hash for unknown block height %d, peer=%s\n", mnv.nBlockHeight, pnode->addr.ToString().c_str());
        return;
    }

    int nRank = GetMasternodeRank(mnv.vin2, mnv.nBlockHeight, MIN_POSE_PROTO_VERSION);

    if (nRank == -1)
    {
        printf("CMasternodeMan::ProcessVerifyBroadcast -- Can't calculate rank for masternode %s\n",
                    mnv.vin2.prevout.ToString().c_str());
        return;
    }

    if(nRank > MAX_POSE_RANK)
    {
        printf("CMasternodeMan::ProcessVerifyBroadcast -- Mastrernode %s is not in top %d, current rank %d, peer=%s\n",
                    mnv.vin2.prevout.ToString().c_str(), (int)MAX_POSE_RANK, nRank, pnode->addr.ToString().c_str());
        return;
    }

    {
        LOCK(cs);

        std::string strMessage1 = strprintf("%s%d%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString());
        std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                mnv.vin1.prevout.ToString(), mnv.vin2.prevout.ToString());

        CMasternode* pmn1 = Find(mnv.vin1);
        if(!pmn1)
        {
            printf("CMasternodeMan::ProcessVerifyBroadcast -- can't find masternode1 %s\n", mnv.vin1.prevout.ToString().c_str());
            return;
        }

        CMasternode* pmn2 = Find(mnv.vin2);
        if(!pmn2)
        {
            printf("CMasternodeMan::ProcessVerifyBroadcast -- can't find masternode2 %s\n", mnv.vin2.prevout.ToString().c_str());
            return;
        }

        if(pmn1->addr != mnv.addr)
        {
            printf("CMasternodeMan::ProcessVerifyBroadcast -- addr %s do not match %s\n", mnv.addr.ToString().c_str(), pnode->addr.ToString().c_str());
            return;
        }

        // TODO implementation
        // if(darkSendSigner.VerifyMessage(pmn1->pubKeyMasternode, mnv.vchSig1, strMessage1, strError))
        if (true)
        {
            printf("MasternodeMan::ProcessVerifyBroadcast -- VerifyMessage() for masternode1 failed, error: %s\n", strError.c_str());
            return;
        }

        // TODO implementation
        // if(darkSendSigner.VerifyMessage(pmn2->pubKeyMasternode, mnv.vchSig2, strMessage2, strError))
        if (true)
        {
            printf("MasternodeMan::ProcessVerifyBroadcast -- VerifyMessage() for masternode2 failed, error: %s\n", strError.c_str());
            return;
        }

        if(!pmn1->IsPoSeVerified())
        {
            pmn1->DecreasePoSeBanScore();
        }
        mnv.Relay();

        printf("CMasternodeMan::ProcessVerifyBroadcast -- verified masternode %s for addr %s\n",
                    pmn1->vin.prevout.ToString().c_str(), pnode->addr.ToString().c_str());

        // increase ban score for everyone else with the same addr
        int nCount = 0;
        for (CMasternode & mn : vMasternodes)
        {
            if(mn.addr != mnv.addr || mn.vin.prevout == mnv.vin1.prevout)
            {
                continue;
            }
            mn.IncreasePoSeBanScore();
            nCount++;
            printf("CMasternodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        mn.vin.prevout.ToString().c_str(), mn.addr.ToString().c_str(), mn.nPoSeBanScore);
        }
        printf("CMasternodeMan::ProcessVerifyBroadcast -- PoSe score incresed for %d fake masternodes, addr %s\n",
                    nCount, pnode->addr.ToString().c_str());
    }
}

std::string CMasternodeMan::ToString() const
{
    std::ostringstream info;

    info << "Masternodes: " << (int)vMasternodes.size() <<
            ", peers who asked us for Masternode list: " << (int)mAskedUsForMasternodeList.size() <<
            ", peers we asked for Masternode list: " << (int)mWeAskedForMasternodeList.size() <<
            ", entries in Masternode list we asked for: " << (int)mWeAskedForMasternodeListEntry.size() <<
            ", masternode index size: " << indexMasternodes.GetSize() <<
            ", nDsqCount: " << (int)nDsqCount;

    return info.str();
}

void CMasternodeMan::UpdateMasternodeList(CMasternodeBroadcast mnb)
{
    LOCK(cs);
    mapSeenMasternodePing.insert(std::make_pair(mnb.lastPing.GetHash(), mnb.lastPing));
    mapSeenMasternodeBroadcast.insert(std::make_pair(mnb.GetHash(), std::make_pair(GetTime(), mnb)));

    printf("CMasternodeMan::UpdateMasternodeList -- masternode=%s  addr=%s\n", mnb.vin.prevout.ToString().c_str(), mnb.addr.ToString().c_str());

    CMasternode* pmn = Find(mnb.vin);
    if(pmn == NULL)
    {
        CMasternode mn(mnb);
        if(Add(mn))
        {
            masternodeSync.AddedMasternodeList();
        }
    }
    else
    {
        CMasternodeBroadcast mnbOld = mapSeenMasternodeBroadcast[CMasternodeBroadcast(*pmn).GetHash()].second;
        if(pmn->UpdateFromNewBroadcast(mnb))
        {
            masternodeSync.AddedMasternodeList();
            mapSeenMasternodeBroadcast.erase(mnbOld.GetHash());
        }
    }
}

bool CMasternodeMan::CheckMnbAndUpdateMasternodeList(CNode* pfrom, CMasternodeBroadcast mnb, int& nDos)
{
    // Need LOCK2 here to ensure consistent locking order because the SimpleCheck call below locks cs_main
    LOCK2(cs_main, cs);

    nDos = 0;
    printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- masternode=%s\n", mnb.vin.prevout.ToString().c_str());

    uint256 hash = mnb.GetHash();
    if(mapSeenMasternodeBroadcast.count(hash) && !mnb.fRecovery)
    {
        //seen
        printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- masternode=%s seen\n", mnb.vin.prevout.ToString().c_str());
        // less then 2 pings left before this MN goes into non-recoverable state, bump sync timeout
        if(GetTime() - mapSeenMasternodeBroadcast[hash].first > MASTERNODE_NEW_START_REQUIRED_SECONDS - MASTERNODE_MIN_MNP_SECONDS * 2)
        {
            printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- masternode=%s seen update\n", mnb.vin.prevout.ToString().c_str());
            mapSeenMasternodeBroadcast[hash].first = GetTime();
            masternodeSync.AddedMasternodeList();
        }
        // did we ask this node for it?
        if(pfrom && IsMnbRecoveryRequested(hash) && GetTime() < mMnbRecoveryRequests[hash].first)
        {
            printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- mnb=%s seen request\n", hash.ToString().c_str());
            if(mMnbRecoveryRequests[hash].second.count(pfrom->addr))
            {
                printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- mnb=%s seen request, addr=%s\n", hash.ToString().c_str(), pfrom->addr.ToString().c_str());
                // do not allow node to send same mnb multiple times in recovery mode
                mMnbRecoveryRequests[hash].second.erase(pfrom->addr);
                // does it have newer lastPing?
                if(mnb.lastPing.sigTime > mapSeenMasternodeBroadcast[hash].second.lastPing.sigTime)
                {
                    // simulate Check
                    CMasternode mnTemp = CMasternode(mnb);
                    mnTemp.Check();
                    printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- mnb=%s seen request, addr=%s, better lastPing: %d min ago, projected mn state: %s\n",
                           hash.ToString().c_str(), pfrom->addr.ToString().c_str(), (GetTime() - mnb.lastPing.sigTime)/60, mnTemp.GetStateString().c_str());
                    if(mnTemp.IsValidStateForAutoStart(mnTemp.nActiveState))
                    {
                        // this node thinks it's a good one
                        printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- masternode=%s seen good\n", mnb.vin.prevout.ToString().c_str());
                        mMnbRecoveryGoodReplies[hash].push_back(mnb);
                    }
                }
            }
        }
        return true;
    }
    mapSeenMasternodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));

    printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- masternode=%s new\n", mnb.vin.prevout.ToString().c_str());

    if(!mnb.SimpleCheck(nDos))
    {
        printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- SimpleCheck() failed, masternode=%s\n", mnb.vin.prevout.ToString().c_str());
        return false;
    }

    // search Masternode list
    CMasternode* pmn = Find(mnb.vin);
    if(pmn)
    {
        CMasternodeBroadcast mnbOld = mapSeenMasternodeBroadcast[CMasternodeBroadcast(*pmn).GetHash()].second;
        if(!mnb.Update(pmn, nDos))
        {
            printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- Update() failed, masternode=%s\n", mnb.vin.prevout.ToString().c_str());
            return false;
        }
        if(hash != mnbOld.GetHash())
        {
           mapSeenMasternodeBroadcast.erase(mnbOld.GetHash());
        }
    }
    else
    {
        if(mnb.CheckOutpoint(nDos))
        {
            Add(mnb);
            masternodeSync.AddedMasternodeList();
            // if it matches our Masternode privkey...
            if(fMasterNode && mnb.pubKeyMasternode == activeMasternode.pubKeyMasternode)
            {
                mnb.nPoSeBanScore = -MASTERNODE_POSE_BAN_MAX_SCORE;
                if(mnb.nProtocolVersion == PROTOCOL_VERSION)
                {
                    // ... and PROTOCOL_VERSION, then we've been remotely activated ...
                    printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- Got NEW Masternode entry: masternode=%s  sigTime=%lld  addr=%s\n",
                                mnb.vin.prevout.ToString().c_str(), mnb.sigTime, mnb.addr.ToString().c_str());
                    activeMasternode.ManageState();
                }
                else
                {
                    // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
                    // but also do not ban the node we get this message from
                    printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", mnb.nProtocolVersion, PROTOCOL_VERSION);
                    return false;
                }
            }
            mnb.Relay();
        }
        else
        {
            printf("CMasternodeMan::CheckMnbAndUpdateMasternodeList -- Rejected Masternode entry: %s  addr=%s\n", mnb.vin.prevout.ToString().c_str(), mnb.addr.ToString().c_str());
            return false;
        }
    }

    return true;
}

void CMasternodeMan::UpdateLastPaid()
{
    LOCK(cs);

    if(!pCurrentBlockIndex)
    {
        return;
    }

    static bool IsFirstRun = true;
    // Do full scan on first run or if we are not a masternode
    // (MNs should update this info on every block, so limited scan should be enough for them)
    int nMaxBlocksToScanBack = (IsFirstRun || !fMasterNode) ? mnpayments.GetStorageLimit() : LAST_PAID_SCAN_BLOCKS;

    // LogPrint("mnpayments", "CMasternodeMan::UpdateLastPaid -- nHeight=%d, nMaxBlocksToScanBack=%d, IsFirstRun=%s\n",
    //                         pCurrentBlockIndex->nHeight, nMaxBlocksToScanBack, IsFirstRun ? "true" : "false");

    for (CMasternode & mn : vMasternodes)
    {
        mn.UpdateLastPaid(pCurrentBlockIndex, nMaxBlocksToScanBack);
    }

    // every time is like the first time if winners list is not synced
    IsFirstRun = !masternodeSync.IsWinnersListSynced();
}

void CMasternodeMan::CheckAndRebuildMasternodeIndex()
{
    LOCK(cs);

    if(GetTime() - nLastIndexRebuildTime < MIN_INDEX_REBUILD_TIME) {
        return;
    }

    if(indexMasternodes.GetSize() <= MAX_EXPECTED_INDEX_SIZE) {
        return;
    }

    if(indexMasternodes.GetSize() <= int(vMasternodes.size())) {
        return;
    }

    indexMasternodesOld = indexMasternodes;
    indexMasternodes.Clear();
    for(size_t i = 0; i < vMasternodes.size(); ++i) {
        indexMasternodes.AddMasternodeVIN(vMasternodes[i].vin);
    }

    fIndexRebuilt = true;
    nLastIndexRebuildTime = GetTime();
}

void CMasternodeMan::UpdateWatchdogVoteTime(const CTxIn& vin)
{
    LOCK(cs);
    CMasternode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->UpdateWatchdogVoteTime();
    nLastWatchdogVoteTime = GetTime();
}

bool CMasternodeMan::IsWatchdogActive()
{
    LOCK(cs);
    // Check if any masternodes have voted recently, otherwise return false
    return (GetTime() - nLastWatchdogVoteTime) <= MASTERNODE_WATCHDOG_MAX_SECONDS;
}

bool CMasternodeMan::AddGovernanceVote(const CTxIn& vin, uint256 nGovernanceObjectHash)
{
    LOCK(cs);
    CMasternode* pMN = Find(vin);
    if(!pMN)  {
        return false;
    }
    pMN->AddGovernanceVote(nGovernanceObjectHash);
    return true;
}

void CMasternodeMan::RemoveGovernanceObject(uint256 nGovernanceObjectHash)
{
//    LOCK(cs);
//    BOOST_FOREACH(CMasternode& mn, vMasternodes) {
//        mn.RemoveGovernanceObject(nGovernanceObjectHash);
//    }
}

void CMasternodeMan::CheckMasternode(const CTxIn& vin, bool fForce)
{
    LOCK(cs);
    CMasternode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

void CMasternodeMan::CheckMasternode(const CPubKey& pubKeyMasternode, bool fForce)
{
    LOCK(cs);
    CMasternode* pMN = Find(pubKeyMasternode);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

int CMasternodeMan::GetMasternodeState(const CTxIn& vin)
{
    LOCK(cs);
    CMasternode* pMN = Find(vin);
    if(!pMN)  {
        return CMasternode::MASTERNODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

int CMasternodeMan::GetMasternodeState(const CPubKey& pubKeyMasternode)
{
    LOCK(cs);
    CMasternode* pMN = Find(pubKeyMasternode);
    if(!pMN)  {
        return CMasternode::MASTERNODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

bool CMasternodeMan::IsMasternodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt)
{
    LOCK(cs);
    CMasternode* pMN = Find(vin);
    if(!pMN) {
        return false;
    }
    return pMN->IsPingedWithin(nSeconds, nTimeToCheckAt);
}

void CMasternodeMan::SetMasternodeLastPing(const CTxIn& vin, const CMasternodePing& mnp)
{
    LOCK(cs);
    CMasternode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->lastPing = mnp;
    mapSeenMasternodePing.insert(std::make_pair(mnp.GetHash(), mnp));

    CMasternodeBroadcast mnb(*pMN);
    uint256 hash = mnb.GetHash();
    if(mapSeenMasternodeBroadcast.count(hash)) {
        mapSeenMasternodeBroadcast[hash].second.lastPing = mnp;
    }
}

void CMasternodeMan::UpdatedBlockTip(const CBlockIndex *pindex)
{
    pCurrentBlockIndex = pindex;
    printf("CMasternodeMan::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);

    CheckSameAddr();

    if(fMasterNode)
    {
        // normal wallet does not need to update this every block, doing update on rpc call should be enough
        UpdateLastPaid();
    }
}

void CMasternodeMan::NotifyMasternodeUpdates()
{
    // Avoid double locking
//    bool fMasternodesAddedLocal = false;
//    bool fMasternodesRemovedLocal = false;
//    {
//        LOCK(cs);
//        fMasternodesAddedLocal = fMasternodesAdded;
//        fMasternodesRemovedLocal = fMasternodesRemoved;
//    }

//    if(fMasternodesAddedLocal) {
//        governance.CheckMasternodeOrphanObjects();
//        governance.CheckMasternodeOrphanVotes();
//    }
//    if(fMasternodesRemovedLocal) {
//        governance.UpdateCachesAndClean();
//    }

    LOCK(cs);
    fMasternodesAdded = false;
    fMasternodesRemoved = false;
}
