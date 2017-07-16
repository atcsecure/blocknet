// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef SERVICENODE_SYNC_H
#define SERVICENODE_SYNC_H

#include "serialize.h"
// #include "chain.h"
#include "net.h"

// #include <univalue.h>

class CServicenodeSync;

static const int SERVICENODE_SYNC_FAILED          = -1;
static const int SERVICENODE_SYNC_INITIAL         = 0;
// static const int SERVICENODE_SYNC_SPORKS          = 1;
static const int SERVICENODE_SYNC_LIST            = 2;
static const int SERVICENODE_SYNC_MNW             = 3;
// static const int SERVICENODE_SYNC_GOVERNANCE      = 4;
// static const int SERVICENODE_SYNC_GOVOBJ          = 10;
// static const int SERVICENODE_SYNC_GOVOBJ_VOTE     = 11;
static const int SERVICENODE_SYNC_FINISHED        = 999;

static const int SERVICENODE_SYNC_TICK_SECONDS    = 6;
static const int SERVICENODE_SYNC_TIMEOUT_SECONDS = 30; // our blocks are 2.5 minutes so 30 seconds should be fine

static const int SERVICENODE_SYNC_ENOUGH_PEERS    = 6;

extern CServicenodeSync servicenodeSync;

//
// CServicenodeSync : Sync servicenode assets in stages
//

class CServicenodeSync
{
private:
    // Keep track of current asset
    int nRequestedServicenodeAssets;
    // Count peers we've requested the asset from
    int nRequestedServicenodeAttempt;

    // Time when current servicenode asset sync started
    int64_t nTimeAssetSyncStarted;

    // Last time when we received some servicenode asset ...
    int64_t nTimeLastServicenodeList;
    int64_t nTimeLastPaymentVote;
    int64_t nTimeLastGovernanceItem;
    // ... or failed
    int64_t nTimeLastFailure;

    // How many times we failed
    int nCountFailures;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    void Fail();
    void ClearFulfilledRequests();

public:
    CServicenodeSync() { Reset(); }

    void AddedServicenodeList() { nTimeLastServicenodeList = GetTime(); }
    void AddedPaymentVote() { nTimeLastPaymentVote = GetTime(); }
    void AddedGovernanceItem() { nTimeLastGovernanceItem = GetTime(); };

    void SendGovernanceSyncRequest(CNode* pnode);

    bool IsFailed() { return nRequestedServicenodeAssets == SERVICENODE_SYNC_FAILED; }
    bool IsBlockchainSynced(bool fBlockAccepted = false);
    bool IsServicenodeListSynced() { return nRequestedServicenodeAssets > SERVICENODE_SYNC_LIST; }
    bool IsWinnersListSynced() { return nRequestedServicenodeAssets > SERVICENODE_SYNC_MNW; }
    bool IsSynced() { return nRequestedServicenodeAssets == SERVICENODE_SYNC_FINISHED; }

    int GetAssetID() { return nRequestedServicenodeAssets; }
    int GetAttempt() { return nRequestedServicenodeAttempt; }
    std::string GetAssetName();
    std::string GetSyncStatus();

    void Reset();
    void SwitchToNextAsset();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    void ProcessTick();

    void UpdatedBlockTip(const CBlockIndex *pindex);
};

#endif
