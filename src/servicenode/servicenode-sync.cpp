// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeservicenode.h"
#include "checkpoints.h"
// #include "governance.h"
#include "main.h"
#include "servicenode.h"
#include "servicenode-payments.h"
#include "servicenode-sync.h"
#include "servicenodeman.h"
#include "netfulfilledman.h"
// #include "spork.h"
#include "util.h"

class CServicenodeSync;
CServicenodeSync servicenodeSync;

bool CServicenodeSync::IsBlockchainSynced(bool fBlockAccepted)
{
    static bool fBlockchainSynced = false;
    static int64_t nTimeLastProcess = GetTime();
    static int nSkipped = 0;
    static bool fFirstBlockAccepted = false;

    // if the last call to this function was more than 60 minutes ago (client was in sleep mode) reset the sync process
    if(GetTime() - nTimeLastProcess > 60*60)
    {
        Reset();
        fBlockchainSynced = false;
    }

    if(!pCurrentBlockIndex) // || !pindexBestHeader) //  || fImporting || fReindex)
    {
        return false;
    }

    if(fBlockAccepted)
    {
        // this should be only triggered while we are still syncing
        if(!IsSynced())
        {
            // we are trying to download smth, reset blockchain sync status
            if(fDebug)
            {
                printf("CServicenodeSync::IsBlockchainSynced -- reset\n");
            }
            fFirstBlockAccepted = true;
            fBlockchainSynced = false;
            nTimeLastProcess = GetTime();
            return false;
        }
    }
    else
    {
        // skip if we already checked less than 1 tick ago
        if(GetTime() - nTimeLastProcess < SERVICENODE_SYNC_TICK_SECONDS)
        {
            nSkipped++;
            return fBlockchainSynced;
        }
    }

    if(fDebug)
    {
        printf("CServicenodeSync::IsBlockchainSynced -- state before check: %ssynced, skipped %d times\n",
               fBlockchainSynced ? "" : "not ", nSkipped);
    }

    nTimeLastProcess = GetTime();
    nSkipped = 0;

    if(fBlockchainSynced)
    {
        return true;
    }

//    if(fCheckpointsEnabled && pCurrentBlockIndex->nHeight < Checkpoints::GetTotalBlocksEstimate(Params().Checkpoints()))
//    {
//        return false;
//    }

    // wait for at least one new block to be accepted
    if(!fFirstBlockAccepted)
    {
        return false;
    }

    // same as !IsInitialBlockDownload() but no cs_main needed here
    // int64_t nMaxBlockTime = std::max(pCurrentBlockIndex->GetBlockTime(), pindexBest->GetBlockTime());
    // fBlockchainSynced =  (pindexBest->nHeight - pCurrentBlockIndex->nHeight) < (24 * 6); // && GetTime() - nMaxBlockTime < Params().MaxTipAge();
    fBlockchainSynced = (GetTime() - pCurrentBlockIndex->GetBlockTime()) < 90*60;

    return fBlockchainSynced;
}

void CServicenodeSync::Fail()
{
    nTimeLastFailure = GetTime();
    nRequestedServicenodeAssets = SERVICENODE_SYNC_FAILED;
}

void CServicenodeSync::Reset()
{
    nRequestedServicenodeAssets = SERVICENODE_SYNC_INITIAL;
    nRequestedServicenodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
    nTimeLastServicenodeList = GetTime();
    nTimeLastPaymentVote = GetTime();
    nTimeLastGovernanceItem = GetTime();
    nTimeLastFailure = 0;
    nCountFailures = 0;
}

std::string CServicenodeSync::GetAssetName()
{
    switch(nRequestedServicenodeAssets)
    {
        case(SERVICENODE_SYNC_INITIAL):      return "SERVICENODE_SYNC_INITIAL";
        // case(SERVICENODE_SYNC_SPORKS):       return "SERVICENODE_SYNC_SPORKS";
        case(SERVICENODE_SYNC_LIST):         return "SERVICENODE_SYNC_LIST";
        case(SERVICENODE_SYNC_MNW):          return "SERVICENODE_SYNC_MNW";
        // case(SERVICENODE_SYNC_GOVERNANCE):   return "SERVICENODE_SYNC_GOVERNANCE";
        case(SERVICENODE_SYNC_FAILED):       return "SERVICENODE_SYNC_FAILED";
        case SERVICENODE_SYNC_FINISHED:      return "SERVICENODE_SYNC_FINISHED";
        default:                            return "UNKNOWN";
    }
}

void CServicenodeSync::SwitchToNextAsset()
{
    switch(nRequestedServicenodeAssets)
    {
        case(SERVICENODE_SYNC_FAILED):
            throw std::runtime_error("Can't switch to next asset from failed, should use Reset() first!");
            break;
        case(SERVICENODE_SYNC_INITIAL):
            ClearFulfilledRequests();
//            nRequestedServicenodeAssets = SERVICENODE_SYNC_SPORKS;
//            printf("CServicenodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName().c_str());
//            break;
//        case(SERVICENODE_SYNC_SPORKS):
            nTimeLastServicenodeList = GetTime();
            nRequestedServicenodeAssets = SERVICENODE_SYNC_LIST;
            printf("CServicenodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName().c_str());
            break;
        case(SERVICENODE_SYNC_LIST):
            nTimeLastPaymentVote = GetTime();
            nRequestedServicenodeAssets = SERVICENODE_SYNC_MNW;
            printf("CServicenodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName().c_str());
            break;
        case(SERVICENODE_SYNC_MNW):
//            nTimeLastGovernanceItem = GetTime();
//            nRequestedServicenodeAssets = SERVICENODE_SYNC_GOVERNANCE;
//            printf("CServicenodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName().c_str());
//            break;
//        case(SERVICENODE_SYNC_GOVERNANCE):
            printf("CServicenodeSync::SwitchToNextAsset -- Sync has finished\n");
            nRequestedServicenodeAssets = SERVICENODE_SYNC_FINISHED;
            uiInterface.NotifyAdditionalDataSyncProgressChanged(1);
            //try to activate our servicenode if possible
            activeServicenode.ManageState();

            TRY_LOCK(cs_vNodes, lockRecv);
            if(!lockRecv)
            {
                return;
            }

            for (CNode * pnode : vNodes)
            {
                netfulfilledman.AddFulfilledRequest(pnode->addr, "full-sync");
            }

            break;
    }
    nRequestedServicenodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
}

std::string CServicenodeSync::GetSyncStatus()
{
    switch (servicenodeSync.nRequestedServicenodeAssets) {
        case SERVICENODE_SYNC_INITIAL:       return _("Synchronization pending...");
        // case SERVICENODE_SYNC_SPORKS:        return _("Synchronizing sporks...");
        case SERVICENODE_SYNC_LIST:          return _("Synchronizing servicenodes...");
        case SERVICENODE_SYNC_MNW:           return _("Synchronizing servicenode payments...");
        // case SERVICENODE_SYNC_GOVERNANCE:    return _("Synchronizing governance objects...");
        case SERVICENODE_SYNC_FAILED:        return _("Synchronization failed");
        case SERVICENODE_SYNC_FINISHED:      return _("Synchronization finished");
        default:                            return "";
    }
}

void CServicenodeSync::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (strCommand == NetMsgType::SYNCSTATUSCOUNT)
    {
        //Sync status count

        //do not care about stats if sync process finished or failed
        if(IsSynced() || IsFailed())
        {
            return;
        }

        int nItemID;
        int nCount;
        vRecv >> nItemID >> nCount;

        printf("SYNCSTATUSCOUNT -- got inventory count: nItemID=%d  nCount=%d  peer=%s\n",
               nItemID, nCount, pfrom->addr.ToString().c_str());
    }
}

void CServicenodeSync::ClearFulfilledRequests()
{
    TRY_LOCK(cs_vNodes, lockRecv);
    if(!lockRecv)
    {
        return;
    }

    for (CNode * pnode : vNodes)
    {
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "spork-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "servicenode-list-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "servicenode-payment-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "governance-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "full-sync");
    }
}

void CServicenodeSync::ProcessTick()
{
    static int nTick = 0;
    if(nTick++ % SERVICENODE_SYNC_TICK_SECONDS != 0)
    {
        return;
    }
    if(!pCurrentBlockIndex)
    {
        return;
    }

    //the actual count of servicenodes we have currently
    int nMnCount = mnodeman.CountServicenodes();

    if(fDebug)
    {
        printf("CServicenodeSync::ProcessTick -- nTick %d nMnCount %d\n", nTick, nMnCount);
    }

    // RESET SYNCING INCASE OF FAILURE
    {
        if(IsSynced())
        {
            /*
                Resync if we lost all servicenodes from sleep/wake or failed to sync originally
            */
            if(nMnCount == 0)
            {
                printf("CServicenodeSync::ProcessTick -- WARNING: not enough data, restarting sync\n");
                Reset();
            }
            else
            {
                // std::vector<CNode*> vNodesCopy = CopyNodeVector();
                // governance.RequestGovernanceObjectVotes(vNodesCopy);
                // ReleaseNodeVector(vNodesCopy);
                return;
            }
        }

        //try syncing again
        if(IsFailed())
        {
            if(nTimeLastFailure + (1*60) < GetTime())
            {
                // 1 minute cooldown after failed sync
                Reset();
            }
            return;
        }
    }

    // INITIAL SYNC SETUP / LOG REPORTING
    double nSyncProgress = double(nRequestedServicenodeAttempt + (nRequestedServicenodeAssets - 1) * 8) / (8*4);
    printf("CServicenodeSync::ProcessTick -- nTick %d nRequestedServicenodeAssets %d nRequestedServicenodeAttempt %d nSyncProgress %f\n",
           nTick, nRequestedServicenodeAssets, nRequestedServicenodeAttempt, nSyncProgress);
    uiInterface.NotifyAdditionalDataSyncProgressChanged(nSyncProgress);

    // sporks synced but blockchain is not, wait until we're almost at a recent block to continue
    // if (!IsBlockchainSynced() && nRequestedServicenodeAssets > SERVICENODE_SYNC_SPORKS)
    if (!IsBlockchainSynced() && nRequestedServicenodeAssets > SERVICENODE_SYNC_LIST)
    {
        printf("CServicenodeSync::ProcessTick -- nTick %d nRequestedServicenodeAssets %d nRequestedServicenodeAttempt %d -- blockchain is not synced yet\n",
               nTick, nRequestedServicenodeAssets, nRequestedServicenodeAttempt);
        nTimeLastServicenodeList = GetTime();
        nTimeLastPaymentVote = GetTime();
        nTimeLastGovernanceItem = GetTime();
        return;
    }

    if (nRequestedServicenodeAssets == SERVICENODE_SYNC_INITIAL) //  ||
        // (nRequestedServicenodeAssets == SERVICENODE_SYNC_SPORKS && IsBlockchainSynced()))
    {
        SwitchToNextAsset();
    }

    std::vector<CNode*> vNodesCopy = CopyNodeVector();

    for (CNode * pnode : vNodesCopy)
    {
        // Don't try to sync any data from outbound "servicenode" connections -
        // they are temporary and should be considered unreliable for a sync process.
        // Inbound connection this early is most likely a "servicenode" connection
        // initialted from another node, so skip it too.
        if(pnode->fServicenode || (fServiceNode && pnode->fInbound))
        {
            continue;
        }

        // QUICK MODE (REGTEST ONLY!)
//        if(Params().NetworkIDString() == CBaseChainParams::REGTEST)
//        {
//            if(nRequestedServicenodeAttempt <= 2) {
//                pnode->PushMessage(NetMsgType::GETSPORKS); //get current network sporks
//            } else if(nRequestedServicenodeAttempt < 4) {
//                mnodeman.DsegUpdate(pnode);
//            } else if(nRequestedServicenodeAttempt < 6) {
//                int nMnCount = mnodeman.CountServicenodes();
//                pnode->PushMessage(NetMsgType::SERVICENODEPAYMENTSYNC, nMnCount); //sync payment votes
//                SendGovernanceSyncRequest(pnode);
//            } else {
//                nRequestedServicenodeAssets = SERVICENODE_SYNC_FINISHED;
//            }
//            nRequestedServicenodeAttempt++;
//            ReleaseNodeVector(vNodesCopy);
//            return;
//        }

        // NORMAL NETWORK MODE - TESTNET/MAINNET
        {
            if(netfulfilledman.HasFulfilledRequest(pnode->addr, "full-sync"))
            {
                // We already fully synced from this node recently,
                // disconnect to free this connection slot for another peer.
                pnode->fDisconnect = true;
                printf("CServicenodeSync::ProcessTick -- disconnecting from recently synced peer %s\n", pnode->addr.ToString().c_str());
                continue;
            }

            // SPORK : ALWAYS ASK FOR SPORKS AS WE SYNC (we skip this mode now)

//            if(!netfulfilledman.HasFulfilledRequest(pnode->addr, "spork-sync"))
//            {
//                // only request once from each peer
//                netfulfilledman.AddFulfilledRequest(pnode->addr, "spork-sync");
//                // get current network sporks
//                pnode->PushMessage(NetMsgType::GETSPORKS);
//                printf("CServicenodeSync::ProcessTick -- nTick %d nRequestedServicenodeAssets %d -- requesting sporks from peer %s\n",
//                       nTick, nRequestedServicenodeAssets, pnode->addr.ToString().c_str());
//                continue; // always get sporks first, switch to the next node without waiting for the next tick
//            }

            // MNLIST : SYNC SERVICENODE LIST FROM OTHER CONNECTED CLIENTS

            if(nRequestedServicenodeAssets == SERVICENODE_SYNC_LIST)
            {
                printf("CServicenodeSync::ProcessTick -- nTick %d nRequestedServicenodeAssets %d nTimeLastServicenodeList %lld GetTime() %lld diff %lld\n",
                       nTick, nRequestedServicenodeAssets, nTimeLastServicenodeList, GetTime(), GetTime() - nTimeLastServicenodeList);
                // check for timeout first
                if(nTimeLastServicenodeList < GetTime() - SERVICENODE_SYNC_TIMEOUT_SECONDS)
                {
                    printf("CServicenodeSync::ProcessTick -- nTick %d nRequestedServicenodeAssets %d -- timeout\n",
                           nTick, nRequestedServicenodeAssets);
                    if (nRequestedServicenodeAttempt == 0)
                    {
                        printf("CServicenodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName().c_str());
                        // there is no way we can continue without servicenode list, fail here and try later
                        Fail();
                        ReleaseNodeVector(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset();
                    ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if(netfulfilledman.HasFulfilledRequest(pnode->addr, "servicenode-list-sync"))
                {
                    continue;
                }
                netfulfilledman.AddFulfilledRequest(pnode->addr, "servicenode-list-sync");

                if (pnode->nVersion < mnpayments.GetMinServicenodePaymentsProto())
                {
                    continue;
                }

                nRequestedServicenodeAttempt++;

                mnodeman.DsegUpdate(pnode);

                ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }

            // MNW : SYNC SERVICENODE PAYMENT VOTES FROM OTHER CONNECTED CLIENTS

            if(nRequestedServicenodeAssets == SERVICENODE_SYNC_MNW)
            {
                printf("CServicenodeSync::ProcessTick -- nTick %d nRequestedServicenodeAssets %d nTimeLastPaymentVote %lld GetTime() %lld diff %lld\n",
                       nTick, nRequestedServicenodeAssets, nTimeLastPaymentVote, GetTime(), GetTime() - nTimeLastPaymentVote);

                // check for timeout first
                // This might take a lot longer than SERVICENODE_SYNC_TIMEOUT_SECONDS minutes due to new blocks,
                // but that should be OK and it should timeout eventually.
                if(nTimeLastPaymentVote < GetTime() - SERVICENODE_SYNC_TIMEOUT_SECONDS)
                {
                    printf("CServicenodeSync::ProcessTick -- nTick %d nRequestedServicenodeAssets %d -- timeout\n",
                           nTick, nRequestedServicenodeAssets);
                    if (nRequestedServicenodeAttempt == 0)
                    {
                        printf("CServicenodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName().c_str());
                        // probably not a good idea to proceed without winner list
                        Fail();
                        ReleaseNodeVector(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset();
                    ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // check for data
                // if mnpayments already has enough blocks and votes, switch to the next asset
                // try to fetch data from at least two peers though
                if(nRequestedServicenodeAttempt > 1 && mnpayments.IsEnoughData())
                {
                    printf("CServicenodeSync::ProcessTick -- nTick %d nRequestedServicenodeAssets %d -- found enough data\n",
                           nTick, nRequestedServicenodeAssets);
                    SwitchToNextAsset();
                    ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if(netfulfilledman.HasFulfilledRequest(pnode->addr, "servicenode-payment-sync"))
                {
                    continue;
                }
                netfulfilledman.AddFulfilledRequest(pnode->addr, "servicenode-payment-sync");

                if(pnode->nVersion < mnpayments.GetMinServicenodePaymentsProto())
                {
                    continue;
                }

                nRequestedServicenodeAttempt++;

                // ask node for all payment votes it has (new nodes will only return votes for future payments)
                pnode->PushMessage(NetMsgType::SERVICENODEPAYMENTSYNC, mnpayments.GetStorageLimit());
                // ask node for missing pieces only (old nodes will not be asked)
                mnpayments.RequestLowDataPaymentBlocks(pnode);

                ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }

            // GOVOBJ : SYNC GOVERNANCE ITEMS FROM OUR PEERS

//            if(nRequestedServicenodeAssets == SERVICENODE_SYNC_GOVERNANCE) {
//                LogPrint("gobject", "CServicenodeSync::ProcessTick -- nTick %d nRequestedServicenodeAssets %d nTimeLastGovernanceItem %lld GetTime() %lld diff %lld\n", nTick, nRequestedServicenodeAssets, nTimeLastGovernanceItem, GetTime(), GetTime() - nTimeLastGovernanceItem);

//                // check for timeout first
//                if(GetTime() - nTimeLastGovernanceItem > SERVICENODE_SYNC_TIMEOUT_SECONDS) {
//                    LogPrintf("CServicenodeSync::ProcessTick -- nTick %d nRequestedServicenodeAssets %d -- timeout\n", nTick, nRequestedServicenodeAssets);
//                    if(nRequestedServicenodeAttempt == 0) {
//                        LogPrintf("CServicenodeSync::ProcessTick -- WARNING: failed to sync %s\n", GetAssetName());
//                        // it's kind of ok to skip this for now, hopefully we'll catch up later?
//                    }
//                    SwitchToNextAsset();
//                    ReleaseNodeVector(vNodesCopy);
//                    return;
//                }

//                // only request obj sync once from each peer, then request votes on per-obj basis
//                if(netfulfilledman.HasFulfilledRequest(pnode->addr, "governance-sync")) {
//                    int nObjsLeftToAsk = governance.RequestGovernanceObjectVotes(pnode);
//                    static int64_t nTimeNoObjectsLeft = 0;
//                    // check for data
//                    if(nObjsLeftToAsk == 0) {
//                        static int nLastTick = 0;
//                        static int nLastVotes = 0;
//                        if(nTimeNoObjectsLeft == 0) {
//                            // asked all objects for votes for the first time
//                            nTimeNoObjectsLeft = GetTime();
//                        }
//                        // make sure the condition below is checked only once per tick
//                        if(nLastTick == nTick) continue;
//                        if(GetTime() - nTimeNoObjectsLeft > SERVICENODE_SYNC_TIMEOUT_SECONDS &&
//                            governance.GetVoteCount() - nLastVotes < std::max(int(0.0001 * nLastVotes), SERVICENODE_SYNC_TICK_SECONDS)
//                        ) {
//                            // We already asked for all objects, waited for SERVICENODE_SYNC_TIMEOUT_SECONDS
//                            // after that and less then 0.01% or SERVICENODE_SYNC_TICK_SECONDS
//                            // (i.e. 1 per second) votes were recieved during the last tick.
//                            // We can be pretty sure that we are done syncing.
//                            LogPrintf("CServicenodeSync::ProcessTick -- nTick %d nRequestedServicenodeAssets %d -- asked for all objects, nothing to do\n", nTick, nRequestedServicenodeAssets);
//                            // reset nTimeNoObjectsLeft to be able to use the same condition on resync
//                            nTimeNoObjectsLeft = 0;
//                            SwitchToNextAsset();
//                            ReleaseNodeVector(vNodesCopy);
//                            return;
//                        }
//                        nLastTick = nTick;
//                        nLastVotes = governance.GetVoteCount();
//                    }
//                    continue;
//                }
//                netfulfilledman.AddFulfilledRequest(pnode->addr, "governance-sync");

//                if (pnode->nVersion < MIN_GOVERNANCE_PEER_PROTO_VERSION) continue;
//                nRequestedServicenodeAttempt++;

//                SendGovernanceSyncRequest(pnode);

//                ReleaseNodeVector(vNodesCopy);
//                return; //this will cause each peer to get one request each six seconds for the various assets we need
//            }
        }
    }

    // looped through all nodes, release them
    ReleaseNodeVector(vNodesCopy);
}

void CServicenodeSync::SendGovernanceSyncRequest(CNode* pnode)
{
//    if(pnode->nVersion >= GOVERNANCE_FILTER_PROTO_VERSION)
//    {
//        CBloomFilter filter;
//        filter.clear();

//        pnode->PushMessage(NetMsgType::MNGOVERNANCESYNC, uint256(), filter);
//    }
//    else
//    {
//        pnode->PushMessage(NetMsgType::MNGOVERNANCESYNC, uint256());
//    }
}

void CServicenodeSync::UpdatedBlockTip(const CBlockIndex *pindex)
{
    pCurrentBlockIndex = pindex;
}
