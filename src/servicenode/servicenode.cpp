// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

 #include "servicenode/activeservicenode.h"
// #include "consensus/validation.h"
// #include "darksend.h"
#include "init.h"
// #include "governance.h"
#include "servicenode.h"
#include "servicenode-payments.h"
#include "servicenode-sync.h"
#include "servicenodeman.h"
#include "servicenodeconfig.h"
#include "util.h"
#include "datasigner.h"

#include <boost/lexical_cast.hpp>


CServicenode::CServicenode() :
    vin(),
    addr(),
    pubKeyCollateralAddress(),
    pubKeyServicenode(),
    lastPing(),
    vchSig(),
    sigTime(GetAdjustedTime()),
    nLastDsq(0),
    nTimeLastChecked(0),
    nTimeLastPaid(0),
    nTimeLastWatchdogVote(0),
    nActiveState(SERVICENODE_ENABLED),
    nCacheCollateralBlock(0),
    nBlockLastPaid(0),
    nProtocolVersion(PROTOCOL_VERSION),
    nPoSeBanScore(0),
    nPoSeBanHeight(0),
    fAllowMixingTx(true),
    fUnitTest(false)
{}

CServicenode::CServicenode(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyServicenodeNew, int nProtocolVersionIn) :
    vin(vinNew),
    addr(addrNew),
    pubKeyCollateralAddress(pubKeyCollateralAddressNew),
    pubKeyServicenode(pubKeyServicenodeNew),
    lastPing(),
    vchSig(),
    sigTime(GetAdjustedTime()),
    nLastDsq(0),
    nTimeLastChecked(0),
    nTimeLastPaid(0),
    nTimeLastWatchdogVote(0),
    nActiveState(SERVICENODE_ENABLED),
    nCacheCollateralBlock(0),
    nBlockLastPaid(0),
    nProtocolVersion(nProtocolVersionIn),
    nPoSeBanScore(0),
    nPoSeBanHeight(0),
    fAllowMixingTx(true),
    fUnitTest(false)
{}

CServicenode::CServicenode(const CServicenode& other) :
    vin(other.vin),
    addr(other.addr),
    pubKeyCollateralAddress(other.pubKeyCollateralAddress),
    pubKeyServicenode(other.pubKeyServicenode),
    lastPing(other.lastPing),
    vchSig(other.vchSig),
    sigTime(other.sigTime),
    nLastDsq(other.nLastDsq),
    nTimeLastChecked(other.nTimeLastChecked),
    nTimeLastPaid(other.nTimeLastPaid),
    nTimeLastWatchdogVote(other.nTimeLastWatchdogVote),
    nActiveState(other.nActiveState),
    nCacheCollateralBlock(other.nCacheCollateralBlock),
    nBlockLastPaid(other.nBlockLastPaid),
    nProtocolVersion(other.nProtocolVersion),
    nPoSeBanScore(other.nPoSeBanScore),
    nPoSeBanHeight(other.nPoSeBanHeight),
    fAllowMixingTx(other.fAllowMixingTx),
    fUnitTest(other.fUnitTest)
{}

CServicenode::CServicenode(const CServicenodeBroadcast& mnb) :
    vin(mnb.vin),
    addr(mnb.addr),
    pubKeyCollateralAddress(mnb.pubKeyCollateralAddress),
    pubKeyServicenode(mnb.pubKeyServicenode),
    lastPing(mnb.lastPing),
    vchSig(mnb.vchSig),
    sigTime(mnb.sigTime),
    nLastDsq(0),
    nTimeLastChecked(0),
    nTimeLastPaid(0),
    nTimeLastWatchdogVote(mnb.sigTime),
    nActiveState(mnb.nActiveState),
    nCacheCollateralBlock(0),
    nBlockLastPaid(0),
    nProtocolVersion(mnb.nProtocolVersion),
    nPoSeBanScore(0),
    nPoSeBanHeight(0),
    fAllowMixingTx(true),
    fUnitTest(false)
{}

//
// When a new servicenode broadcast is sent, update our information
//
bool CServicenode::UpdateFromNewBroadcast(CServicenodeBroadcast& mnb)
{
    if(mnb.sigTime <= sigTime && !mnb.fRecovery)
    {
        return false;
    }

    pubKeyServicenode = mnb.pubKeyServicenode;
    sigTime = mnb.sigTime;
    vchSig = mnb.vchSig;
    nProtocolVersion = mnb.nProtocolVersion;
    addr = mnb.addr;
    nPoSeBanScore = 0;
    nPoSeBanHeight = 0;
    nTimeLastChecked = 0;
    int nDos = 0;
    if(mnb.lastPing == CServicenodePing() || (mnb.lastPing != CServicenodePing() && mnb.lastPing.CheckAndUpdate(this, true, nDos)))
    {
        lastPing = mnb.lastPing;
        mnodeman.mapSeenServicenodePing.insert(std::make_pair(lastPing.GetHash(), lastPing));
    }
    // if it matches our Servicenode privkey...
    if(fServiceNode && pubKeyServicenode == activeServicenode.pubKeyServicenode)
    {
        nPoSeBanScore = -SERVICENODE_POSE_BAN_MAX_SCORE;
        if(nProtocolVersion == PROTOCOL_VERSION)
        {
            // ... and PROTOCOL_VERSION, then we've been remotely activated ...
            activeServicenode.ManageState();
        }
        else
        {
            // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
            // but also do not ban the node we get this message from
            printf("CServicenode::UpdateFromNewBroadcast -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", nProtocolVersion, PROTOCOL_VERSION);
            return false;
        }
    }
    return true;
}

//
// Deterministically calculate a given "score" for a Servicenode depending on how close it's hash is to
// the proof of work for that block. The further away they are the better, the furthest will win the election
// and get paid this block
//
arith_uint256 CServicenode::CalculateScore(const uint256& blockHash)
{
//    uint256 aux = ArithToUint256(UintToArith256(vin.prevout.hash) + vin.prevout.n);

//    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
//    ss << blockHash;
//    arith_uint256 hash2 = UintToArith256(ss.GetHash());

//    CHashWriter ss2(SER_GETHASH, PROTOCOL_VERSION);
//    ss2 << blockHash;
//    ss2 << aux;
//    arith_uint256 hash3 = UintToArith256(ss2.GetHash());

//    return (hash3 > hash2 ? hash3 - hash2 : hash2 - hash3);
    return uint256();
}

void CServicenode::Check(bool fForce)
{
    LOCK(cs);

    if(fShutdown)
    {
        return;
    }

    if(!fForce && (GetTime() - nTimeLastChecked < SERVICENODE_CHECK_SECONDS))
    {
        return;
    }

    nTimeLastChecked = GetTime();

    printf("CServicenode::Check -- Servicenode %s is in %s state\n", vin.prevout.ToString().c_str(), GetStateString().c_str());

    //once spent, stop doing the checks
    if(IsOutpointSpent())
    {
        return;
    }

    int nHeight = 0;
    if(!fUnitTest)
    {
        TRY_LOCK(cs_main, lockMain);
        if(!lockMain)
        {
            return;
        }

//        CCoins coins;
//        if(!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
//           (unsigned int)vin.prevout.n>=coins.vout.size() ||
//           coins.vout[vin.prevout.n].IsNull()) {
//            nActiveState = SERVICENODE_OUTPOINT_SPENT;
//            LogPrint("servicenode", "CServicenode::Check -- Failed to find Servicenode UTXO, servicenode=%s\n", vin.prevout.ToString());
//            return;
//        }

//        nHeight = chainActive.Height();
    }

    if(IsPoSeBanned())
    {
        if(nHeight < nPoSeBanHeight)
        {
            return; // too early?
        }
        // Otherwise give it a chance to proceed further to do all the usual checks and to change its state.
        // Servicenode still will be on the edge and can be banned back easily if it keeps ignoring mnverify
        // or connect attempts. Will require few mnverify messages to strengthen its position in mn list.
        printf("CServicenode::Check -- Servicenode %s is unbanned and back in list now\n", vin.prevout.ToString().c_str());
        DecreasePoSeBanScore();
    }
    else if(nPoSeBanScore >= SERVICENODE_POSE_BAN_MAX_SCORE)
    {
        nActiveState = SERVICENODE_POSE_BAN;
        // ban for the whole payment cycle
        nPoSeBanHeight = nHeight + mnodeman.size();
        printf("CServicenode::Check -- Servicenode %s is banned till block %d now\n", vin.prevout.ToString().c_str(), nPoSeBanHeight);
        return;
    }

    int nActiveStatePrev = nActiveState;
    bool fOurServicenode = fServiceNode && activeServicenode.pubKeyServicenode == pubKeyServicenode;

    // servicenode doesn't meet payment protocol requirements ...
    // or it's our own node and we just updated it to the new protocol but we are still waiting for activation ...
    bool fRequireUpdate = nProtocolVersion < mnpayments.GetMinServicenodePaymentsProto() ||
            (fOurServicenode && nProtocolVersion < PROTOCOL_VERSION);

    if(fRequireUpdate)
    {
        nActiveState = SERVICENODE_UPDATE_REQUIRED;
        if(nActiveStatePrev != nActiveState)
        {
            printf("CServicenode::Check -- Servicenode %s is in %s state now\n", vin.prevout.ToString().c_str(), GetStateString().c_str());
        }
        return;
    }

    // keep old servicenodes on start, give them a chance to receive updates...
    bool fWaitForPing = !servicenodeSync.IsServicenodeListSynced() && !IsPingedWithin(SERVICENODE_MIN_MNP_SECONDS);

    if(fWaitForPing && !fOurServicenode)
    {
        // ...but if it was already expired before the initial check - return right away
        if(IsExpired() || IsWatchdogExpired() || IsNewStartRequired())
        {
            printf("CServicenode::Check -- Servicenode %s is in %s state, waiting for ping\n", vin.prevout.ToString().c_str(), GetStateString().c_str());
            return;
        }
    }

    // don't expire if we are still in "waiting for ping" mode unless it's our own servicenode
    if(!fWaitForPing || fOurServicenode)
    {
        if(!IsPingedWithin(SERVICENODE_NEW_START_REQUIRED_SECONDS))
        {
            nActiveState = SERVICENODE_NEW_START_REQUIRED;
            if(nActiveStatePrev != nActiveState)
            {
                printf("CServicenode::Check -- Servicenode %s is in %s state now\n", vin.prevout.ToString().c_str(), GetStateString().c_str());
            }
            return;
        }

        bool fWatchdogActive = servicenodeSync.IsSynced() && mnodeman.IsWatchdogActive();
        bool fWatchdogExpired = (fWatchdogActive && ((GetTime() - nTimeLastWatchdogVote) > SERVICENODE_WATCHDOG_MAX_SECONDS));

        printf("CServicenode::Check -- outpoint=%s, nTimeLastWatchdogVote=%d, GetTime()=%d, fWatchdogExpired=%d\n",
                vin.prevout.ToString().c_str(), nTimeLastWatchdogVote, GetTime(), fWatchdogExpired);

        if(fWatchdogExpired)
        {
            nActiveState = SERVICENODE_WATCHDOG_EXPIRED;
            if(nActiveStatePrev != nActiveState)
            {
                printf("CServicenode::Check -- Servicenode %s is in %s state now\n", vin.prevout.ToString().c_str(), GetStateString().c_str());
            }
            return;
        }

        if(!IsPingedWithin(SERVICENODE_EXPIRATION_SECONDS))
        {
            nActiveState = SERVICENODE_EXPIRED;
            if(nActiveStatePrev != nActiveState)
            {
                printf("CServicenode::Check -- Servicenode %s is in %s state now\n", vin.prevout.ToString().c_str(), GetStateString().c_str());
            }
            return;
        }
    }

    if(lastPing.sigTime - sigTime < SERVICENODE_MIN_MNP_SECONDS)
    {
        nActiveState = SERVICENODE_PRE_ENABLED;
        if(nActiveStatePrev != nActiveState)
        {
            printf("CServicenode::Check -- Servicenode %s is in %s state now\n", vin.prevout.ToString().c_str(), GetStateString().c_str());
        }
        return;
    }

    nActiveState = SERVICENODE_ENABLED; // OK
    if(nActiveStatePrev != nActiveState)
    {
        printf("CServicenode::Check -- Servicenode %s is in %s state now\n", vin.prevout.ToString().c_str(), GetStateString().c_str());
    }
}

bool CServicenode::IsValidNetAddr()
{
    return IsValidNetAddr(addr);
}

bool CServicenode::IsValidNetAddr(CService addrIn)
{
    // TODO: regtest is fine with any addresses for now,
    // should probably be a bit smarter if one day we start to implement tests for this
    return // Params().NetworkIDString() == CBaseChainParams::REGTEST ||
            (addrIn.IsIPv4() && IsReachable(addrIn) && addrIn.IsRoutable());
}

servicenode_info_t CServicenode::GetInfo()
{
    servicenode_info_t info;
    info.vin = vin;
    info.addr = addr;
    info.pubKeyCollateralAddress = pubKeyCollateralAddress;
    info.pubKeyServicenode = pubKeyServicenode;
    info.sigTime = sigTime;
    info.nLastDsq = nLastDsq;
    info.nTimeLastChecked = nTimeLastChecked;
    info.nTimeLastPaid = nTimeLastPaid;
    info.nTimeLastWatchdogVote = nTimeLastWatchdogVote;
    info.nTimeLastPing = lastPing.sigTime;
    info.nActiveState = nActiveState;
    info.nProtocolVersion = nProtocolVersion;
    info.fInfoValid = true;
    return info;
}

std::string CServicenode::StateToString(int nStateIn)
{
    switch(nStateIn) {
        case SERVICENODE_PRE_ENABLED:            return "PRE_ENABLED";
        case SERVICENODE_ENABLED:                return "ENABLED";
        case SERVICENODE_EXPIRED:                return "EXPIRED";
        case SERVICENODE_OUTPOINT_SPENT:         return "OUTPOINT_SPENT";
        case SERVICENODE_UPDATE_REQUIRED:        return "UPDATE_REQUIRED";
        case SERVICENODE_WATCHDOG_EXPIRED:       return "WATCHDOG_EXPIRED";
        case SERVICENODE_NEW_START_REQUIRED:     return "NEW_START_REQUIRED";
        case SERVICENODE_POSE_BAN:               return "POSE_BAN";
        default:                                return "UNKNOWN";
    }
}

std::string CServicenode::GetStateString() const
{
    return StateToString(nActiveState);
}

std::string CServicenode::GetStatus() const
{
    // TODO: return smth a bit more human readable here
    return GetStateString();
}

int CServicenode::GetCollateralAge()
{
    int nHeight;
    {
        TRY_LOCK(cs_main, lockMain);
        if(!lockMain || !pindexBest)
        {
            return -1;
        }
        nHeight = pindexBest->nHeight;
    }

    if (nCacheCollateralBlock == 0)
    {
        int nInputAge = nHeight - GetInputDepthInMainChain(vin);
        if(nInputAge > 0)
        {
            nCacheCollateralBlock = nHeight - nInputAge;
        }
        else
        {
            return nInputAge;
        }
    }

    return nHeight - nCacheCollateralBlock;
}

void CServicenode::UpdateLastPaid(const CBlockIndex *pindex, int nMaxBlocksToScanBack)
{
    // TODO implementation

//    if(!pindex)
//    {
//        return;
//    }

//    const CBlockIndex *BlockReading = pindex;

//    CScript mnpayee;
//    mnpayee.SetDestination(pubKeyCollateralAddress.GetID());
//    // LogPrint("servicenode", "CServicenode::UpdateLastPaidBlock -- searching for block with payment to %s\n", vin.prevout.ToString());

//    LOCK(cs_mapServicenodeBlocks);

//    for (int i = 0; BlockReading && BlockReading->nHeight > nBlockLastPaid && i < nMaxBlocksToScanBack; i++)
//    {
//        if(mnpayments.mapServicenodeBlocks.count(BlockReading->nHeight) &&
//            mnpayments.mapServicenodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2))
//        {
//            CBlock block;
//            if(!ReadBlockFromDisk(block, BlockReading, Params().GetConsensus()))
//            {
//                // shouldn't really happen
//                continue;
//            }

//            CAmount nServicenodePayment = GetServicenodePayment(BlockReading->nHeight, block.vtx[0].GetValueOut());

//            for (CTxOut & txout : block.vtx[0].vout)
//            {
//                if(mnpayee == txout.scriptPubKey && nServicenodePayment == txout.nValue)
//                {
//                    nBlockLastPaid = BlockReading->nHeight;
//                    nTimeLastPaid = BlockReading->nTime;
//                    printf("CServicenode::UpdateLastPaidBlock -- searching for block with payment to %s -- found new %d\n", vin.prevout.ToString().c_str(), nBlockLastPaid);
//                    return;
//                }
//            }
//        }

//        if (BlockReading->pprev == NULL)
//        {
//            assert(BlockReading);
//            break;
//        }
//        BlockReading = BlockReading->pprev;
//    }

//    // Last payment for this servicenode wasn't found in latest mnpayments blocks
//    // or it was found in mnpayments blocks but wasn't found in the blockchain.
//    // LogPrint("servicenode", "CServicenode::UpdateLastPaidBlock -- searching for block with payment to %s -- keeping old %d\n", vin.prevout.ToString(), nBlockLastPaid);
}

bool CServicenodeBroadcast::Create(const std::string & strService, const std::string & strKeyServicenode,
                                  const std::string & strTxHash, const std::string & strOutputIndex,
                                  std::string & strErrorRet, CServicenodeBroadcast & mnbRet,
                                  const bool fOffline)
{
    CTxIn txin;
    CPubKey pubKeyCollateralAddressNew;
    CKey keyCollateralAddressNew;
    CPubKey pubKeyServicenodeNew;
    CKey keyServicenodeNew;

    //need correct blocks to send ping
    if(!fOffline && !servicenodeSync.IsBlockchainSynced())
    {
        strErrorRet = "Sync in progress. Must wait until sync is complete to start Servicenode";
        printf("CServicenodeBroadcast::Create -- %s\n", strErrorRet.c_str());
        return false;
    }

    if(!DataSigner::GetKeysFromSecret(strKeyServicenode, keyServicenodeNew, pubKeyServicenodeNew))
    {
        strErrorRet = strprintf("Invalid servicenode key %s", strKeyServicenode);
        printf("CServicenodeBroadcast::Create -- %s\n", strErrorRet.c_str());
        return false;
    }

    if(!pwalletMain->GetServicenodeVinAndKeys(txin,
                                             pubKeyCollateralAddressNew, keyCollateralAddressNew,
                                             strTxHash, strOutputIndex))
    {
        strErrorRet = strprintf("Could not allocate txin %s:%s for servicenode %s", strTxHash, strOutputIndex, strService);
        printf("CServicenodeBroadcast::Create -- %s\n", strErrorRet.c_str());
        return false;
    }

    CService service = CService(strService);
    int mainnetDefaultPort = GetDefaultPort(false);
    if (!fTestNet)
    {
        if (service.GetPort() != mainnetDefaultPort)
        {
            strErrorRet = strprintf("Invalid port %u for servicenode %s, only %d is supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
            printf("CServicenodeBroadcast::Create -- %s\n", strErrorRet.c_str());
            return false;
        }
    }
    else if (service.GetPort() == mainnetDefaultPort)
    {
        strErrorRet = strprintf("Invalid port %u for servicenode %s, %d is the only supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
        printf("CServicenodeBroadcast::Create -- %s\n", strErrorRet.c_str());
        return false;
    }

    return Create(txin, CService(strService),
                  keyCollateralAddressNew, pubKeyCollateralAddressNew,
                  keyServicenodeNew, pubKeyServicenodeNew,
                  strErrorRet, mnbRet);
}

bool CServicenodeBroadcast::Create(const CTxIn & txin, const CService & service,
                                  const CKey & keyCollateralAddressNew,
                                  const CPubKey & pubKeyCollateralAddressNew,
                                  const CKey & keyServicenodeNew,
                                  const CPubKey & pubKeyServicenodeNew,
                                  std::string & strErrorRet, CServicenodeBroadcast & mnbRet)
{
    // wait for reindex and/or import to finish
//    if (fImporting || fReindex)
//    {
//        return false;
//    }

    printf("CServicenodeBroadcast::Create -- pubKeyCollateralAddressNew = %s, pubKeyServicenodeNew.GetID() = %s\n",
             CBitcoinAddress(pubKeyCollateralAddressNew.GetID()).ToString().c_str(),
             pubKeyServicenodeNew.GetID().ToString().c_str());

    CServicenodePing mnp(txin);
    if(!mnp.Sign(keyServicenodeNew, pubKeyServicenodeNew))
    {
        strErrorRet = strprintf("Failed to sign ping, servicenode=%s", txin.prevout.ToString());
        printf("CServicenodeBroadcast::Create -- %s\n", strErrorRet.c_str());
        mnbRet = CServicenodeBroadcast();
        return false;
    }

    mnbRet = CServicenodeBroadcast(service, txin,
                                  pubKeyCollateralAddressNew, pubKeyServicenodeNew,
                                  PROTOCOL_VERSION);

    if(!mnbRet.IsValidNetAddr())
    {
        strErrorRet = strprintf("Invalid IP address, servicenode=%s", txin.prevout.ToString());
        printf("CServicenodeBroadcast::Create -- %s\n", strErrorRet.c_str());
        mnbRet = CServicenodeBroadcast();
        return false;
    }

    mnbRet.lastPing = mnp;
    if(!mnbRet.Sign(keyCollateralAddressNew))
    {
        strErrorRet = strprintf("Failed to sign broadcast, servicenode=%s", txin.prevout.ToString());
        printf("CServicenodeBroadcast::Create -- %s\n", strErrorRet.c_str());
        mnbRet = CServicenodeBroadcast();
        return false;
    }

    return true;
}

bool CServicenodeBroadcast::SimpleCheck(int& nDos)
{
    nDos = 0;

    // make sure addr is valid
    if(!IsValidNetAddr())
    {
        printf("CServicenodeBroadcast::SimpleCheck -- Invalid addr, rejected: servicenode=%s  addr=%s\n",
                    vin.prevout.ToString().c_str(), addr.ToString().c_str());
        return false;
    }

    // make sure signature isn't in the future (past is OK)
    if (sigTime > GetAdjustedTime() + 60 * 60)
    {
        printf("CServicenodeBroadcast::SimpleCheck -- Signature rejected, too far into the future: servicenode=%s\n",
               vin.prevout.ToString().c_str());
        nDos = 1;
        return false;
    }

    // empty ping or incorrect sigTime/unknown blockhash
    if(lastPing == CServicenodePing() || !lastPing.SimpleCheck(nDos))
    {
        // one of us is probably forked or smth, just mark it as expired and check the rest of the rules
        nActiveState = SERVICENODE_EXPIRED;
    }

    if(nProtocolVersion < mnpayments.GetMinServicenodePaymentsProto())
    {
        printf("CServicenodeBroadcast::SimpleCheck -- ignoring outdated Servicenode: servicenode=%s  nProtocolVersion=%d\n", vin.prevout.ToString().c_str(), nProtocolVersion);
        return false;
    }

    CScript pubkeyScript;
    pubkeyScript.SetDestination(pubKeyCollateralAddress.GetID());

    if(pubkeyScript.size() != 25)
    {
        printf("CServicenodeBroadcast::SimpleCheck -- pubKeyCollateralAddress has the wrong size\n");
        nDos = 100;
        return false;
    }

    CScript pubkeyScript2;
    pubkeyScript2.SetDestination(pubKeyServicenode.GetID());

    if(pubkeyScript2.size() != 25)
    {
        printf("CServicenodeBroadcast::SimpleCheck -- pubKeyServicenode has the wrong size\n");
        nDos = 100;
        return false;
    }

    if(!vin.scriptSig.empty())
    {
        printf("CServicenodeBroadcast::SimpleCheck -- Ignore Not Empty ScriptSig %s\n", vin.ToString().c_str());
        nDos = 100;
        return false;
    }

    int mainnetDefaultPort = GetDefaultPort(false);
    if (!fTestNet)
    {
        if (addr.GetPort() != mainnetDefaultPort)
        {
            return false;
        }
    }
    else
    {
        if(addr.GetPort() == mainnetDefaultPort)
        {
            return false;
        }
    }

    return true;
}

bool CServicenodeBroadcast::Update(CServicenode* pmn, int& nDos)
{
    nDos = 0;

    if(pmn->sigTime == sigTime && !fRecovery)
    {
        // mapSeenServicenodeBroadcast in CServicenodeMan::CheckMnbAndUpdateServicenodeList should filter legit duplicates
        // but this still can happen if we just started, which is ok, just do nothing here.
        return false;
    }

    // this broadcast is older than the one that we already have - it's bad and should never happen
    // unless someone is doing something fishy
    if(pmn->sigTime > sigTime)
    {
        printf("CServicenodeBroadcast::Update -- Bad sigTime %d (existing broadcast is at %d) for Servicenode %s %s\n",
                      sigTime, pmn->sigTime, vin.prevout.ToString().c_str(), addr.ToString().c_str());
        return false;
    }

    pmn->Check();

    // servicenode is banned by PoSe
    if(pmn->IsPoSeBanned())
    {
        printf("CServicenodeBroadcast::Update -- Banned by PoSe, servicenode=%s\n", vin.prevout.ToString().c_str());
        return false;
    }

    // IsVnAssociatedWithPubkey is validated once in CheckOutpoint, after that they just need to match
    if(pmn->pubKeyCollateralAddress != pubKeyCollateralAddress)
    {
        printf("CServicenodeBroadcast::Update -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    if (!CheckSignature(nDos))
    {
        printf("CServicenodeBroadcast::Update -- CheckSignature() failed, servicenode=%s\n", vin.prevout.ToString().c_str());
        return false;
    }

    // if ther was no servicenode broadcast recently or if it matches our Servicenode privkey...
    if(!pmn->IsBroadcastedWithin(SERVICENODE_MIN_MNB_SECONDS) || (fServiceNode && pubKeyServicenode == activeServicenode.pubKeyServicenode))
    {
        // take the newest entry
        printf("CServicenodeBroadcast::Update -- Got UPDATED Servicenode entry: addr=%s\n", addr.ToString().c_str());
        if (pmn->UpdateFromNewBroadcast((*this)))
        {
            pmn->Check();
            Relay();
       }
       servicenodeSync.AddedServicenodeList();
    }

    return true;
}

bool CServicenodeBroadcast::CheckOutpoint(int& nDos)
{
    // we are a servicenode with the same vin (i.e. already activated) and this mnb is ours (matches our Servicenode privkey)
    // so nothing to do here for us
    if(fServiceNode && vin.prevout == activeServicenode.vin.prevout && pubKeyServicenode == activeServicenode.pubKeyServicenode)
    {
        return false;
    }

    if (!CheckSignature(nDos))
    {
        printf("CServicenodeBroadcast::CheckOutpoint -- CheckSignature() failed, servicenode=%s\n", vin.prevout.ToString().c_str());
        return false;
    }

    {
        TRY_LOCK(cs_main, lockMain);
        if(!lockMain)
        {
            // not mnb fault, let it to be checked again later
            printf("CServicenodeBroadcast::CheckOutpoint -- Failed to aquire lock, addr=%s", addr.ToString().c_str());
            mnodeman.mapSeenServicenodeBroadcast.erase(GetHash());
            return false;
        }

        // TODO implementation
        return false;

//        CCoins coins;
//        if(!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
//           (unsigned int)vin.prevout.n>=coins.vout.size() ||
//           coins.vout[vin.prevout.n].IsNull()) {
//            LogPrint("servicenode", "CServicenodeBroadcast::CheckOutpoint -- Failed to find Servicenode UTXO, servicenode=%s\n", vin.prevout.ToString());
//            return false;
//        }
//        if(coins.vout[vin.prevout.n].nValue != SERVICENODE_AMOUNT * COIN) {
//            LogPrint("servicenode", "CServicenodeBroadcast::CheckOutpoint -- Servicenode UTXO should have %d BLOCK, servicenode=%s\n", SERVICENODE_AMOUNT, vin.prevout.ToString());
//            return false;
//        }
//        if(chainActive.Height() - coins.nHeight + 1 < Params().GetConsensus().nServicenodeMinimumConfirmations) {
//            LogPrintf("CServicenodeBroadcast::CheckOutpoint -- Servicenode UTXO must have at least %d confirmations, servicenode=%s\n",
//                    Params().GetConsensus().nServicenodeMinimumConfirmations, vin.prevout.ToString());
//            // maybe we miss few blocks, let this mnb to be checked again later
//            mnodeman.mapSeenServicenodeBroadcast.erase(GetHash());
//            return false;
//        }
    }

    printf("CServicenodeBroadcast::CheckOutpoint -- Servicenode UTXO verified\n");

    // make sure the vout that was signed is related to the transaction that spawned the Servicenode
    //  - this is expensive, so it's only done once per Servicenode

    if(!DataSigner::IsVinAssociatedWithPubkey(vin, pubKeyCollateralAddress))
    {
        printf("CServicenodeMan::CheckOutpoint -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    // verify that sig time is legit in past
    // should be at least not earlier than block when 1000 BLOCKS tx got nServicenodeMinimumConfirmations
    uint256 hashBlock = uint256();
    CTransaction tx2;
    GetTransaction(vin.prevout.hash, tx2, hashBlock);
    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second)
        {
            CBlockIndex* pMNIndex = (*mi).second; // block for 1000 BLOCKS tx -> 1 confirmation
            CBlockIndex * pConfIndex = FindBlockByHeight(pMNIndex->nHeight + nServicenodeMinimumConfirmations - 1);
            if(!pConfIndex || pConfIndex->GetBlockTime() > sigTime)
            {
                printf("CServicenodeBroadcast::CheckOutpoint -- Bad sigTime %d (%d conf block is at %d) for Servicenode %s %s\n",
                       sigTime, nServicenodeMinimumConfirmations,
                       pConfIndex ? pConfIndex->GetBlockTime() : 0,
                       vin.prevout.ToString().c_str(), addr.ToString().c_str());
                return false;
            }
        }
    }

    return true;
}

bool CServicenodeBroadcast::Sign(const CKey & keyCollateralAddress)
{
    std::string strError;
    std::string strMessage;

    sigTime = GetAdjustedTime();

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                    pubKeyCollateralAddress.GetID().ToString() + pubKeyServicenode.GetID().ToString() +
                    boost::lexical_cast<std::string>(nProtocolVersion);

    if(!DataSigner::SignMessage(strMessage, vchSig, keyCollateralAddress))
    {
        printf("CServicenodeBroadcast::Sign -- SignMessage() failed\n");
        return false;
    }

    if(!DataSigner::VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError))
    {
        printf("CServicenodeBroadcast::Sign -- VerifyMessage() failed, error: %s\n", strError.c_str());
        return false;
    }

    return true;
}

bool CServicenodeBroadcast::CheckSignature(int& nDos)
{
    std::string strMessage;
    std::string strError = "";
    nDos = 0;

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                    pubKeyCollateralAddress.GetID().ToString() + pubKeyServicenode.GetID().ToString() +
                    boost::lexical_cast<std::string>(nProtocolVersion);

    printf("CServicenodeBroadcast::CheckSignature -- strMessage: %s  pubKeyCollateralAddress address: %s  sig: %s\n",
           strMessage.c_str(), CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString().c_str(),
           EncodeBase64(&vchSig[0], vchSig.size()).c_str());

    if(!DataSigner::VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError))
    {
        printf("CServicenodeBroadcast::CheckSignature -- Got bad Servicenode announce signature, error: %s\n", strError.c_str());
        nDos = 100;
        return false;
    }

    return true;
}

void CServicenodeBroadcast::Relay()
{
    CInv inv(MSG_SERVICENODE_ANNOUNCE, GetHash());
    RelayInventory(inv);
}

CServicenodePing::CServicenodePing(const CTxIn & vinNew)
{
    LOCK(cs_main);
    if (!pindexBest || pindexBest->nHeight < 12)
    {
        return;
    }

    vin = vinNew;
    blockHash = pindexBest->GetBlockHash();
    sigTime = GetAdjustedTime();
    vchSig = std::vector<unsigned char>();
}

bool CServicenodePing::Sign(const CKey & keyServicenode, const CPubKey & pubKeyServicenode)
{
    std::string strError;
    // std::string strServiceNodeSignMessage;

    sigTime = GetAdjustedTime();
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);

    if (!DataSigner::SignMessage(strMessage, vchSig, keyServicenode))
    {
        printf("CServicenodePing::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!DataSigner::VerifyMessage(pubKeyServicenode, vchSig, strMessage, strError))
    {
        printf("CServicenodePing::Sign -- VerifyMessage() failed, error: %s\n", strError.c_str());
        return false;
    }

    return true;
}

bool CServicenodePing::CheckSignature(CPubKey& pubKeyServicenode, int &nDos)
{
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);
    std::string strError = "";
    nDos = 0;

    if (!DataSigner::VerifyMessage(pubKeyServicenode, vchSig, strMessage, strError))
    {
        printf("CServicenodePing::CheckSignature -- Got bad Servicenode ping signature, servicenode=%s, error: %s\n",
               vin.prevout.ToString().c_str(), strError.c_str());
        nDos = 33;
        return false;
    }
    return true;
}

bool CServicenodePing::SimpleCheck(int& nDos)
{
    // don't ban by default
    nDos = 0;

    if (sigTime > GetAdjustedTime() + 60 * 60)
    {
        printf("CServicenodePing::SimpleCheck -- Signature rejected, too far into the future, servicenode=%s\n", vin.prevout.ToString().c_str());
        nDos = 1;
        return false;
    }

    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if (mi == mapBlockIndex.end())
        {
            printf("CServicenodePing::SimpleCheck -- Servicenode ping is invalid, unknown block hash: servicenode=%s blockHash=%s\n",
                   vin.prevout.ToString().c_str(), blockHash.ToString().c_str());

            // maybe we stuck or forked so we shouldn't ban this node, just fail to accept this ping
            // TODO: or should we also request this block?
            return false;
        }
    }
    printf("CServicenodePing::SimpleCheck -- Servicenode ping verified: servicenode=%s  blockHash=%s  sigTime=%d\n",
           vin.prevout.ToString().c_str(), blockHash.ToString().c_str(), sigTime);
    return true;
}

bool CServicenodePing::CheckAndUpdate(CServicenode* pmn, bool fFromNewBroadcast, int& nDos)
{
    // don't ban by default
    nDos = 0;

    if (!SimpleCheck(nDos))
    {
        return false;
    }

    if (pmn == NULL)
    {
        printf("CServicenodePing::CheckAndUpdate -- Couldn't find Servicenode entry, servicenode=%s\n", vin.prevout.ToString().c_str());
        return false;
    }

    if(!fFromNewBroadcast)
    {
        if (pmn->IsUpdateRequired())
        {
            printf("CServicenodePing::CheckAndUpdate -- servicenode protocol is outdated, servicenode=%s\n", vin.prevout.ToString().c_str());
            return false;
        }

        if (pmn->IsNewStartRequired())
        {
            printf("CServicenodePing::CheckAndUpdate -- servicenode is completely expired, new start is required, servicenode=%s\n", vin.prevout.ToString().c_str());
            return false;
        }
    }

    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if ((*mi).second && (*mi).second->nHeight < pindexBest->nHeight - 24)
        {
            printf("CServicenodePing::CheckAndUpdate -- Servicenode ping is invalid, block hash is too old: servicenode=%s  blockHash=%s\n",
                   vin.prevout.ToString().c_str(), blockHash.ToString().c_str());
            // nDos = 1;
            return false;
        }
    }

    printf("CServicenodePing::CheckAndUpdate -- New ping: servicenode=%s  blockHash=%s  sigTime=%d\n",
           vin.prevout.ToString().c_str(), blockHash.ToString().c_str(), sigTime);

    // LogPrintf("mnping - Found corresponding mn for vin: %s\n", vin.prevout.ToString());
    // update only if there is no known ping for this servicenode or
    // last ping was more then SERVICENODE_MIN_MNP_SECONDS-60 ago comparing to this one
    if (pmn->IsPingedWithin(SERVICENODE_MIN_MNP_SECONDS - 60, sigTime))
    {
        printf("CServicenodePing::CheckAndUpdate -- Servicenode ping arrived too early, servicenode=%s\n",
               vin.prevout.ToString().c_str());
        //nDos = 1; //disable, this is happening frequently and causing banned peers
        return false;
    }

    if (!CheckSignature(pmn->pubKeyServicenode, nDos))
    {
        return false;
    }

    // so, ping seems to be ok

    // if we are still syncing and there was no known ping for this mn for quite a while
    // (NOTE: assuming that SERVICENODE_EXPIRATION_SECONDS/2 should be enough to finish mn list sync)
    if(!servicenodeSync.IsServicenodeListSynced() && !pmn->IsPingedWithin(SERVICENODE_EXPIRATION_SECONDS/2))
    {
        // let's bump sync timeout
        printf("CServicenodePing::CheckAndUpdate -- bumping sync timeout, servicenode=%s\n", vin.prevout.ToString().c_str());
        servicenodeSync.AddedServicenodeList();
    }

    // let's store this ping as the last one
    printf("CServicenodePing::CheckAndUpdate -- Servicenode ping accepted, servicenode=%s\n", vin.prevout.ToString().c_str());
    pmn->lastPing = *this;

    // and update mnodeman.mapSeenServicenodeBroadcast.lastPing which is probably outdated
    CServicenodeBroadcast mnb(*pmn);
    uint256 hash = mnb.GetHash();
    if (mnodeman.mapSeenServicenodeBroadcast.count(hash))
    {
        mnodeman.mapSeenServicenodeBroadcast[hash].second.lastPing = *this;
    }

    pmn->Check(true); // force update, ignoring cache
    if (!pmn->IsEnabled())
    {
        return false;
    }

    printf("CServicenodePing::CheckAndUpdate -- Servicenode ping acceepted and relayed, servicenode=%s\n",
           vin.prevout.ToString().c_str());
    Relay();

    return true;
}

void CServicenodePing::Relay()
{
    CInv inv(MSG_SERVICENODE_PING, GetHash());
    RelayInventory(inv);
}

void CServicenode::AddGovernanceVote(uint256 nGovernanceObjectHash)
{
    if(mapGovernanceObjectsVotedOn.count(nGovernanceObjectHash)) {
        mapGovernanceObjectsVotedOn[nGovernanceObjectHash]++;
    } else {
        mapGovernanceObjectsVotedOn.insert(std::make_pair(nGovernanceObjectHash, 1));
    }
}

void CServicenode::RemoveGovernanceObject(uint256 nGovernanceObjectHash)
{
    std::map<uint256, int>::iterator it = mapGovernanceObjectsVotedOn.find(nGovernanceObjectHash);
    if(it == mapGovernanceObjectsVotedOn.end()) {
        return;
    }
    mapGovernanceObjectsVotedOn.erase(it);
}

void CServicenode::UpdateWatchdogVoteTime()
{
    LOCK(cs);
    nTimeLastWatchdogVote = GetTime();
}

/**
*   FLAG GOVERNANCE ITEMS AS DIRTY
*
*   - When servicenode come and go on the network, we must flag the items they voted on to recalc it's cached flags
*
*/
void CServicenode::FlagGovernanceItemsAsDirty()
{
    std::vector<uint256> vecDirty;
    {
        std::map<uint256, int>::iterator it = mapGovernanceObjectsVotedOn.begin();
        while(it != mapGovernanceObjectsVotedOn.end()) {
            vecDirty.push_back(it->first);
            ++it;
        }
    }
    for(size_t i = 0; i < vecDirty.size(); ++i) {
        mnodeman.AddDirtyGovernanceObjectHash(vecDirty[i]);
    }
}
