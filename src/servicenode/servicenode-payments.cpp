// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeservicenode.h"
// #include "darksend.h"
// #include "governance-classes.h"
#include "servicenode-payments.h"
#include "servicenode-sync.h"
#include "servicenodeman.h"
#include "netfulfilledman.h"
// #include "spork.h"
#include "util.h"
#include "datasigner.h"

#include <boost/lexical_cast.hpp>

/** Object for who's going to get paid on which blocks */
CServicenodePayments mnpayments;

CCriticalSection cs_vecPayees;
CCriticalSection cs_mapServicenodeBlocks;
CCriticalSection cs_mapServicenodePaymentVotes;

/**
* IsBlockValueValid
*
*   Determine if coinbase outgoing created money is the correct value
*
*   Why is this needed?
*   - In Dash some blocks are superblocks, which output much higher amounts of coins
*   - Otherblocks are 10% lower in outgoing value, so in total, no extra coins are created
*   - When non-superblocks are detected, the normal schedule should be maintained
*/

bool IsBlockValueValid(const CBlock& block, int nBlockHeight, CAmount blockReward, std::string &strErrorRet)
{
    strErrorRet = "";

    bool isBlockRewardValueMet = (block.vtx[0].GetValueOut() <= blockReward);
    if(fDebug)
    {
        printf("block.vtx[0].GetValueOut() %lld <= blockReward %lld\n",
               block.vtx[0].GetValueOut(), blockReward);
    }

    // we are still using budgets, but we have no data about them anymore,
    // all we know is predefined budget cycle and window

//    const Consensus::Params& consensusParams = Params().GetConsensus();

//    if(nBlockHeight < consensusParams.nSuperblockStartBlock) {
//        int nOffset = nBlockHeight % consensusParams.nBudgetPaymentsCycleBlocks;
//        if(nBlockHeight >= consensusParams.nBudgetPaymentsStartBlock &&
//            nOffset < consensusParams.nBudgetPaymentsWindowBlocks) {
//            // NOTE: make sure SPORK_13_OLD_SUPERBLOCK_FLAG is disabled when 12.1 starts to go live
//            if(servicenodeSync.IsSynced() && !sporkManager.IsSporkActive(SPORK_13_OLD_SUPERBLOCK_FLAG)) {
//                // no budget blocks should be accepted here, if SPORK_13_OLD_SUPERBLOCK_FLAG is disabled
//                LogPrint("gobject", "IsBlockValueValid -- Client synced but budget spork is disabled, checking block value against block reward\n");
//                if(!isBlockRewardValueMet) {
//                    strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, budgets are disabled",
//                                            nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//                }
//                return isBlockRewardValueMet;
//            }
//            LogPrint("gobject", "IsBlockValueValid -- WARNING: Skipping budget block value checks, accepting block\n");
//            // TODO: reprocess blocks to make sure they are legit?
//            return true;
//        }
//        // LogPrint("gobject", "IsBlockValueValid -- Block is not in budget cycle window, checking block value against block reward\n");
//        if(!isBlockRewardValueMet) {
//            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, block is not in budget cycle window",
//                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//        }
//        return isBlockRewardValueMet;
//    }

//    // superblocks started

//    CAmount nSuperblockMaxValue =  blockReward + CSuperblock::GetPaymentsLimit(nBlockHeight);
//    bool isSuperblockMaxValueMet = (block.vtx[0].GetValueOut() <= nSuperblockMaxValue);

//    LogPrint("gobject", "block.vtx[0].GetValueOut() %lld <= nSuperblockMaxValue %lld\n", block.vtx[0].GetValueOut(), nSuperblockMaxValue);

//    if(!servicenodeSync.IsSynced()) {
//        // not enough data but at least it must NOT exceed superblock max value
//        if(CSuperblock::IsValidBlockHeight(nBlockHeight)) {
//            if(fDebug) LogPrintf("IsBlockPayeeValid -- WARNING: Client not synced, checking superblock max bounds only\n");
//            if(!isSuperblockMaxValueMet) {
//                strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded superblock max value",
//                                        nBlockHeight, block.vtx[0].GetValueOut(), nSuperblockMaxValue);
//            }
//            return isSuperblockMaxValueMet;
//        }
//        if(!isBlockRewardValueMet) {
//            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, only regular blocks are allowed at this height",
//                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//        }
//        // it MUST be a regular block otherwise
//        return isBlockRewardValueMet;
//    }

//    // we are synced, let's try to check as much data as we can

//    if(sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED)) {
//        if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//            if(CSuperblockManager::IsValid(block.vtx[0], nBlockHeight, blockReward)) {
//                LogPrint("gobject", "IsBlockValueValid -- Valid superblock at height %d: %s", nBlockHeight, block.vtx[0].ToString());
//                // all checks are done in CSuperblock::IsValid, nothing to do here
//                return true;
//            }

//            // triggered but invalid? that's weird
//            LogPrintf("IsBlockValueValid -- ERROR: Invalid superblock detected at height %d: %s", nBlockHeight, block.vtx[0].ToString());
//            // should NOT allow invalid superblocks, when superblocks are enabled
//            strErrorRet = strprintf("invalid superblock detected at height %d", nBlockHeight);
//            return false;
//        }
//        LogPrint("gobject", "IsBlockValueValid -- No triggered superblock detected at height %d\n", nBlockHeight);
//        if(!isBlockRewardValueMet) {
//            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, no triggered superblock detected",
//                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//        }
//    } else {
//        // should NOT allow superblocks at all, when superblocks are disabled
//        LogPrint("gobject", "IsBlockValueValid -- Superblocks are disabled, no superblocks allowed\n");
//        if(!isBlockRewardValueMet) {
//            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, superblocks are disabled",
//                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//        }
//    }

//    // it MUST be a regular block
//    return isBlockRewardValueMet;
    return false;
}

bool IsBlockPayeeValid(const CTransaction& txNew, int nBlockHeight, CAmount blockReward)
{
    if(!servicenodeSync.IsSynced())
    {
        //there is no budget data to use to check anything, let's just accept the longest chain
        if(fDebug)
        {
            printf("IsBlockPayeeValid -- WARNING: Client not synced, skipping block payee checks\n");
        }
        return true;
    }

    // we are still using budgets, but we have no data about them anymore,
    // we can only check servicenode payments

//    const Consensus::Params& consensusParams = Params().GetConsensus();

//    if(nBlockHeight < consensusParams.nSuperblockStartBlock) {
//        if(mnpayments.IsTransactionValid(txNew, nBlockHeight)) {
//            LogPrint("mnpayments", "IsBlockPayeeValid -- Valid servicenode payment at height %d: %s", nBlockHeight, txNew.ToString());
//            return true;
//        }

//        int nOffset = nBlockHeight % consensusParams.nBudgetPaymentsCycleBlocks;
//        if(nBlockHeight >= consensusParams.nBudgetPaymentsStartBlock &&
//            nOffset < consensusParams.nBudgetPaymentsWindowBlocks) {
//            if(!sporkManager.IsSporkActive(SPORK_13_OLD_SUPERBLOCK_FLAG)) {
//                // no budget blocks should be accepted here, if SPORK_13_OLD_SUPERBLOCK_FLAG is disabled
//                LogPrint("gobject", "IsBlockPayeeValid -- ERROR: Client synced but budget spork is disabled and servicenode payment is invalid\n");
//                return false;
//            }
//            // NOTE: this should never happen in real, SPORK_13_OLD_SUPERBLOCK_FLAG MUST be disabled when 12.1 starts to go live
//            LogPrint("gobject", "IsBlockPayeeValid -- WARNING: Probably valid budget block, have no data, accepting\n");
//            // TODO: reprocess blocks to make sure they are legit?
//            return true;
//        }

//        if(sporkManager.IsSporkActive(SPORK_8_SERVICENODE_PAYMENT_ENFORCEMENT)) {
//            LogPrintf("IsBlockPayeeValid -- ERROR: Invalid servicenode payment detected at height %d: %s", nBlockHeight, txNew.ToString());
//            return false;
//        }

//        LogPrintf("IsBlockPayeeValid -- WARNING: Servicenode payment enforcement is disabled, accepting any payee\n");
//        return true;
//    }

//    // superblocks started
//    // SEE IF THIS IS A VALID SUPERBLOCK

//    if(sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED)) {
//        if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//            if(CSuperblockManager::IsValid(txNew, nBlockHeight, blockReward)) {
//                LogPrint("gobject", "IsBlockPayeeValid -- Valid superblock at height %d: %s", nBlockHeight, txNew.ToString());
//                return true;
//            }

//            LogPrintf("IsBlockPayeeValid -- ERROR: Invalid superblock detected at height %d: %s", nBlockHeight, txNew.ToString());
//            // should NOT allow such superblocks, when superblocks are enabled
//            return false;
//        }
//        // continue validation, should pay MN
//        LogPrint("gobject", "IsBlockPayeeValid -- No triggered superblock detected at height %d\n", nBlockHeight);
//    } else {
//        // should NOT allow superblocks at all, when superblocks are disabled
//        LogPrint("gobject", "IsBlockPayeeValid -- Superblocks are disabled, no superblocks allowed\n");
//    }

//    // IF THIS ISN'T A SUPERBLOCK OR SUPERBLOCK IS INVALID, IT SHOULD PAY A SERVICENODE DIRECTLY
//    if(mnpayments.IsTransactionValid(txNew, nBlockHeight)) {
//        LogPrint("mnpayments", "IsBlockPayeeValid -- Valid servicenode payment at height %d: %s", nBlockHeight, txNew.ToString());
//        return true;
//    }

//    if(sporkManager.IsSporkActive(SPORK_8_SERVICENODE_PAYMENT_ENFORCEMENT)) {
//        LogPrintf("IsBlockPayeeValid -- ERROR: Invalid servicenode payment detected at height %d: %s", nBlockHeight, txNew.ToString());
//        return false;
//    }

//    LogPrintf("IsBlockPayeeValid -- WARNING: Servicenode payment enforcement is disabled, accepting any payee\n");
    return true;
}

void FillBlockPayments(CMutableTransaction& txNew, int nBlockHeight, CAmount blockReward, CTxOut& txoutServicenodeRet, std::vector<CTxOut>& voutSuperblockRet)
{
//    // only create superblocks if spork is enabled AND if superblock is actually triggered
//    // (height should be validated inside)
//    if(sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED) &&
//        CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//            LogPrint("gobject", "FillBlockPayments -- triggered superblock creation at height %d\n", nBlockHeight);
//            CSuperblockManager::CreateSuperblock(txNew, nBlockHeight, voutSuperblockRet);
//            return;
//    }

//    // FILL BLOCK PAYEE WITH SERVICENODE PAYMENT OTHERWISE
//    mnpayments.FillBlockPayee(txNew, nBlockHeight, blockReward, txoutServicenodeRet);
//    LogPrint("mnpayments", "FillBlockPayments -- nBlockHeight %d blockReward %lld txoutServicenodeRet %s txNew %s",
//                            nBlockHeight, blockReward, txoutServicenodeRet.ToString(), txNew.ToString());
}

std::string GetRequiredPaymentsString(int nBlockHeight)
{
    // OTHERWISE, PAY SERVICENODE
    return mnpayments.GetRequiredPaymentsString(nBlockHeight);
}

void CServicenodePayments::Clear()
{
    LOCK2(cs_mapServicenodeBlocks, cs_mapServicenodePaymentVotes);
    mapServicenodeBlocks.clear();
    mapServicenodePaymentVotes.clear();
}

bool CServicenodePayments::CanVote(COutPoint outServicenode, int nBlockHeight)
{
    LOCK(cs_mapServicenodePaymentVotes);

    if (mapServicenodesLastVote.count(outServicenode) && mapServicenodesLastVote[outServicenode] == nBlockHeight) {
        return false;
    }

    //record this servicenode voted
    mapServicenodesLastVote[outServicenode] = nBlockHeight;
    return true;
}

/**
*   FillBlockPayee
*
*   Fill Servicenode ONLY payment block
*/

void CServicenodePayments::FillBlockPayee(CMutableTransaction& txNew, int nBlockHeight, CAmount blockReward, CTxOut& txoutServicenodeRet)
{
    // make sure it's not filled yet
    txoutServicenodeRet = CTxOut();

    CScript payee;

    if(!mnpayments.GetBlockPayee(nBlockHeight, payee))
    {
        // no servicenode detected...
        int nCount = 0;
        CServicenode *winningNode = mnodeman.GetNextServicenodeInQueueForPayment(nBlockHeight, true, nCount);
        if(!winningNode)
        {
            // ...and we can't calculate it on our own
            printf("CServicenodePayments::FillBlockPayee -- Failed to detect servicenode to pay\n");
            return;
        }
        // fill payee with locally calculated winner and hope for the best
        payee.SetDestination(winningNode->pubKeyCollateralAddress.GetID());
    }

    // GET SERVICENODE PAYMENT VARIABLES SETUP
    CAmount servicenodePayment = GetServicenodePayment(nBlockHeight, blockReward);

    // split reward between miner ...
    txNew.vout[0].nValue -= servicenodePayment;
    // ... and servicenode
    txoutServicenodeRet = CTxOut(servicenodePayment, payee);
    txNew.vout.push_back(txoutServicenodeRet);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);

    printf("CServicenodePayments::FillBlockPayee -- Servicenode payment %lld to %s\n", servicenodePayment, address2.ToString().c_str());
}

int CServicenodePayments::GetMinServicenodePaymentsProto()
{
//    return sporkManager.IsSporkActive(SPORK_10_SERVICENODE_PAY_UPDATED_NODES)
//            ? MIN_SERVICENODE_PAYMENT_PROTO_VERSION_2
//            : MIN_SERVICENODE_PAYMENT_PROTO_VERSION_1;
    return MIN_SERVICENODE_PAYMENT_PROTO_VERSION_1;
}

void CServicenodePayments::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    // Ignore any payments messages until servicenode list is synced
    if(!servicenodeSync.IsServicenodeListSynced())
    {
        return;
    }

    if (strCommand == NetMsgType::SERVICENODEPAYMENTSYNC)
    { //Servicenode Payments Request Sync
        // Ignore such requests until we are fully synced.
        // We could start processing this after servicenode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!servicenodeSync.IsSynced())
        {
            return;
        }

        int nCountNeeded;
        vRecv >> nCountNeeded;

        if(netfulfilledman.HasFulfilledRequest(pfrom->addr, NetMsgType::SERVICENODEPAYMENTSYNC))
        {
            // Asking for the payments list multiple times in a short period of time is no good
            printf("SERVICENODEPAYMENTSYNC -- peer already asked me for the list, peer=%s\n", pfrom->addr.ToString().c_str());
            pfrom->Misbehaving(20);
            return;
        }
        netfulfilledman.AddFulfilledRequest(pfrom->addr, NetMsgType::SERVICENODEPAYMENTSYNC);

        Sync(pfrom);
        printf("SERVICENODEPAYMENTSYNC -- Sent Servicenode payment votes to peer %s\n", pfrom->addr.ToString().c_str());

    }
    else if (strCommand == NetMsgType::SERVICENODEPAYMENTVOTE)
    {
        // Servicenode Payments Vote for the Winner

        CServicenodePaymentVote vote;
        vRecv >> vote;

        if(pfrom->nVersion < GetMinServicenodePaymentsProto())
        {
            return;
        }

        if(!pCurrentBlockIndex)
        {
            return;
        }

        uint256 nHash = vote.GetHash();

//        pfrom->setAskFor.erase(nHash);

        {
            LOCK(cs_mapServicenodePaymentVotes);
            if(mapServicenodePaymentVotes.count(nHash))
            {
                printf("SERVICENODEPAYMENTVOTE -- hash=%s, nHeight=%d seen\n",
                       nHash.ToString().c_str(), pCurrentBlockIndex->nHeight);
                return;
            }

            // Avoid processing same vote multiple times
            mapServicenodePaymentVotes[nHash] = vote;
            // but first mark vote as non-verified,
            // AddPaymentVote() below should take care of it if vote is actually ok
            mapServicenodePaymentVotes[nHash].MarkAsNotVerified();
        }

        int nFirstBlock = pCurrentBlockIndex->nHeight - GetStorageLimit();
        if(vote.nBlockHeight < nFirstBlock || vote.nBlockHeight > pCurrentBlockIndex->nHeight+20)
        {
            printf("SERVICENODEPAYMENTVOTE -- vote out of range: nFirstBlock=%d, nBlockHeight=%d, nHeight=%d\n",
                   nFirstBlock, vote.nBlockHeight, pCurrentBlockIndex->nHeight);
            return;
        }

        std::string strError = "";
        if(!vote.IsValid(pfrom, pCurrentBlockIndex->nHeight, strError))
        {
            printf("SERVICENODEPAYMENTVOTE -- invalid message, error: %s\n", strError.c_str());
            return;
        }

        if(!CanVote(vote.vinServicenode.prevout, vote.nBlockHeight))
        {
            printf("SERVICENODEPAYMENTVOTE -- servicenode already voted, servicenode=%s\n",
                   vote.vinServicenode.prevout.ToString().c_str());
            return;
        }

        servicenode_info_t mnInfo = mnodeman.GetServicenodeInfo(vote.vinServicenode);
        if(!mnInfo.fInfoValid)
        {
            // mn was not found, so we can't check vote, some info is probably missing
            printf("SERVICENODEPAYMENTVOTE -- servicenode is missing %s\n", vote.vinServicenode.prevout.ToString().c_str());
           mnodeman.AskForMN(pfrom, vote.vinServicenode);
            return;
        }

        int nDos = 0;
        if(!vote.CheckSignature(mnInfo.pubKeyServicenode, pCurrentBlockIndex->nHeight, nDos))
        {
            if(nDos)
            {
                printf("SERVICENODEPAYMENTVOTE -- ERROR: invalid signature\n");
                pfrom->Misbehaving(nDos);
            }
            else
            {
                // only warn about anything non-critical (i.e. nDos == 0) in debug mode
                printf("SERVICENODEPAYMENTVOTE -- WARNING: invalid signature\n");
            }
            // Either our info or vote info could be outdated.
            // In case our info is outdated, ask for an update,
            mnodeman.AskForMN(pfrom, vote.vinServicenode);
            // but there is nothing we can do if vote info itself is outdated
            // (i.e. it was signed by a mn which changed its key),
            // so just quit here.
            return;
        }

        CTxDestination address1;
        ExtractDestination(vote.payee, address1);
        CBitcoinAddress address2(address1);

        printf("SERVICENODEPAYMENTVOTE -- vote: address=%s, nBlockHeight=%d, nHeight=%d, prevout=%s\n",
               address2.ToString().c_str(), vote.nBlockHeight, pCurrentBlockIndex->nHeight, vote.vinServicenode.prevout.ToString().c_str());

        if(AddPaymentVote(vote))
        {
            vote.Relay();
            servicenodeSync.AddedPaymentVote();
        }
    }
}

bool CServicenodePaymentVote::Sign()
{
    std::string strError;
    std::string strMessage = vinServicenode.prevout.ToString() +
                boost::lexical_cast<std::string>(nBlockHeight) +
                payee.ToString();

    if(!DataSigner::SignMessage(strMessage, vchSig, activeServicenode.keyServicenode))
    {
        printf("CServicenodePaymentVote::Sign -- SignMessage() failed\n");
        return false;
    }

    if(!DataSigner::VerifyMessage(activeServicenode.pubKeyServicenode, vchSig, strMessage, strError))
    {
        printf("CServicenodePaymentVote::Sign -- VerifyMessage() failed, error: %s\n", strError.c_str());
        return false;
    }

    return true;
}

bool CServicenodePayments::GetBlockPayee(int nBlockHeight, CScript& payee)
{
    if(mapServicenodeBlocks.count(nBlockHeight)){
        return mapServicenodeBlocks[nBlockHeight].GetBestPayee(payee);
    }

    return false;
}

// Is this servicenode scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 blocks of votes
bool CServicenodePayments::IsScheduled(CServicenode& mn, int nNotBlockHeight)
{
    LOCK(cs_mapServicenodeBlocks);

    if(!pCurrentBlockIndex)
    {
        return false;
    }

    CScript mnpayee;
    mnpayee.SetDestination(mn.pubKeyCollateralAddress.GetID());

    CScript payee;
    for(int64_t h = pCurrentBlockIndex->nHeight; h <= pCurrentBlockIndex->nHeight + 8; h++)
    {
        if(h == nNotBlockHeight)
        {
            continue;
        }
        if(mapServicenodeBlocks.count(h) && mapServicenodeBlocks[h].GetBestPayee(payee) && mnpayee == payee)
        {
            return true;
        }
    }

    return false;
}

bool CServicenodePayments::AddPaymentVote(const CServicenodePaymentVote& vote)
{
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, vote.nBlockHeight - 101))
    {
        return false;
    }

    if(HasVerifiedPaymentVote(vote.GetHash()))
    {
        return false;
    }

    LOCK2(cs_mapServicenodeBlocks, cs_mapServicenodePaymentVotes);

    mapServicenodePaymentVotes[vote.GetHash()] = vote;

    if(!mapServicenodeBlocks.count(vote.nBlockHeight))
    {
       CServicenodeBlockPayees blockPayees(vote.nBlockHeight);
       mapServicenodeBlocks[vote.nBlockHeight] = blockPayees;
    }

    mapServicenodeBlocks[vote.nBlockHeight].AddPayee(vote);

    return true;
}

bool CServicenodePayments::HasVerifiedPaymentVote(uint256 hashIn)
{
    LOCK(cs_mapServicenodePaymentVotes);
    std::map<uint256, CServicenodePaymentVote>::iterator it = mapServicenodePaymentVotes.find(hashIn);
    return it != mapServicenodePaymentVotes.end() && it->second.IsVerified();
}

void CServicenodeBlockPayees::AddPayee(const CServicenodePaymentVote& vote)
{
    LOCK(cs_vecPayees);

    for (CServicenodePayee & payee : vecPayees) {
        if (payee.GetPayee() == vote.payee) {
            payee.AddVoteHash(vote.GetHash());
            return;
        }
    }
    CServicenodePayee payeeNew(vote.payee, vote.GetHash());
    vecPayees.push_back(payeeNew);
}

bool CServicenodeBlockPayees::GetBestPayee(CScript& payeeRet)
{
    LOCK(cs_vecPayees);

    if(!vecPayees.size()) {
        printf("CServicenodeBlockPayees::GetBestPayee -- ERROR: couldn't find any payee\n");
        return false;
    }

    int nVotes = -1;
    for (CServicenodePayee & payee : vecPayees) {
        if (payee.GetVoteCount() > nVotes) {
            payeeRet = payee.GetPayee();
            nVotes = payee.GetVoteCount();
        }
    }

    return (nVotes > -1);
}

bool CServicenodeBlockPayees::HasPayeeWithVotes(CScript payeeIn, int nVotesReq)
{
    LOCK(cs_vecPayees);

    for (CServicenodePayee & payee : vecPayees) {
        if (payee.GetVoteCount() >= nVotesReq && payee.GetPayee() == payeeIn) {
            return true;
        }
    }

    printf("CServicenodeBlockPayees::HasPayeeWithVotes -- ERROR: couldn't find any payee with %d+ votes\n", nVotesReq);
    return false;
}

bool CServicenodeBlockPayees::IsTransactionValid(const CTransaction& txNew)
{
    LOCK(cs_vecPayees);

    int nMaxSignatures = 0;
    std::string strPayeesPossible = "";

    CAmount nServicenodePayment = GetServicenodePayment(nBlockHeight, txNew.GetValueOut());

    //require at least MNPAYMENTS_SIGNATURES_REQUIRED signatures

    for (CServicenodePayee & payee : vecPayees)
    {
        if (payee.GetVoteCount() >= nMaxSignatures)
        {
            nMaxSignatures = payee.GetVoteCount();
        }
    }

    // if we don't have at least MNPAYMENTS_SIGNATURES_REQUIRED signatures on a payee, approve whichever is the longest chain
    if(nMaxSignatures < MNPAYMENTS_SIGNATURES_REQUIRED)
    {
        return true;
    }

    for (CServicenodePayee & payee : vecPayees)
    {
        if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED)
        {
            for (const CTxOut & txout : txNew.vout)
            {
                if (payee.GetPayee() == txout.scriptPubKey && nServicenodePayment == txout.nValue)
                {
                    printf("CServicenodeBlockPayees::IsTransactionValid -- Found required payment\n");
                    return true;
                }
            }

            CTxDestination address1;
            ExtractDestination(payee.GetPayee(), address1);
            CBitcoinAddress address2(address1);

            if(strPayeesPossible == "")
            {
                strPayeesPossible = address2.ToString();
            }
            else
            {
                strPayeesPossible += "," + address2.ToString();
            }
        }
    }

    printf("CServicenodeBlockPayees::IsTransactionValid -- ERROR: Missing required payment, possible payees: '%s', amount: %f BLOCK\n",
           strPayeesPossible.c_str(), (float)nServicenodePayment/COIN);
    return false;
}

std::string CServicenodeBlockPayees::GetRequiredPaymentsString()
{
    LOCK(cs_vecPayees);

    std::string strRequiredPayments = "Unknown";

    for (CServicenodePayee & payee : vecPayees)
    {
        CTxDestination address1;
        ExtractDestination(payee.GetPayee(), address1);
        CBitcoinAddress address2(address1);

        if (strRequiredPayments != "Unknown") {
            strRequiredPayments += ", " + address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        } else {
            strRequiredPayments = address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        }
    }

    return strRequiredPayments;
}

std::string CServicenodePayments::GetRequiredPaymentsString(int nBlockHeight)
{
    LOCK(cs_mapServicenodeBlocks);

    if(mapServicenodeBlocks.count(nBlockHeight)){
        return mapServicenodeBlocks[nBlockHeight].GetRequiredPaymentsString();
    }

    return "Unknown";
}

bool CServicenodePayments::IsTransactionValid(const CTransaction& txNew, int nBlockHeight)
{
    LOCK(cs_mapServicenodeBlocks);

    if(mapServicenodeBlocks.count(nBlockHeight)){
        return mapServicenodeBlocks[nBlockHeight].IsTransactionValid(txNew);
    }

    return true;
}

void CServicenodePayments::CheckAndRemove()
{
    if(!pCurrentBlockIndex) return;

    LOCK2(cs_mapServicenodeBlocks, cs_mapServicenodePaymentVotes);

    int nLimit = GetStorageLimit();

    std::map<uint256, CServicenodePaymentVote>::iterator it = mapServicenodePaymentVotes.begin();
    while(it != mapServicenodePaymentVotes.end()) {
        CServicenodePaymentVote vote = (*it).second;

        if(pCurrentBlockIndex->nHeight - vote.nBlockHeight > nLimit) {
            printf("CServicenodePayments::CheckAndRemove -- Removing old Servicenode payment: nBlockHeight=%d\n", vote.nBlockHeight);
            mapServicenodePaymentVotes.erase(it++);
            mapServicenodeBlocks.erase(vote.nBlockHeight);
        } else {
            ++it;
        }
    }
    printf("CServicenodePayments::CheckAndRemove -- %s\n", ToString().c_str());
}

bool CServicenodePaymentVote::IsValid(CNode* pnode, int nValidationHeight, std::string& strError)
{
    CServicenode* pmn = mnodeman.Find(vinServicenode);

    if(!pmn) {
        strError = strprintf("Unknown Servicenode: prevout=%s", vinServicenode.prevout.ToString().c_str());
        // Only ask if we are already synced and still have no idea about that Servicenode
        if(servicenodeSync.IsServicenodeListSynced()) {
            mnodeman.AskForMN(pnode, vinServicenode);
        }

        return false;
    }

    int nMinRequiredProtocol;
    if(nBlockHeight >= nValidationHeight) {
        // new votes must comply SPORK_10_SERVICENODE_PAY_UPDATED_NODES rules
        nMinRequiredProtocol = mnpayments.GetMinServicenodePaymentsProto();
    } else {
        // allow non-updated servicenodes for old blocks
        nMinRequiredProtocol = MIN_SERVICENODE_PAYMENT_PROTO_VERSION_1;
    }

    if(pmn->nProtocolVersion < nMinRequiredProtocol) {
        strError = strprintf("Servicenode protocol is too old: nProtocolVersion=%d, nMinRequiredProtocol=%d", pmn->nProtocolVersion, nMinRequiredProtocol);
        return false;
    }

    // Only servicenodes should try to check servicenode rank for old votes - they need to pick the right winner for future blocks.
    // Regular clients (miners included) need to verify servicenode rank for future block votes only.
    if(!fServiceNode && nBlockHeight < nValidationHeight) return true;

    int nRank = mnodeman.GetServicenodeRank(vinServicenode, nBlockHeight - 101, nMinRequiredProtocol, false);

    if(nRank == -1) {
        printf("CServicenodePaymentVote::IsValid -- Can't calculate rank for servicenode %s\n",
                    vinServicenode.prevout.ToString().c_str());
        return false;
    }

    if(nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        // It's common to have servicenodes mistakenly think they are in the top 10
        // We don't want to print all of these messages in normal mode, debug mode should print though
        strError = strprintf("Servicenode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        // Only ban for new mnw which is out of bounds, for old mnw MN list itself might be way too much off
        if(nRank > MNPAYMENTS_SIGNATURES_TOTAL*2 && nBlockHeight > nValidationHeight) {
            strError = strprintf("Servicenode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL*2, nRank);
            printf("CServicenodePaymentVote::IsValid -- Error: %s\n", strError.c_str());
            pnode->Misbehaving(20);
        }
        // Still invalid however
        return false;
    }

    return true;
}

bool CServicenodePayments::ProcessBlock(int nBlockHeight)
{
    // DETERMINE IF WE SHOULD BE VOTING FOR THE NEXT PAYEE

    // if(fLiteMode || !fServiceNode) return false;
    if(!fServiceNode) return false;

    // We have little chances to pick the right winner if winners list is out of sync
    // but we have no choice, so we'll try. However it doesn't make sense to even try to do so
    // if we have not enough data about servicenodes.
    if(!servicenodeSync.IsServicenodeListSynced()) return false;

    int nRank = mnodeman.GetServicenodeRank(activeServicenode.vin, nBlockHeight - 101, GetMinServicenodePaymentsProto(), false);

    if (nRank == -1) {
        printf("CServicenodePayments::ProcessBlock -- Unknown Servicenode\n");
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        printf("CServicenodePayments::ProcessBlock -- Servicenode not in the top %d (%d)\n", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        return false;
    }


    // LOCATE THE NEXT SERVICENODE WHICH SHOULD BE PAID

    printf("CServicenodePayments::ProcessBlock -- Start: nBlockHeight=%d, servicenode=%s\n", nBlockHeight, activeServicenode.vin.prevout.ToString().c_str());

    // pay to the oldest MN that still had no payment but its input is old enough and it was active long enough
    int nCount = 0;
    CServicenode *pmn = mnodeman.GetNextServicenodeInQueueForPayment(nBlockHeight, true, nCount);

    if (pmn == NULL) {
        printf("CServicenodePayments::ProcessBlock -- ERROR: Failed to find servicenode to pay\n");
        return false;
    }

    printf("CServicenodePayments::ProcessBlock -- Servicenode found by GetNextServicenodeInQueueForPayment(): %s\n", pmn->vin.prevout.ToString().c_str());


    CScript payee;
    payee.SetDestination(pmn->pubKeyCollateralAddress.GetID());

    CServicenodePaymentVote voteNew(activeServicenode.vin, nBlockHeight, payee);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);

    printf("CServicenodePayments::ProcessBlock -- vote: payee=%s, nBlockHeight=%d\n", address2.ToString().c_str(), nBlockHeight);

    // SIGN MESSAGE TO NETWORK WITH OUR SERVICENODE KEYS

    printf("CServicenodePayments::ProcessBlock -- Signing vote\n");
    if (voteNew.Sign()) {
        printf("CServicenodePayments::ProcessBlock -- AddPaymentVote()\n");

        if (AddPaymentVote(voteNew)) {
            voteNew.Relay();
            return true;
        }
    }

    return false;
}

void CServicenodePaymentVote::Relay()
{
    // do not relay until synced
    if (!servicenodeSync.IsWinnersListSynced()) return;
    CInv inv(MSG_SERVICENODE_PAYMENT_VOTE, GetHash());
    RelayInventory(inv);
}

bool CServicenodePaymentVote::CheckSignature(const CPubKey& pubKeyServicenode, int nValidationHeight, int &nDos)
{
    // do not ban by default
    nDos = 0;

    std::string strMessage = vinServicenode.prevout.ToString() +
                boost::lexical_cast<std::string>(nBlockHeight) +
                payee.ToString();

    std::string strError = "";

    if (!DataSigner::VerifyMessage(pubKeyServicenode, vchSig, strMessage, strError))
    {
        // Only ban for future block vote when we are already synced.
        // Otherwise it could be the case when MN which signed this vote is using another key now
        // and we have no idea about the old one.
        if(servicenodeSync.IsServicenodeListSynced() && nBlockHeight > nValidationHeight)
        {
            nDos = 20;
        }
        return error("CServicenodePaymentVote::CheckSignature -- Got bad Servicenode payment signature, servicenode=%s, error: %s", vinServicenode.prevout.ToString().c_str(), strError.c_str());
    }

    return true;
}

std::string CServicenodePaymentVote::ToString() const
{
    std::ostringstream info;

    info << vinServicenode.prevout.ToString() <<
            ", " << nBlockHeight <<
            ", " << payee.ToString() <<
            ", " << (int)vchSig.size();

    return info.str();
}

// Send only votes for future blocks, node should request every other missing payment block individually
void CServicenodePayments::Sync(CNode* pnode)
{
    LOCK(cs_mapServicenodeBlocks);

    if(!pCurrentBlockIndex) return;

    int nInvCount = 0;

    for(int h = pCurrentBlockIndex->nHeight; h < pCurrentBlockIndex->nHeight + 20; h++) {
        if(mapServicenodeBlocks.count(h)) {
            for (CServicenodePayee & payee : mapServicenodeBlocks[h].vecPayees) {
                std::vector<uint256> vecVoteHashes = payee.GetVoteHashes();
                for (uint256 & hash : vecVoteHashes) {
                    if(!HasVerifiedPaymentVote(hash)) continue;
                    pnode->PushInventory(CInv(MSG_SERVICENODE_PAYMENT_VOTE, hash));
                    nInvCount++;
                }
            }
        }
    }

    printf("CServicenodePayments::Sync -- Sent %d votes to peer %s\n", nInvCount, pnode->addr.ToString().c_str());
    pnode->PushMessage(NetMsgType::SYNCSTATUSCOUNT, SERVICENODE_SYNC_MNW, nInvCount);
}

// Request low data/unknown payment blocks in batches directly from some node instead of/after preliminary Sync.
void CServicenodePayments::RequestLowDataPaymentBlocks(CNode* pnode)
{
    if(!pCurrentBlockIndex)
    {
        return;
    }

    LOCK2(cs_main, cs_mapServicenodeBlocks);

    std::vector<CInv> vToFetch;
    int nLimit = GetStorageLimit();

    const CBlockIndex *pindex = pCurrentBlockIndex;

    while(pCurrentBlockIndex->nHeight - pindex->nHeight < nLimit)
    {
        if(!mapServicenodeBlocks.count(pindex->nHeight))
        {
            // We have no idea about this block height, let's ask
            vToFetch.push_back(CInv(MSG_SERVICENODE_PAYMENT_BLOCK, pindex->GetBlockHash()));
            // We should not violate GETDATA rules
            if(vToFetch.size() == MAX_INV_SZ)
            {
                printf("CServicenodePayments::SyncLowDataPaymentBlocks -- asking peer %s for %d blocks\n",
                       pnode->addr.ToString().c_str(), MAX_INV_SZ);
                pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
                // Start filling new batch
                vToFetch.clear();
            }
        }
        if(!pindex->pprev) break;
        pindex = pindex->pprev;
    }

    std::map<int, CServicenodeBlockPayees>::iterator it = mapServicenodeBlocks.begin();

    while(it != mapServicenodeBlocks.end())
    {
        int nTotalVotes = 0;
        bool fFound = false;
        for (CServicenodePayee & payee : it->second.vecPayees)
        {
            if(payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED)
            {
                fFound = true;
                break;
            }
            nTotalVotes += payee.GetVoteCount();
        }
        // A clear winner (MNPAYMENTS_SIGNATURES_REQUIRED+ votes) was found
        // or no clear winner was found but there are at least avg number of votes
        if(fFound || nTotalVotes >= (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED)/2)
        {
            // so just move to the next block
            ++it;
            continue;
        }
        // DEBUG
//        DBG (
//            // Let's see why this failed
//            for (CServicenodePayee & payee : it->second.vecPayees) {
//                CTxDestination address1;
//                ExtractDestination(payee.GetPayee(), address1);
//                CBitcoinAddress address2(address1);
//                printf("payee %s votes %d\n", address2.ToString().c_str(), payee.GetVoteCount());
//            }
//            printf("block %d votes total %d\n", it->first, nTotalVotes);
//        )
//        // END DEBUG
        // Low data block found, let's try to sync it
        uint256 hash;
        if(GetBlockHash(hash, it->first))
        {
            vToFetch.push_back(CInv(MSG_SERVICENODE_PAYMENT_BLOCK, hash));
        }
        // We should not violate GETDATA rules
        if(vToFetch.size() == MAX_INV_SZ)
        {
            printf("CServicenodePayments::SyncLowDataPaymentBlocks -- asking peer %s for %d payment blocks\n",
                   pnode->addr.ToString().c_str(), MAX_INV_SZ);
            pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
            // Start filling new batch
            vToFetch.clear();
        }
        ++it;
    }
    // Ask for the rest of it
    if(!vToFetch.empty())
    {
        printf("CServicenodePayments::SyncLowDataPaymentBlocks -- asking peer %s for %d payment blocks\n",
               pnode->addr.ToString().c_str(), vToFetch.size());
        pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
    }
}

std::string CServicenodePayments::ToString() const
{
    std::ostringstream info;

    info << "Votes: " << (int)mapServicenodePaymentVotes.size() <<
            ", Blocks: " << (int)mapServicenodeBlocks.size();

    return info.str();
}

bool CServicenodePayments::IsEnoughData()
{
    float nAverageVotes = (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2;
    int nStorageLimit = GetStorageLimit();
    return GetBlockCount() > nStorageLimit && GetVoteCount() > nStorageLimit * nAverageVotes;
}

int CServicenodePayments::GetStorageLimit()
{
    return std::max(int(mnodeman.size() * nStorageCoeff), nMinBlocksToStore);
}

void CServicenodePayments::UpdatedBlockTip(const CBlockIndex *pindex)
{
    pCurrentBlockIndex = pindex;
    printf("CServicenodePayments::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);

    ProcessBlock(pindex->nHeight + 10);
}
