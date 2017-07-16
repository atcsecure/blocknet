// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeservicenode.h"
#include "addrman.h"
// #include "darksend.h"
// #include "governance.h"
#include "servicenode-payments.h"
#include "servicenode-sync.h"
#include "servicenodeman.h"
#include "netfulfilledman.h"
#include "util.h"
#include "datasigner.h"

/** Servicenode manager */
CServicenodeMan mnodeman;

const std::string CServicenodeMan::SERIALIZATION_VERSION_STRING = "CServicenodeMan-Version-4";

struct CompareLastPaidBlock
{
    bool operator()(const std::pair<int, CServicenode*>& t1,
                    const std::pair<int, CServicenode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

struct CompareScoreMN
{
    bool operator()(const std::pair<int64_t, CServicenode*>& t1,
                    const std::pair<int64_t, CServicenode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

CServicenodeIndex::CServicenodeIndex()
    : nSize(0),
      mapIndex(),
      mapReverseIndex()
{}

bool CServicenodeIndex::Get(int nIndex, CTxIn& vinServicenode) const
{
    rindex_m_cit it = mapReverseIndex.find(nIndex);
    if(it == mapReverseIndex.end()) {
        return false;
    }
    vinServicenode = it->second;
    return true;
}

int CServicenodeIndex::GetServicenodeIndex(const CTxIn& vinServicenode) const
{
    index_m_cit it = mapIndex.find(vinServicenode);
    if(it == mapIndex.end()) {
        return -1;
    }
    return it->second;
}

void CServicenodeIndex::AddServicenodeVIN(const CTxIn& vinServicenode)
{
    index_m_it it = mapIndex.find(vinServicenode);
    if(it != mapIndex.end()) {
        return;
    }
    int nNextIndex = nSize;
    mapIndex[vinServicenode] = nNextIndex;
    mapReverseIndex[nNextIndex] = vinServicenode;
    ++nSize;
}

void CServicenodeIndex::Clear()
{
    mapIndex.clear();
    mapReverseIndex.clear();
    nSize = 0;
}
struct CompareByAddr

{
    bool operator()(const CServicenode* t1,
                    const CServicenode* t2) const
    {
        return t1->addr < t2->addr;
    }
};

void CServicenodeIndex::RebuildIndex()
{
    nSize = mapIndex.size();
    for(index_m_it it = mapIndex.begin(); it != mapIndex.end(); ++it) {
        mapReverseIndex[it->second] = it->first;
    }
}

CServicenodeMan::CServicenodeMan()
: cs(),
  vServicenodes(),
  mAskedUsForServicenodeList(),
  mWeAskedForServicenodeList(),
  mWeAskedForServicenodeListEntry(),
  mWeAskedForVerification(),
  mMnbRecoveryRequests(),
  mMnbRecoveryGoodReplies(),
  listScheduledMnbRequestConnections(),
  nLastIndexRebuildTime(0),
  indexServicenodes(),
  indexServicenodesOld(),
  fIndexRebuilt(false),
  fServicenodesAdded(false),
  fServicenodesRemoved(false),
  vecDirtyGovernanceObjectHashes(),
  nLastWatchdogVoteTime(0),
  mapSeenServicenodeBroadcast(),
  mapSeenServicenodePing(),
  nDsqCount(0)
{}

bool CServicenodeMan::Add(CServicenode &mn)
{
    LOCK(cs);

    CServicenode *pmn = Find(mn.vin);
    if (pmn == NULL) {
        printf("CServicenodeMan::Add -- Adding new Servicenode: addr=%s, %i now\n", mn.addr.ToString().c_str(), size() + 1);
        vServicenodes.push_back(mn);
        indexServicenodes.AddServicenodeVIN(mn.vin);
        fServicenodesAdded = true;
        return true;
    }

    return false;
}

void CServicenodeMan::AskForMN(CNode* pnode, const CTxIn &vin)
{
    if(!pnode) return;

    LOCK(cs);

    std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it1 = mWeAskedForServicenodeListEntry.find(vin.prevout);
    if (it1 != mWeAskedForServicenodeListEntry.end()) {
        std::map<CNetAddr, int64_t>::iterator it2 = it1->second.find(pnode->addr);
        if (it2 != it1->second.end()) {
            if (GetTime() < it2->second) {
                // we've asked recently, should not repeat too often or we could get banned
                return;
            }
            // we asked this node for this outpoint but it's ok to ask again already
            printf("CServicenodeMan::AskForMN -- Asking same peer %s for missing servicenode entry again: %s\n", pnode->addr.ToString().c_str(), vin.prevout.ToString().c_str());
        } else {
            // we already asked for this outpoint but not this node
            printf("CServicenodeMan::AskForMN -- Asking new peer %s for missing servicenode entry: %s\n", pnode->addr.ToString().c_str(), vin.prevout.ToString().c_str());
        }
    } else {
        // we never asked any node for this outpoint
        printf("CServicenodeMan::AskForMN -- Asking peer %s for missing servicenode entry for the first time: %s\n", pnode->addr.ToString().c_str(), vin.prevout.ToString().c_str());
    }
    mWeAskedForServicenodeListEntry[vin.prevout][pnode->addr] = GetTime() + DSEG_UPDATE_SECONDS;

    pnode->PushMessage(NetMsgType::DSEG, vin);
}

void CServicenodeMan::Check()
{
    LOCK(cs);

    printf("CServicenodeMan::Check -- nLastWatchdogVoteTime=%d, IsWatchdogActive()=%d\n", nLastWatchdogVoteTime, IsWatchdogActive());

    for (CServicenode & mn : vServicenodes) {
        mn.Check();
    }
}

void CServicenodeMan::CheckAndRemove()
{
    if(!servicenodeSync.IsServicenodeListSynced())
    {
        return;
    }

    printf("CServicenodeMan::CheckAndRemove\n");

    {
        // Need LOCK2 here to ensure consistent locking order because code below locks cs_main
        // in CheckMnbAndUpdateServicenodeList()
        LOCK2(cs_main, cs);

        Check();

        // Remove spent servicenodes, prepare structures and make requests to reasure the state of inactive ones
        std::vector<CServicenode>::iterator it = vServicenodes.begin();
        std::vector<std::pair<int, CServicenode> > vecServicenodeRanks;
        // ask for up to MNB_RECOVERY_MAX_ASK_ENTRIES servicenode entries at a time
        int nAskForMnbRecovery = MNB_RECOVERY_MAX_ASK_ENTRIES;
        while(it != vServicenodes.end())
        {
            CServicenodeBroadcast mnb = CServicenodeBroadcast(*it);
            uint256 hash = mnb.GetHash();
            // If collateral was spent ...
            if ((*it).IsOutpointSpent())
            {
                printf("CServicenodeMan::CheckAndRemove -- Removing Servicenode: %s  addr=%s  %i now\n", (*it).GetStateString().c_str(), (*it).addr.ToString().c_str(), size() - 1);

                // erase all of the broadcasts we've seen from this txin, ...
                mapSeenServicenodeBroadcast.erase(hash);
                mWeAskedForServicenodeListEntry.erase((*it).vin.prevout);

                // and finally remove it from the list
                it->FlagGovernanceItemsAsDirty();
                it = vServicenodes.erase(it);
                fServicenodesRemoved = true;
            }
            else
            {
                bool fAsk = pCurrentBlockIndex &&
                            (nAskForMnbRecovery > 0) &&
                            servicenodeSync.IsSynced() &&
                            it->IsNewStartRequired() &&
                            !IsMnbRecoveryRequested(hash);
                if(fAsk)
                {
                    // this mn is in a non-recoverable state and we haven't asked other nodes yet
                    std::set<CNetAddr> setRequested;
                    // calulate only once and only when it's needed
                    if(vecServicenodeRanks.empty())
                    {
                        int nRandomBlockHeight = GetRandInt(pCurrentBlockIndex->nHeight);
                        vecServicenodeRanks = GetServicenodeRanks(nRandomBlockHeight);
                    }
                    bool fAskedForMnbRecovery = false;
                    // ask first MNB_RECOVERY_QUORUM_TOTAL servicenodes we can connect to and we haven't asked recently
                    for(int i = 0; setRequested.size() < MNB_RECOVERY_QUORUM_TOTAL && i < (int)vecServicenodeRanks.size(); i++)
                    {
                        // avoid banning
                        if(mWeAskedForServicenodeListEntry.count(it->vin.prevout) && mWeAskedForServicenodeListEntry[it->vin.prevout].count(vecServicenodeRanks[i].second.addr))
                        {
                            continue;
                        }
                        // didn't ask recently, ok to ask now
                        CService addr = vecServicenodeRanks[i].second.addr;
                        setRequested.insert(addr);
                        listScheduledMnbRequestConnections.push_back(std::make_pair(addr, hash));
                        fAskedForMnbRecovery = true;
                    }
                    if(fAskedForMnbRecovery)
                    {
                        printf("CServicenodeMan::CheckAndRemove -- Recovery initiated, servicenode=%s\n", it->vin.prevout.ToString().c_str());
                        nAskForMnbRecovery--;
                    }
                    // wait for mnb recovery replies for MNB_RECOVERY_WAIT_SECONDS seconds
                    mMnbRecoveryRequests[hash] = std::make_pair(GetTime() + MNB_RECOVERY_WAIT_SECONDS, setRequested);
                }
                ++it;
            }
        }

        // proces replies for SERVICENODE_NEW_START_REQUIRED servicenodes
        printf("CServicenodeMan::CheckAndRemove -- mMnbRecoveryGoodReplies size=%d\n", (int)mMnbRecoveryGoodReplies.size());
        std::map<uint256, std::vector<CServicenodeBroadcast> >::iterator itMnbReplies = mMnbRecoveryGoodReplies.begin();
        while(itMnbReplies != mMnbRecoveryGoodReplies.end())
        {
            if(mMnbRecoveryRequests[itMnbReplies->first].first < GetTime())
            {
                // all nodes we asked should have replied now
                if(itMnbReplies->second.size() >= MNB_RECOVERY_QUORUM_REQUIRED)
                {
                    // majority of nodes we asked agrees that this mn doesn't require new mnb, reprocess one of new mnbs
                    printf("CServicenodeMan::CheckAndRemove -- reprocessing mnb, servicenode=%s\n", itMnbReplies->second[0].vin.prevout.ToString().c_str());
                    // mapSeenServicenodeBroadcast.erase(itMnbReplies->first);
                    int nDos;
                    itMnbReplies->second[0].fRecovery = true;
                    CheckMnbAndUpdateServicenodeList(NULL, itMnbReplies->second[0], nDos);
                }
                printf("CServicenodeMan::CheckAndRemove -- removing mnb recovery reply, servicenode=%s, size=%d\n", itMnbReplies->second[0].vin.prevout.ToString().c_str(), (int)itMnbReplies->second.size());
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
            // if mn is still in SERVICENODE_NEW_START_REQUIRED state.
            if(GetTime() - itMnbRequest->second.first > MNB_RECOVERY_RETRY_SECONDS)
            {
                mMnbRecoveryRequests.erase(itMnbRequest++);
            }
            else
            {
                ++itMnbRequest;
            }
        }

        // check who's asked for the Servicenode list
        std::map<CNetAddr, int64_t>::iterator it1 = mAskedUsForServicenodeList.begin();
        while(it1 != mAskedUsForServicenodeList.end())
        {
            if((*it1).second < GetTime())
            {
                mAskedUsForServicenodeList.erase(it1++);
            }
            else
            {
                ++it1;
            }
        }

        // check who we asked for the Servicenode list
        it1 = mWeAskedForServicenodeList.begin();
        while(it1 != mWeAskedForServicenodeList.end())
        {
            if((*it1).second < GetTime())
            {
                mWeAskedForServicenodeList.erase(it1++);
            }
            else
            {
                ++it1;
            }
        }

        // check which Servicenodes we've asked for
        std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it2 = mWeAskedForServicenodeListEntry.begin();
        while(it2 != mWeAskedForServicenodeListEntry.end())
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
                mWeAskedForServicenodeListEntry.erase(it2++);
            }
            else
            {
                ++it2;
            }
        }

        std::map<CNetAddr, CServicenodeVerification>::iterator it3 = mWeAskedForVerification.begin();
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

        // NOTE: do not expire mapSeenServicenodeBroadcast entries here, clean them on mnb updates!

        // remove expired mapSeenServicenodePing
        std::map<uint256, CServicenodePing>::iterator it4 = mapSeenServicenodePing.begin();
        while(it4 != mapSeenServicenodePing.end())
        {
            if((*it4).second.IsExpired())
            {
                printf("CServicenodeMan::CheckAndRemove -- Removing expired Servicenode ping: hash=%s\n", (*it4).second.GetHash().ToString().c_str());
                mapSeenServicenodePing.erase(it4++);
            }
            else
            {
                ++it4;
            }
        }

        // remove expired mapSeenServicenodeVerification
        std::map<uint256, CServicenodeVerification>::iterator itv2 = mapSeenServicenodeVerification.begin();
        while(itv2 != mapSeenServicenodeVerification.end())
        {
            if((*itv2).second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS)
            {
                printf("CServicenodeMan::CheckAndRemove -- Removing expired Servicenode verification: hash=%s\n", (*itv2).first.ToString().c_str());
                mapSeenServicenodeVerification.erase(itv2++);
            }
            else
            {
                ++itv2;
            }
        }

        printf("CServicenodeMan::CheckAndRemove -- %s\n", ToString().c_str());

        if(fServicenodesRemoved)
        {
            CheckAndRebuildServicenodeIndex();
        }
    }

    if(fServicenodesRemoved)
    {
        NotifyServicenodeUpdates();
    }
}

void CServicenodeMan::Clear()
{
    LOCK(cs);
    vServicenodes.clear();
    mAskedUsForServicenodeList.clear();
    mWeAskedForServicenodeList.clear();
    mWeAskedForServicenodeListEntry.clear();
    mapSeenServicenodeBroadcast.clear();
    mapSeenServicenodePing.clear();
    nDsqCount = 0;
    nLastWatchdogVoteTime = 0;
    indexServicenodes.Clear();
    indexServicenodesOld.Clear();
}

int CServicenodeMan::CountServicenodes(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinServicenodePaymentsProto() : nProtocolVersion;

    for (CServicenode & mn : vServicenodes)
    {
        if(mn.nProtocolVersion < nProtocolVersion) continue;
        nCount++;
    }

    return nCount;
}

int CServicenodeMan::CountEnabled(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinServicenodePaymentsProto() : nProtocolVersion;

    for (CServicenode & mn : vServicenodes)
    {
        if(mn.nProtocolVersion < nProtocolVersion || !mn.IsEnabled()) continue;
        nCount++;
    }

    return nCount;
}

/* Only IPv4 servicenodes are allowed in 12.1, saving this for later
int CServicenodeMan::CountByIP(int nNetworkType)
{
    LOCK(cs);
    int nNodeCount = 0;

    BOOST_FOREACH(CServicenode& mn, vServicenodes)
        if ((nNetworkType == NET_IPV4 && mn.addr.IsIPv4()) ||
            (nNetworkType == NET_TOR  && mn.addr.IsTor())  ||
            (nNetworkType == NET_IPV6 && mn.addr.IsIPv6())) {
                nNodeCount++;
        }

    return nNodeCount;
}
*/

void CServicenodeMan::DsegUpdate(CNode* pnode)
{
    LOCK(cs);

    if (!fTestNet)
    {
        if (!(pnode->addr.IsRFC1918() || pnode->addr.IsLocal()))
        {
            std::map<CNetAddr, int64_t>::iterator it = mWeAskedForServicenodeList.find(pnode->addr);
            if (it != mWeAskedForServicenodeList.end() && GetTime() < (*it).second)
            {
                printf("CServicenodeMan::DsegUpdate -- we already asked %s for the list; skipping...\n", pnode->addr.ToString().c_str());
                return;
            }
        }
    }
    
    pnode->PushMessage(NetMsgType::DSEG, CTxIn());
    int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
    mWeAskedForServicenodeList[pnode->addr] = askAgain;

    printf("CServicenodeMan::DsegUpdate -- asked %s for the list\n", pnode->addr.ToString().c_str());
}

CServicenode* CServicenodeMan::Find(const CScript &payee)
{
    LOCK(cs);

    for (CServicenode & mn : vServicenodes)
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

CServicenode* CServicenodeMan::Find(const CTxIn &vin)
{
    LOCK(cs);

    for (CServicenode & mn : vServicenodes)
    {
        if(mn.vin.prevout == vin.prevout)
        {
            return &mn;
        }
    }
    return NULL;
}

CServicenode* CServicenodeMan::Find(const CPubKey &pubKeyServicenode)
{
    LOCK(cs);

    for (CServicenode & mn : vServicenodes)
    {
        if(mn.pubKeyServicenode == pubKeyServicenode)
        {
            return &mn;
        }
    }
    return NULL;
}

bool CServicenodeMan::Get(const CPubKey& pubKeyServicenode, CServicenode& servicenode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CServicenode* pMN = Find(pubKeyServicenode);
    if(!pMN)  {
        return false;
    }
    servicenode = *pMN;
    return true;
}

bool CServicenodeMan::Get(const CTxIn& vin, CServicenode& servicenode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CServicenode* pMN = Find(vin);
    if(!pMN)  {
        return false;
    }
    servicenode = *pMN;
    return true;
}

servicenode_info_t CServicenodeMan::GetServicenodeInfo(const CTxIn& vin)
{
    servicenode_info_t info;
    LOCK(cs);
    CServicenode* pMN = Find(vin);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

servicenode_info_t CServicenodeMan::GetServicenodeInfo(const CPubKey& pubKeyServicenode)
{
    servicenode_info_t info;
    LOCK(cs);
    CServicenode* pMN = Find(pubKeyServicenode);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

bool CServicenodeMan::Has(const CTxIn& vin)
{
    LOCK(cs);
    CServicenode* pMN = Find(vin);
    return (pMN != NULL);
}

//
// Deterministically select the oldest/best servicenode to pay on the network
//
CServicenode* CServicenodeMan::GetNextServicenodeInQueueForPayment(bool fFilterSigTime, int& nCount)
{
    if(!pCurrentBlockIndex) {
        nCount = 0;
        return NULL;
    }
    return GetNextServicenodeInQueueForPayment(pCurrentBlockIndex->nHeight, fFilterSigTime, nCount);
}

CServicenode* CServicenodeMan::GetNextServicenodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount)
{
    // Need LOCK2 here to ensure consistent locking order because the GetBlockHash call below locks cs_main
    LOCK2(cs_main,cs);

    CServicenode *pBestServicenode = NULL;
    std::vector<std::pair<int, CServicenode*> > vecServicenodeLastPaid;

    /*
        Make a vector with all of the last paid times
    */

    int nMnCount = CountEnabled();
    for (CServicenode & mn : vServicenodes)
    {
        if(!mn.IsValidForPayment())
        {
            continue;
        }

        // //check protocol version
        if(mn.nProtocolVersion < mnpayments.GetMinServicenodePaymentsProto())
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

        //make sure it has at least as many confirmations as there are servicenodes
        if(mn.GetCollateralAge() < nMnCount)
        {
            continue;
        }

        vecServicenodeLastPaid.push_back(std::make_pair(mn.GetLastPaidBlock(), &mn));
    }

    nCount = (int)vecServicenodeLastPaid.size();

    //when the network is in the process of upgrading, don't penalize nodes that recently restarted
    if(fFilterSigTime && nCount < nMnCount/3)
    {
        return GetNextServicenodeInQueueForPayment(nBlockHeight, false, nCount);
    }

    // Sort them low to high
    sort(vecServicenodeLastPaid.begin(), vecServicenodeLastPaid.end(), CompareLastPaidBlock());

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight - 101))
    {
        printf("CServicenode::GetNextServicenodeInQueueForPayment -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight - 101);
        return NULL;
    }

    // Look at 1/10 of the oldest nodes (by last payment), calculate their scores and pay the best one
    //  -- This doesn't look at who is being paid in the +8-10 blocks, allowing for double payments very rarely
    //  -- 1/100 payments should be a double payment on mainnet - (1/(3000/10))*2
    //  -- (chance per block * chances before IsScheduled will fire)
    int nTenthNetwork = nMnCount/10;
    int nCountTenth = 0;
    arith_uint256 nHighest = 0;
    for (PAIRTYPE(int, CServicenode*)& s : vecServicenodeLastPaid)
    {
        arith_uint256 nScore = s.second->CalculateScore(blockHash);
        if(nScore > nHighest)
        {
            nHighest = nScore;
            pBestServicenode = s.second;
        }
        nCountTenth++;
        if(nCountTenth >= nTenthNetwork)
        {
            break;
        }
    }
    return pBestServicenode;
}

CServicenode* CServicenodeMan::FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion)
{
    LOCK(cs);

    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinServicenodePaymentsProto() : nProtocolVersion;

    int nCountEnabled = CountEnabled(nProtocolVersion);
    int nCountNotExcluded = nCountEnabled - vecToExclude.size();

    printf("CServicenodeMan::FindRandomNotInVec -- %d enabled servicenodes, %d servicenodes to choose from\n", nCountEnabled, nCountNotExcluded);
    if(nCountNotExcluded < 1)
    {
        return nullptr;
    }

    // fill a vector of pointers
    std::vector<CServicenode*> vpServicenodesShuffled;
    for (CServicenode & mn : vServicenodes)
    {
        vpServicenodesShuffled.push_back(&mn);
    }

    InsecureRand insecureRand;
    // shuffle pointers
    std::random_shuffle(vpServicenodesShuffled.begin(), vpServicenodesShuffled.end(), insecureRand);
    bool fExclude;

    // loop through
    for (CServicenode * pmn : vpServicenodesShuffled)
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
        printf("CServicenodeMan::FindRandomNotInVec -- found, servicenode=%s\n", pmn->vin.prevout.ToString().c_str());
        return pmn;
    }

    printf("CServicenodeMan::FindRandomNotInVec -- failed\n");
    return nullptr;
}

int CServicenodeMan::GetServicenodeRank(const CTxIn& vin, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CServicenode*> > vecServicenodeScores;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight))
    {
        return -1;
    }

    LOCK(cs);

    // scan for winner
    for (CServicenode & mn : vServicenodes)
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

        vecServicenodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecServicenodeScores.rbegin(), vecServicenodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    for (PAIRTYPE(int64_t, CServicenode*) & scorePair : vecServicenodeScores)
    {
        nRank++;
        if(scorePair.second->vin.prevout == vin.prevout)
        {
            return nRank;
        }
    }

    return -1;
}

std::vector<std::pair<int, CServicenode> > CServicenodeMan::GetServicenodeRanks(int nBlockHeight, int nMinProtocol)
{
    std::vector<std::pair<int64_t, CServicenode*> > vecServicenodeScores;
    std::vector<std::pair<int, CServicenode> > vecServicenodeRanks;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight))
    {
        return vecServicenodeRanks;
    }

    LOCK(cs);

    // scan for winner
    for (CServicenode & mn : vServicenodes)
    {
        if(mn.nProtocolVersion < nMinProtocol || !mn.IsEnabled())
        {
            continue;
        }

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecServicenodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecServicenodeScores.rbegin(), vecServicenodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    for (PAIRTYPE(int64_t, CServicenode*) & s : vecServicenodeScores)
    {
        nRank++;
        vecServicenodeRanks.push_back(std::make_pair(nRank, *s.second));
    }

    return vecServicenodeRanks;
}

CServicenode* CServicenodeMan::GetServicenodeByRank(int nRank, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CServicenode*> > vecServicenodeScores;

    LOCK(cs);

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight))
    {
        printf("CServicenode::GetServicenodeByRank -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight);
        return nullptr;
    }

    // Fill scores
    for (CServicenode & mn : vServicenodes)
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

        vecServicenodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecServicenodeScores.rbegin(), vecServicenodeScores.rend(), CompareScoreMN());

    int rank = 0;
    for (PAIRTYPE(int64_t, CServicenode*) & s : vecServicenodeScores)
    {
        rank++;
        if(rank == nRank)
        {
            return s.second;
        }
    }

    return nullptr;
}

void CServicenodeMan::ProcessServicenodeConnections()
{
    //we don't care about this for regtest
//    if(Params().NetworkIDString() == CBaseChainParams::REGTEST) return;

    LOCK(cs_vNodes);
    for (CNode * pnode : vNodes)
    {
        if(pnode->fServicenode)
        {
//            if(darkSendPool.pSubmittedToServicenode != NULL && pnode->addr == darkSendPool.pSubmittedToServicenode->addr)
//            {
//                continue;
//            }
            printf("Closing Servicenode connection: addr=%s\n", pnode->addr.ToString().c_str());
            pnode->fDisconnect = true;
        }
    }
}

std::pair<CService, std::set<uint256> > CServicenodeMan::PopScheduledMnbRequestConnection()
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


void CServicenodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if(!servicenodeSync.IsBlockchainSynced())
    {
        return;
    }

    if (strCommand == NetMsgType::MNANNOUNCE)
    {
        //Servicenode Broadcast

        CServicenodeBroadcast mnb;
        vRecv >> mnb;

        // pfrom->setAskFor.erase(mnb.GetHash());

        printf("MNANNOUNCE -- Servicenode announce, servicenode=%s\n", mnb.vin.prevout.ToString().c_str());

        int nDos = 0;

        if (CheckMnbAndUpdateServicenodeList(pfrom, mnb, nDos))
        {
            // use announced Servicenode as a peer
            addrman.Add(CAddress(mnb.addr), pfrom->addr, 2*60*60);
        }
        else if(nDos > 0)
        {
            pfrom->Misbehaving(nDos);
        }

        if(fServicenodesAdded)
        {
            NotifyServicenodeUpdates();
        }
    }

    else if (strCommand == NetMsgType::MNPING)
    {
        //Servicenode Ping

        CServicenodePing mnp;
        vRecv >> mnp;

        uint256 nHash = mnp.GetHash();

        // pfrom->setAskFor.erase(nHash);

        printf("MNPING -- Servicenode ping, servicenode=%s\n", mnp.vin.prevout.ToString().c_str());

        // Need LOCK2 here to ensure consistent locking order because the CheckAndUpdate call below locks cs_main
        LOCK2(cs_main, cs);

        if(mapSeenServicenodePing.count(nHash))
        {
            return; //seen
        }

        mapSeenServicenodePing.insert(std::make_pair(nHash, mnp));

        printf("MNPING -- Servicenode ping, servicenode=%s new\n", mnp.vin.prevout.ToString().c_str());

        // see if we have this Servicenode
        CServicenode* pmn = mnodeman.Find(mnp.vin);

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
        // we might have to ask for a servicenode entry once
        AskForMN(pfrom, mnp.vin);

    }

    else if (strCommand == NetMsgType::DSEG)
    {
        // Get Servicenode list or specific entry

        // Ignore such requests until we are fully synced.
        // We could start processing this after servicenode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!servicenodeSync.IsSynced())
        {
            return;
        }

        CTxIn vin;
        vRecv >> vin;

        printf("DSEG -- Servicenode list, servicenode=%s\n", vin.prevout.ToString().c_str());

        LOCK(cs);

        if(vin == CTxIn())
        {
            //only should ask for this once

            //local network
            bool isLocal = (pfrom->addr.IsRFC1918() || pfrom->addr.IsLocal());

            if(!isLocal && !fTestNet)
            {
                std::map<CNetAddr, int64_t>::iterator i = mAskedUsForServicenodeList.find(pfrom->addr);
                if (i != mAskedUsForServicenodeList.end())
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
                mAskedUsForServicenodeList[pfrom->addr] = askAgain;
            }
        } //else, asking for a specific node which is ok

        int nInvCount = 0;

        for (CServicenode & mn : vServicenodes)
        {
            if (vin != CTxIn() && vin != mn.vin)
            {
                continue; // asked for specific vin but we are not there yet
            }

            if (mn.addr.IsRFC1918() || mn.addr.IsLocal())
            {
                continue; // do not send local network servicenode
            }

            if (mn.IsUpdateRequired())
            {
                continue; // do not send outdated servicenodes
            }

            printf("DSEG -- Sending Servicenode entry: servicenode=%s  addr=%s\n", mn.vin.prevout.ToString().c_str(), mn.addr.ToString().c_str());

            CServicenodeBroadcast mnb = CServicenodeBroadcast(mn);
            uint256 hash = mnb.GetHash();
            pfrom->PushInventory(CInv(MSG_SERVICENODE_ANNOUNCE, hash));
            pfrom->PushInventory(CInv(MSG_SERVICENODE_PING, mn.lastPing.GetHash()));
            nInvCount++;

            if (!mapSeenServicenodeBroadcast.count(hash))
            {
                mapSeenServicenodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));
            }

            if (vin == mn.vin)
            {
                printf("DSEG -- Sent 1 Servicenode inv to peer %s\n", pfrom->addr.ToString().c_str());
                return;
            }
        }

        if(vin == CTxIn())
        {
            pfrom->PushMessage(NetMsgType::SYNCSTATUSCOUNT, SERVICENODE_SYNC_LIST, nInvCount);
            printf("DSEG -- Sent %d Servicenode invs to peer %s\n", nInvCount, pfrom->addr.ToString().c_str());
            return;
        }

        // smth weird happen - someone asked us for vin we have no idea about?
        printf("DSEG -- No invs sent to peer %s\n", pfrom->addr.ToString().c_str());

    }

    else if (strCommand == NetMsgType::MNVERIFY)
    {
        // Servicenode Verify

        // Need LOCK2 here to ensure consistent locking order because the all functions below call GetBlockHash which locks cs_main
        LOCK2(cs_main, cs);

        CServicenodeVerification mnv;
        vRecv >> mnv;

        if(mnv.vchSig1.empty())
        {
            // CASE 1: someone asked me to verify myself /IP we are using/
            SendVerifyReply(pfrom, mnv);
        }
        else if (mnv.vchSig2.empty())
        {
            // CASE 2: we _probably_ got verification we requested from some servicenode
            ProcessVerifyReply(pfrom, mnv);
        }
        else
        {
            // CASE 3: we _probably_ got verification broadcast signed by some servicenode which verified another one
            ProcessVerifyBroadcast(pfrom, mnv);
        }
    }
}

// Verification of servicenodes via unique direct requests.

void CServicenodeMan::DoFullVerificationStep()
{
    if(activeServicenode.vin == CTxIn())
    {
        return;
    }

    if(!servicenodeSync.IsSynced())
    {
        return;
    }

    std::vector<std::pair<int, CServicenode> > vecServicenodeRanks = GetServicenodeRanks(pCurrentBlockIndex->nHeight - 1, MIN_POSE_PROTO_VERSION);

    // Need LOCK2 here to ensure consistent locking order because the SendVerifyRequest call below locks cs_main
    // through GetHeight() signal in ConnectNode
    LOCK2(cs_main, cs);

    int nCount = 0;

    int nMyRank = -1;
    int nRanksTotal = (int)vecServicenodeRanks.size();

    // send verify requests only if we are in top MAX_POSE_RANK
    std::vector<std::pair<int, CServicenode> >::iterator it = vecServicenodeRanks.begin();
    while(it != vecServicenodeRanks.end())
    {
        if(it->first > MAX_POSE_RANK)
        {
            printf("CServicenodeMan::DoFullVerificationStep -- Must be in top %d to send verify request\n",
                        (int)MAX_POSE_RANK);
            return;
        }

        if(it->second.vin == activeServicenode.vin)
        {
            nMyRank = it->first;
            printf("servicenode", "CServicenodeMan::DoFullVerificationStep -- Found self at rank %d/%d, verifying up to %d servicenodes\n",
                        nMyRank, nRanksTotal, (int)MAX_POSE_CONNECTIONS);
            break;
        }
        ++it;
    }

    // edge case: list is too short and this servicenode is not enabled
    if(nMyRank == -1)
    {
        return;
    }

    // send verify requests to up to MAX_POSE_CONNECTIONS servicenodes
    // starting from MAX_POSE_RANK + nMyRank and using MAX_POSE_CONNECTIONS as a step
    int nOffset = MAX_POSE_RANK + nMyRank - 1;
    if(nOffset >= (int)vecServicenodeRanks.size())
    {
        return;
    }

    std::vector<CServicenode*> vSortedByAddr;
    for (CServicenode & mn : vServicenodes)
    {
        vSortedByAddr.push_back(&mn);
    }

    sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

    it = vecServicenodeRanks.begin() + nOffset;
    while(it != vecServicenodeRanks.end())
    {
        if(it->second.IsPoSeVerified() || it->second.IsPoSeBanned())
        {
            printf("CServicenodeMan::DoFullVerificationStep -- Already %s%s%s servicenode %s address %s, skipping...\n",
                        it->second.IsPoSeVerified() ? "verified" : "",
                        it->second.IsPoSeVerified() && it->second.IsPoSeBanned() ? " and " : "",
                        it->second.IsPoSeBanned() ? "banned" : "",
                        it->second.vin.prevout.ToString().c_str(), it->second.addr.ToString().c_str());
            nOffset += MAX_POSE_CONNECTIONS;
            if(nOffset >= (int)vecServicenodeRanks.size())
            {
                break;
            }
            it += MAX_POSE_CONNECTIONS;
            continue;
        }
        printf("CServicenodeMan::DoFullVerificationStep -- Verifying servicenode %s rank %d/%d address %s\n",
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
        if(nOffset >= (int)vecServicenodeRanks.size())
        {
            break;
        }
        it += MAX_POSE_CONNECTIONS;
    }

    printf("CServicenodeMan::DoFullVerificationStep -- Sent verification requests to %d servicenodes\n", nCount);
}

// This function tries to find servicenodes with the same addr,
// find a verified one and ban all the other. If there are many nodes
// with the same addr but none of them is verified yet, then none of them are banned.
// It could take many times to run this before most of the duplicate nodes are banned.

void CServicenodeMan::CheckSameAddr()
{
    if(!servicenodeSync.IsSynced() || vServicenodes.empty())
    {
        return;
    }

    std::vector<CServicenode*> vBan;
    std::vector<CServicenode*> vSortedByAddr;

    {
        LOCK(cs);

        CServicenode* pprevServicenode = NULL;
        CServicenode* pverifiedServicenode = NULL;

        for (CServicenode&  mn : vServicenodes)
        {
            vSortedByAddr.push_back(&mn);
        }

        sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

        for (CServicenode * pmn : vSortedByAddr)
        {
            // check only (pre)enabled servicenodes
            if(!pmn->IsEnabled() && !pmn->IsPreEnabled())
            {
                continue;
            }
            // initial step
            if(!pprevServicenode)
            {
                pprevServicenode = pmn;
                pverifiedServicenode = pmn->IsPoSeVerified() ? pmn : NULL;
                continue;
            }
            // second+ step
            if(pmn->addr == pprevServicenode->addr)
            {
                if(pverifiedServicenode)
                {
                    // another servicenode with the same ip is verified, ban this one
                    vBan.push_back(pmn);
                }
                else if(pmn->IsPoSeVerified())
                {
                    // this servicenode with the same ip is verified, ban previous one
                    vBan.push_back(pprevServicenode);
                    // and keep a reference to be able to ban following servicenodes with the same ip
                    pverifiedServicenode = pmn;
                }
            }
            else
            {
                pverifiedServicenode = pmn->IsPoSeVerified() ? pmn : NULL;
            }
            pprevServicenode = pmn;
        }
    }

    // ban duplicates
    for (CServicenode * pmn : vBan)
    {
        printf("CServicenodeMan::CheckSameAddr -- increasing PoSe ban score for servicenode %s\n", pmn->vin.prevout.ToString().c_str());
        pmn->IncreasePoSeBanScore();
    }
}

bool CServicenodeMan::SendVerifyRequest(const CAddress& addr, const std::vector<CServicenode*>& vSortedByAddr)
{
    if(netfulfilledman.HasFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request"))
    {
        // we already asked for verification, not a good idea to do this too often, skip it
        printf("CServicenodeMan::SendVerifyRequest -- too many requests, skipping... addr=%s\n", addr.ToString().c_str());
        return false;
    }

    CNode* pnode = ConnectNode(addr, NULL, true);
    if(pnode == NULL)
    {
        printf("CServicenodeMan::SendVerifyRequest -- can't connect to node to verify it, addr=%s\n", addr.ToString().c_str());
        return false;
    }

    netfulfilledman.AddFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request");
    // use random nonce, store it and require node to reply with correct one later
    CServicenodeVerification mnv(addr, GetRandInt(999999), pCurrentBlockIndex->nHeight - 1);
    mWeAskedForVerification[addr] = mnv;
    printf("CServicenodeMan::SendVerifyRequest -- verifying node using nonce %d addr=%s\n", mnv.nonce, addr.ToString().c_str());
    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);

    return true;
}

void CServicenodeMan::SendVerifyReply(CNode* pnode, CServicenodeVerification& mnv)
{
    // only servicenodes can sign this, why would someone ask regular node?
    if(!fServiceNode)
    {
        // do not ban, malicious node might be using my IP
        // and trying to confuse the node which tries to verify it
        return;
    }

    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply"))
    {
        // peer should not ask us that often
        printf("ServicenodeMan::SendVerifyReply -- ERROR: peer already asked me recently, peer=%s\n", pnode->addr.ToString().c_str());
        pnode->Misbehaving(20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight))
    {
        printf("ServicenodeMan::SendVerifyReply -- can't get block hash for unknown block height %d, peer=%s\n", mnv.nBlockHeight, pnode->addr.ToString().c_str());
        return;
    }

    std::string strMessage = strprintf("%s%d%s", activeServicenode.service.ToString(), mnv.nonce, blockHash.ToString());

    if (!DataSigner::SignMessage(strMessage, mnv.vchSig1, activeServicenode.keyServicenode))
    {
        printf("ServicenodeMan::SendVerifyReply -- SignMessage() failed\n");
        return;
    }

    std::string strError;

    if (!DataSigner::VerifyMessage(activeServicenode.pubKeyServicenode, mnv.vchSig1, strMessage, strError))
    {
        printf("ServicenodeMan::SendVerifyReply -- VerifyMessage() failed, error: %s\n", strError.c_str());
        return;
    }

    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);
    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply");
}

void CServicenodeMan::ProcessVerifyReply(CNode* pnode, CServicenodeVerification& mnv)
{
    std::string strError;

    // did we even ask for it? if that's the case we should have matching fulfilled request
    if(!netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request"))
    {
        printf("CServicenodeMan::ProcessVerifyReply -- ERROR: we didn't ask for verification of %s\n", pnode->addr.ToString().c_str());
        pnode->Misbehaving(20);
        return;
    }

    // Received nonce for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nonce != mnv.nonce)
    {
        printf("CServicenodeMan::ProcessVerifyReply -- ERROR: wrong nounce: requested=%d, received=%d, peer=%s\n",
                    mWeAskedForVerification[pnode->addr].nonce, mnv.nonce, pnode->addr.ToString().c_str());
        pnode->Misbehaving(20);
        return;
    }

    // Received nBlockHeight for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nBlockHeight != mnv.nBlockHeight)
    {
        printf("CServicenodeMan::ProcessVerifyReply -- ERROR: wrong nBlockHeight: requested=%d, received=%d, peer=%s\n",
                    mWeAskedForVerification[pnode->addr].nBlockHeight, mnv.nBlockHeight, pnode->addr.ToString().c_str());
        pnode->Misbehaving(20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight))
    {
        // this shouldn't happen...
        printf("ServicenodeMan::ProcessVerifyReply -- can't get block hash for unknown block height %d, peer=%s\n", mnv.nBlockHeight, pnode->addr.ToString().c_str());
        return;
    }

    // we already verified this address, why node is spamming?
    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done"))
    {
        printf("CServicenodeMan::ProcessVerifyReply -- ERROR: already verified %s recently\n", pnode->addr.ToString().c_str());
        pnode->Misbehaving(20);
        return;
    }

    {
        LOCK(cs);

        CServicenode* prealServicenode = NULL;
        std::vector<CServicenode*> vpServicenodesToBan;
        std::vector<CServicenode>::iterator it = vServicenodes.begin();
        std::string strMessage1 = strprintf("%s%d%s", pnode->addr.ToString(), mnv.nonce, blockHash.ToString());
        while(it != vServicenodes.end())
        {
            if((CAddress)it->addr == pnode->addr)
            {
                if (DataSigner::VerifyMessage(it->pubKeyServicenode, mnv.vchSig1, strMessage1, strError))
                {
                    // found it!
                    prealServicenode = &(*it);
                    if(!it->IsPoSeVerified())
                    {
                        it->DecreasePoSeBanScore();
                    }
                    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done");

                    // we can only broadcast it if we are an activated servicenode
                    if(activeServicenode.vin == CTxIn())
                    {
                        continue;
                    }
                    // update ...
                    mnv.addr = it->addr;
                    mnv.vin1 = it->vin;
                    mnv.vin2 = activeServicenode.vin;
                    std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                            mnv.vin1.prevout.ToString(), mnv.vin2.prevout.ToString());
                    // ... and sign it
                    if (!DataSigner::SignMessage(strMessage2, mnv.vchSig2, activeServicenode.keyServicenode))
                    {
                        printf("ServicenodeMan::ProcessVerifyReply -- SignMessage() failed\n");
                        return;
                    }

                    std::string strError;

                    if (!DataSigner::VerifyMessage(activeServicenode.pubKeyServicenode, mnv.vchSig2, strMessage2, strError))
                    {
                        printf("ServicenodeMan::ProcessVerifyReply -- VerifyMessage() failed, error: %s\n", strError.c_str());
                        return;
                    }

                    mWeAskedForVerification[pnode->addr] = mnv;
                    mnv.Relay();

                }
                else
                {
                    vpServicenodesToBan.push_back(&(*it));
                }
            }
            ++it;
        }
        // no real servicenode found?...
        if(!prealServicenode)
        {
            // this should never be the case normally,
            // only if someone is trying to game the system in some way or smth like that
            printf("CServicenodeMan::ProcessVerifyReply -- ERROR: no real servicenode found for addr %s\n", pnode->addr.ToString().c_str());
            pnode->Misbehaving(20);
            return;
        }
        printf("CServicenodeMan::ProcessVerifyReply -- verified real servicenode %s for addr %s\n",
                    prealServicenode->vin.prevout.ToString().c_str(), pnode->addr.ToString().c_str());
        // increase ban score for everyone else
        for (CServicenode * pmn : vpServicenodesToBan)
        {
            pmn->IncreasePoSeBanScore();
            printf("CServicenodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        prealServicenode->vin.prevout.ToString().c_str(), pnode->addr.ToString().c_str(), pmn->nPoSeBanScore);
        }
        printf("CServicenodeMan::ProcessVerifyBroadcast -- PoSe score increased for %d fake servicenodes, addr %s\n",
                    (int)vpServicenodesToBan.size(), pnode->addr.ToString().c_str());
    }
}

void CServicenodeMan::ProcessVerifyBroadcast(CNode* pnode, const CServicenodeVerification& mnv)
{
    std::string strError;

    if(mapSeenServicenodeVerification.find(mnv.GetHash()) != mapSeenServicenodeVerification.end())
    {
        // we already have one
        return;
    }
    mapSeenServicenodeVerification[mnv.GetHash()] = mnv;

    // we don't care about history
    if(mnv.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS)
    {
        printf("ServicenodeMan::ProcessVerifyBroadcast -- Outdated: current block %d, verification block %d, peer=%s\n",
                    pCurrentBlockIndex->nHeight, mnv.nBlockHeight, pnode->addr.ToString().c_str());
        return;
    }

    if(mnv.vin1.prevout == mnv.vin2.prevout)
    {
        printf("servicenode", "ServicenodeMan::ProcessVerifyBroadcast -- ERROR: same vins %s, peer=%s\n",
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
        printf("ServicenodeMan::ProcessVerifyBroadcast -- Can't get block hash for unknown block height %d, peer=%s\n", mnv.nBlockHeight, pnode->addr.ToString().c_str());
        return;
    }

    int nRank = GetServicenodeRank(mnv.vin2, mnv.nBlockHeight, MIN_POSE_PROTO_VERSION);

    if (nRank == -1)
    {
        printf("CServicenodeMan::ProcessVerifyBroadcast -- Can't calculate rank for servicenode %s\n",
                    mnv.vin2.prevout.ToString().c_str());
        return;
    }

    if(nRank > MAX_POSE_RANK)
    {
        printf("CServicenodeMan::ProcessVerifyBroadcast -- Mastrernode %s is not in top %d, current rank %d, peer=%s\n",
                    mnv.vin2.prevout.ToString().c_str(), (int)MAX_POSE_RANK, nRank, pnode->addr.ToString().c_str());
        return;
    }

    {
        LOCK(cs);

        std::string strMessage1 = strprintf("%s%d%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString());
        std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                mnv.vin1.prevout.ToString(), mnv.vin2.prevout.ToString());

        CServicenode* pmn1 = Find(mnv.vin1);
        if(!pmn1)
        {
            printf("CServicenodeMan::ProcessVerifyBroadcast -- can't find servicenode1 %s\n", mnv.vin1.prevout.ToString().c_str());
            return;
        }

        CServicenode* pmn2 = Find(mnv.vin2);
        if(!pmn2)
        {
            printf("CServicenodeMan::ProcessVerifyBroadcast -- can't find servicenode2 %s\n", mnv.vin2.prevout.ToString().c_str());
            return;
        }

        if(pmn1->addr != mnv.addr)
        {
            printf("CServicenodeMan::ProcessVerifyBroadcast -- addr %s do not match %s\n", mnv.addr.ToString().c_str(), pnode->addr.ToString().c_str());
            return;
        }

        if (DataSigner::VerifyMessage(pmn1->pubKeyServicenode, mnv.vchSig1, strMessage1, strError))
        {
            printf("ServicenodeMan::ProcessVerifyBroadcast -- VerifyMessage() for servicenode1 failed, error: %s\n", strError.c_str());
            return;
        }

        if (DataSigner::VerifyMessage(pmn2->pubKeyServicenode, mnv.vchSig2, strMessage2, strError))
        {
            printf("ServicenodeMan::ProcessVerifyBroadcast -- VerifyMessage() for servicenode2 failed, error: %s\n", strError.c_str());
            return;
        }

        if(!pmn1->IsPoSeVerified())
        {
            pmn1->DecreasePoSeBanScore();
        }
        mnv.Relay();

        printf("CServicenodeMan::ProcessVerifyBroadcast -- verified servicenode %s for addr %s\n",
                    pmn1->vin.prevout.ToString().c_str(), pnode->addr.ToString().c_str());

        // increase ban score for everyone else with the same addr
        int nCount = 0;
        for (CServicenode & mn : vServicenodes)
        {
            if(mn.addr != mnv.addr || mn.vin.prevout == mnv.vin1.prevout)
            {
                continue;
            }
            mn.IncreasePoSeBanScore();
            nCount++;
            printf("CServicenodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        mn.vin.prevout.ToString().c_str(), mn.addr.ToString().c_str(), mn.nPoSeBanScore);
        }
        printf("CServicenodeMan::ProcessVerifyBroadcast -- PoSe score incresed for %d fake servicenodes, addr %s\n",
                    nCount, pnode->addr.ToString().c_str());
    }
}

std::string CServicenodeMan::ToString() const
{
    std::ostringstream info;

    info << "Servicenodes: " << (int)vServicenodes.size() <<
            ", peers who asked us for Servicenode list: " << (int)mAskedUsForServicenodeList.size() <<
            ", peers we asked for Servicenode list: " << (int)mWeAskedForServicenodeList.size() <<
            ", entries in Servicenode list we asked for: " << (int)mWeAskedForServicenodeListEntry.size() <<
            ", servicenode index size: " << indexServicenodes.GetSize() <<
            ", nDsqCount: " << (int)nDsqCount;

    return info.str();
}

void CServicenodeMan::UpdateServicenodeList(CServicenodeBroadcast mnb)
{
    LOCK(cs);
    mapSeenServicenodePing.insert(std::make_pair(mnb.lastPing.GetHash(), mnb.lastPing));
    mapSeenServicenodeBroadcast.insert(std::make_pair(mnb.GetHash(), std::make_pair(GetTime(), mnb)));

    printf("CServicenodeMan::UpdateServicenodeList -- servicenode=%s  addr=%s\n", mnb.vin.prevout.ToString().c_str(), mnb.addr.ToString().c_str());

    CServicenode* pmn = Find(mnb.vin);
    if(pmn == NULL)
    {
        CServicenode mn(mnb);
        if(Add(mn))
        {
            servicenodeSync.AddedServicenodeList();
        }
    }
    else
    {
        CServicenodeBroadcast mnbOld = mapSeenServicenodeBroadcast[CServicenodeBroadcast(*pmn).GetHash()].second;
        if(pmn->UpdateFromNewBroadcast(mnb))
        {
            servicenodeSync.AddedServicenodeList();
            mapSeenServicenodeBroadcast.erase(mnbOld.GetHash());
        }
    }
}

bool CServicenodeMan::CheckMnbAndUpdateServicenodeList(CNode* pfrom, CServicenodeBroadcast mnb, int& nDos)
{
    // Need LOCK2 here to ensure consistent locking order because the SimpleCheck call below locks cs_main
    LOCK2(cs_main, cs);

    nDos = 0;
    printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- servicenode=%s\n", mnb.vin.prevout.ToString().c_str());

    uint256 hash = mnb.GetHash();
    if(mapSeenServicenodeBroadcast.count(hash) && !mnb.fRecovery)
    {
        //seen
        printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- servicenode=%s seen\n", mnb.vin.prevout.ToString().c_str());
        // less then 2 pings left before this MN goes into non-recoverable state, bump sync timeout
        if(GetTime() - mapSeenServicenodeBroadcast[hash].first > SERVICENODE_NEW_START_REQUIRED_SECONDS - SERVICENODE_MIN_MNP_SECONDS * 2)
        {
            printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- servicenode=%s seen update\n", mnb.vin.prevout.ToString().c_str());
            mapSeenServicenodeBroadcast[hash].first = GetTime();
            servicenodeSync.AddedServicenodeList();
        }
        // did we ask this node for it?
        if(pfrom && IsMnbRecoveryRequested(hash) && GetTime() < mMnbRecoveryRequests[hash].first)
        {
            printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- mnb=%s seen request\n", hash.ToString().c_str());
            if(mMnbRecoveryRequests[hash].second.count(pfrom->addr))
            {
                printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- mnb=%s seen request, addr=%s\n", hash.ToString().c_str(), pfrom->addr.ToString().c_str());
                // do not allow node to send same mnb multiple times in recovery mode
                mMnbRecoveryRequests[hash].second.erase(pfrom->addr);
                // does it have newer lastPing?
                if(mnb.lastPing.sigTime > mapSeenServicenodeBroadcast[hash].second.lastPing.sigTime)
                {
                    // simulate Check
                    CServicenode mnTemp = CServicenode(mnb);
                    mnTemp.Check();
                    printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- mnb=%s seen request, addr=%s, better lastPing: %d min ago, projected mn state: %s\n",
                           hash.ToString().c_str(), pfrom->addr.ToString().c_str(), (GetTime() - mnb.lastPing.sigTime)/60, mnTemp.GetStateString().c_str());
                    if(mnTemp.IsValidStateForAutoStart(mnTemp.nActiveState))
                    {
                        // this node thinks it's a good one
                        printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- servicenode=%s seen good\n", mnb.vin.prevout.ToString().c_str());
                        mMnbRecoveryGoodReplies[hash].push_back(mnb);
                    }
                }
            }
        }
        return true;
    }
    mapSeenServicenodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));

    printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- servicenode=%s new\n", mnb.vin.prevout.ToString().c_str());

    if(!mnb.SimpleCheck(nDos))
    {
        printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- SimpleCheck() failed, servicenode=%s\n", mnb.vin.prevout.ToString().c_str());
        return false;
    }

    // search Servicenode list
    CServicenode* pmn = Find(mnb.vin);
    if(pmn)
    {
        CServicenodeBroadcast mnbOld = mapSeenServicenodeBroadcast[CServicenodeBroadcast(*pmn).GetHash()].second;
        if(!mnb.Update(pmn, nDos))
        {
            printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- Update() failed, servicenode=%s\n", mnb.vin.prevout.ToString().c_str());
            return false;
        }
        if(hash != mnbOld.GetHash())
        {
           mapSeenServicenodeBroadcast.erase(mnbOld.GetHash());
        }
    }
    else
    {
        if(mnb.CheckOutpoint(nDos))
        {
            Add(mnb);
            servicenodeSync.AddedServicenodeList();
            // if it matches our Servicenode privkey...
            if(fServiceNode && mnb.pubKeyServicenode == activeServicenode.pubKeyServicenode)
            {
                mnb.nPoSeBanScore = -SERVICENODE_POSE_BAN_MAX_SCORE;
                if(mnb.nProtocolVersion == PROTOCOL_VERSION)
                {
                    // ... and PROTOCOL_VERSION, then we've been remotely activated ...
                    printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- Got NEW Servicenode entry: servicenode=%s  sigTime=%lld  addr=%s\n",
                                mnb.vin.prevout.ToString().c_str(), mnb.sigTime, mnb.addr.ToString().c_str());
                    activeServicenode.ManageState();
                }
                else
                {
                    // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
                    // but also do not ban the node we get this message from
                    printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", mnb.nProtocolVersion, PROTOCOL_VERSION);
                    return false;
                }
            }
            mnb.Relay();
        }
        else
        {
            printf("CServicenodeMan::CheckMnbAndUpdateServicenodeList -- Rejected Servicenode entry: %s  addr=%s\n", mnb.vin.prevout.ToString().c_str(), mnb.addr.ToString().c_str());
            return false;
        }
    }

    return true;
}

void CServicenodeMan::UpdateLastPaid()
{
    LOCK(cs);

    if(!pCurrentBlockIndex)
    {
        return;
    }

    static bool IsFirstRun = true;
    // Do full scan on first run or if we are not a servicenode
    // (MNs should update this info on every block, so limited scan should be enough for them)
    int nMaxBlocksToScanBack = (IsFirstRun || !fServiceNode) ? mnpayments.GetStorageLimit() : LAST_PAID_SCAN_BLOCKS;

    // LogPrint("mnpayments", "CServicenodeMan::UpdateLastPaid -- nHeight=%d, nMaxBlocksToScanBack=%d, IsFirstRun=%s\n",
    //                         pCurrentBlockIndex->nHeight, nMaxBlocksToScanBack, IsFirstRun ? "true" : "false");

    for (CServicenode & mn : vServicenodes)
    {
        mn.UpdateLastPaid(pCurrentBlockIndex, nMaxBlocksToScanBack);
    }

    // every time is like the first time if winners list is not synced
    IsFirstRun = !servicenodeSync.IsWinnersListSynced();
}

void CServicenodeMan::CheckAndRebuildServicenodeIndex()
{
    LOCK(cs);

    if(GetTime() - nLastIndexRebuildTime < MIN_INDEX_REBUILD_TIME) {
        return;
    }

    if(indexServicenodes.GetSize() <= MAX_EXPECTED_INDEX_SIZE) {
        return;
    }

    if(indexServicenodes.GetSize() <= int(vServicenodes.size())) {
        return;
    }

    indexServicenodesOld = indexServicenodes;
    indexServicenodes.Clear();
    for(size_t i = 0; i < vServicenodes.size(); ++i) {
        indexServicenodes.AddServicenodeVIN(vServicenodes[i].vin);
    }

    fIndexRebuilt = true;
    nLastIndexRebuildTime = GetTime();
}

void CServicenodeMan::UpdateWatchdogVoteTime(const CTxIn& vin)
{
    LOCK(cs);
    CServicenode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->UpdateWatchdogVoteTime();
    nLastWatchdogVoteTime = GetTime();
}

bool CServicenodeMan::IsWatchdogActive()
{
    LOCK(cs);
    // Check if any servicenodes have voted recently, otherwise return false
    return (GetTime() - nLastWatchdogVoteTime) <= SERVICENODE_WATCHDOG_MAX_SECONDS;
}

bool CServicenodeMan::AddGovernanceVote(const CTxIn& vin, uint256 nGovernanceObjectHash)
{
    LOCK(cs);
    CServicenode* pMN = Find(vin);
    if(!pMN)  {
        return false;
    }
    pMN->AddGovernanceVote(nGovernanceObjectHash);
    return true;
}

void CServicenodeMan::RemoveGovernanceObject(uint256 nGovernanceObjectHash)
{
//    LOCK(cs);
//    BOOST_FOREACH(CServicenode& mn, vServicenodes) {
//        mn.RemoveGovernanceObject(nGovernanceObjectHash);
//    }
}

void CServicenodeMan::CheckServicenode(const CTxIn& vin, bool fForce)
{
    LOCK(cs);
    CServicenode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

void CServicenodeMan::CheckServicenode(const CPubKey& pubKeyServicenode, bool fForce)
{
    LOCK(cs);
    CServicenode* pMN = Find(pubKeyServicenode);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

int CServicenodeMan::GetServicenodeState(const CTxIn& vin)
{
    LOCK(cs);
    CServicenode* pMN = Find(vin);
    if(!pMN)  {
        return CServicenode::SERVICENODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

int CServicenodeMan::GetServicenodeState(const CPubKey& pubKeyServicenode)
{
    LOCK(cs);
    CServicenode* pMN = Find(pubKeyServicenode);
    if(!pMN)  {
        return CServicenode::SERVICENODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

bool CServicenodeMan::IsServicenodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt)
{
    LOCK(cs);
    CServicenode* pMN = Find(vin);
    if(!pMN) {
        return false;
    }
    return pMN->IsPingedWithin(nSeconds, nTimeToCheckAt);
}

void CServicenodeMan::SetServicenodeLastPing(const CTxIn& vin, const CServicenodePing& mnp)
{
    LOCK(cs);
    CServicenode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->lastPing = mnp;
    mapSeenServicenodePing.insert(std::make_pair(mnp.GetHash(), mnp));

    CServicenodeBroadcast mnb(*pMN);
    uint256 hash = mnb.GetHash();
    if(mapSeenServicenodeBroadcast.count(hash)) {
        mapSeenServicenodeBroadcast[hash].second.lastPing = mnp;
    }
}

void CServicenodeMan::UpdatedBlockTip(const CBlockIndex *pindex)
{
    pCurrentBlockIndex = pindex;
    printf("CServicenodeMan::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);

    CheckSameAddr();

    if(fServiceNode)
    {
        // normal wallet does not need to update this every block, doing update on rpc call should be enough
        UpdateLastPaid();
    }
}

void CServicenodeMan::NotifyServicenodeUpdates()
{
    // Avoid double locking
//    bool fServicenodesAddedLocal = false;
//    bool fServicenodesRemovedLocal = false;
//    {
//        LOCK(cs);
//        fServicenodesAddedLocal = fServicenodesAdded;
//        fServicenodesRemovedLocal = fServicenodesRemoved;
//    }

//    if(fServicenodesAddedLocal) {
//        governance.CheckServicenodeOrphanObjects();
//        governance.CheckServicenodeOrphanVotes();
//    }
//    if(fServicenodesRemovedLocal) {
//        governance.UpdateCachesAndClean();
//    }

    LOCK(cs);
    fServicenodesAdded = false;
    fServicenodesRemoved = false;
}
