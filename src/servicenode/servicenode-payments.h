// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SERVICENODE_PAYMENTS_H
#define SERVICENODE_PAYMENTS_H

#include "serialize.h"
#include "util.h"
// #include "core_io.h"
#include "key.h"
#include "main.h"
#include "servicenode.h"
// #include "utilstrencodings.h"

class CServicenodePayments;
class CServicenodePaymentVote;
class CServicenodeBlockPayees;

static const int MNPAYMENTS_SIGNATURES_REQUIRED         = 6;
static const int MNPAYMENTS_SIGNATURES_TOTAL            = 10;

//! minimum peer version that can receive and send servicenode payment messages,
//  vote for servicenode and be elected as a payment winner
// V1 - Last protocol version before update
// V2 - Newest protocol version
static const int MIN_SERVICENODE_PAYMENT_PROTO_VERSION_1 = 60027;
static const int MIN_SERVICENODE_PAYMENT_PROTO_VERSION_2 = 60027;

extern CCriticalSection cs_vecPayees;
extern CCriticalSection cs_mapServicenodeBlocks;
extern CCriticalSection cs_mapServicenodePayeeVotes;

extern CServicenodePayments mnpayments;

/// TODO: all 4 functions do not belong here really, they should be refactored/moved somewhere (main.cpp ?)
bool IsBlockValueValid(const CBlock& block, int nBlockHeight, CAmount blockReward, std::string &strErrorRet);
bool IsBlockPayeeValid(const CTransaction& txNew, int nBlockHeight, CAmount blockReward);
void FillBlockPayments(CMutableTransaction& txNew, int nBlockHeight, CAmount blockReward, CTxOut& txoutServicenodeRet, std::vector<CTxOut>& voutSuperblockRet);
std::string GetRequiredPaymentsString(int nBlockHeight);

class CServicenodePayee
{
private:
    CScript scriptPubKey;
    std::vector<uint256> vecVoteHashes;

public:
    CServicenodePayee() :
        scriptPubKey(),
        vecVoteHashes()
        {}

    CServicenodePayee(CScript payee, uint256 hashIn) :
        scriptPubKey(payee),
        vecVoteHashes()
    {
        vecVoteHashes.push_back(hashIn);
    }

    IMPLEMENT_SERIALIZE(
        READWRITE(scriptPubKey);
        READWRITE(vecVoteHashes);
    )

    CScript GetPayee() { return scriptPubKey; }

    void AddVoteHash(uint256 hashIn) { vecVoteHashes.push_back(hashIn); }
    std::vector<uint256> GetVoteHashes() { return vecVoteHashes; }
    int GetVoteCount() { return vecVoteHashes.size(); }
};

// Keep track of votes for payees from servicenodes
class CServicenodeBlockPayees
{
public:
    int nBlockHeight;
    std::vector<CServicenodePayee> vecPayees;

    CServicenodeBlockPayees() :
        nBlockHeight(0),
        vecPayees()
        {}
    CServicenodeBlockPayees(int nBlockHeightIn) :
        nBlockHeight(nBlockHeightIn),
        vecPayees()
        {}

    IMPLEMENT_SERIALIZE(
        READWRITE(nBlockHeight);
        READWRITE(vecPayees);
    )

    void AddPayee(const CServicenodePaymentVote& vote);
    bool GetBestPayee(CScript& payeeRet);
    bool HasPayeeWithVotes(CScript payeeIn, int nVotesReq);

    bool IsTransactionValid(const CTransaction& txNew);

    std::string GetRequiredPaymentsString();
};

// vote for the winning payment
class CServicenodePaymentVote
{
public:
    CTxIn vinServicenode;

    int nBlockHeight;
    CScript payee;
    std::vector<unsigned char> vchSig;

    CServicenodePaymentVote() :
        vinServicenode(),
        nBlockHeight(0),
        payee(),
        vchSig()
        {}

    CServicenodePaymentVote(CTxIn vinServicenode, int nBlockHeight, CScript payee) :
        vinServicenode(vinServicenode),
        nBlockHeight(nBlockHeight),
        payee(payee),
        vchSig()
        {}

    IMPLEMENT_SERIALIZE(
        READWRITE(vinServicenode);
        READWRITE(nBlockHeight);
        READWRITE(payee);
        READWRITE(vchSig);
    )

    uint256 GetHash() const {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << payee;
        ss << nBlockHeight;
        ss << vinServicenode.prevout;
        return ss.GetHash();
    }

    bool Sign();
    bool CheckSignature(const CPubKey& pubKeyServicenode, int nValidationHeight, int &nDos);

    bool IsValid(CNode* pnode, int nValidationHeight, std::string& strError);
    void Relay();

    bool IsVerified() { return !vchSig.empty(); }
    void MarkAsNotVerified() { vchSig.clear(); }

    std::string ToString() const;
};

//
// Servicenode Payments Class
// Keeps track of who should get paid for which blocks
//

class CServicenodePayments
{
private:
    // servicenode count times nStorageCoeff payments blocks should be stored ...
    const float nStorageCoeff;
    // ... but at least nMinBlocksToStore (payments blocks)
    const int nMinBlocksToStore;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

public:
    std::map<uint256, CServicenodePaymentVote> mapServicenodePaymentVotes;
    std::map<int, CServicenodeBlockPayees> mapServicenodeBlocks;
    std::map<COutPoint, int> mapServicenodesLastVote;

    CServicenodePayments() : nStorageCoeff(1.25), nMinBlocksToStore(5000) {}

    IMPLEMENT_SERIALIZE(
        READWRITE(mapServicenodePaymentVotes);
        READWRITE(mapServicenodeBlocks);
    )

    void Clear();

    bool AddPaymentVote(const CServicenodePaymentVote& vote);
    bool HasVerifiedPaymentVote(uint256 hashIn);
    bool ProcessBlock(int nBlockHeight);

    void Sync(CNode* node);
    void RequestLowDataPaymentBlocks(CNode* pnode);
    void CheckAndRemove();

    bool GetBlockPayee(int nBlockHeight, CScript& payee);
    bool IsTransactionValid(const CTransaction& txNew, int nBlockHeight);
    bool IsScheduled(CServicenode& mn, int nNotBlockHeight);

    bool CanVote(COutPoint outServicenode, int nBlockHeight);

    int GetMinServicenodePaymentsProto();
    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    std::string GetRequiredPaymentsString(int nBlockHeight);
    void FillBlockPayee(CMutableTransaction& txNew, int nBlockHeight, CAmount blockReward, CTxOut& txoutServicenodeRet);
    std::string ToString() const;

    int GetBlockCount() { return mapServicenodeBlocks.size(); }
    int GetVoteCount() { return mapServicenodePaymentVotes.size(); }

    bool IsEnoughData();
    int GetStorageLimit();

    void UpdatedBlockTip(const CBlockIndex *pindex);
};

#endif
