// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activemasternode.h"
#include "masternode.h"
#include "masternode-sync.h"
#include "masternodeman.h"
#include "masternodeconfig.h"
#include "protocol.h"

extern CWallet* pwalletMain;

// Keep track of the active Masternode
CActiveMasternode activeMasternode;

void CActiveMasternode::ManageState()
{
    printf("CActiveMasternode::ManageState -- Start\n");
    if(!fMasterNode)
    {
        printf("CActiveMasternode::ManageState -- Not a masternode, returning\n");
        return;
    }

    if (!masternodeSync.IsBlockchainSynced())
    {
        nState = ACTIVE_MASTERNODE_SYNC_IN_PROCESS;
        printf("CActiveMasternode::ManageState -- %s: %s\n", GetStateString().c_str(), GetStatus().c_str());
        return;
    }

    if (nState == ACTIVE_MASTERNODE_SYNC_IN_PROCESS)
    {
        nState = ACTIVE_MASTERNODE_INITIAL;
    }

    printf("CActiveMasternode::ManageState -- status = %s, type = %s, pinger enabled = %d\n", GetStatus().c_str(), GetTypeString().c_str(), fPingerEnabled);

    if (eType == MASTERNODE_UNKNOWN)
    {
        ManageStateInitial();
    }

    if(eType == MASTERNODE_REMOTE)
    {
        ManageStateRemote();
    }
    else if(eType == MASTERNODE_LOCAL)
    {
        // Try Remote Start first so the started local masternode can be restarted without recreate masternode broadcast.
        ManageStateRemote();
        if(nState != ACTIVE_MASTERNODE_STARTED)
        {
            ManageStateLocal();
        }
    }

    SendMasternodePing();
}

std::string CActiveMasternode::GetStateString() const
{
    switch (nState) {
        case ACTIVE_MASTERNODE_INITIAL:         return "INITIAL";
        case ACTIVE_MASTERNODE_SYNC_IN_PROCESS: return "SYNC_IN_PROCESS";
        case ACTIVE_MASTERNODE_INPUT_TOO_NEW:   return "INPUT_TOO_NEW";
        case ACTIVE_MASTERNODE_NOT_CAPABLE:     return "NOT_CAPABLE";
        case ACTIVE_MASTERNODE_STARTED:         return "STARTED";
        default:                                return "UNKNOWN";
    }
}

std::string CActiveMasternode::GetStatus() const
{
    switch (nState) {
        case ACTIVE_MASTERNODE_INITIAL:         return "Node just started, not yet activated";
        case ACTIVE_MASTERNODE_SYNC_IN_PROCESS: return "Sync in progress. Must wait until sync is complete to start Masternode";
        case ACTIVE_MASTERNODE_INPUT_TOO_NEW:   return strprintf("Masternode input must have at least %d confirmations", nMasternodeMinimumConfirmations);
        case ACTIVE_MASTERNODE_NOT_CAPABLE:     return "Not capable masternode: " + strNotCapableReason;
        case ACTIVE_MASTERNODE_STARTED:         return "Masternode successfully started";
        default:                                return "Unknown";
    }
}

std::string CActiveMasternode::GetTypeString() const
{
    std::string strType;
    switch(eType) {
    case MASTERNODE_UNKNOWN:
        strType = "UNKNOWN";
        break;
    case MASTERNODE_REMOTE:
        strType = "REMOTE";
        break;
    case MASTERNODE_LOCAL:
        strType = "LOCAL";
        break;
    default:
        strType = "UNKNOWN";
        break;
    }
    return strType;
}

bool CActiveMasternode::SendMasternodePing()
{
    if(!fPingerEnabled)
    {
        printf("CActiveMasternode::SendMasternodePing -- %s: masternode ping service is disabled, skipping...\n", GetStateString().c_str());
        return false;
    }

    if(!mnodeman.Has(vin))
    {
        strNotCapableReason = "Masternode not in masternode list";
        nState = ACTIVE_MASTERNODE_NOT_CAPABLE;
        printf("CActiveMasternode::SendMasternodePing -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
        return false;
    }

    CMasternodePing mnp(vin);
    if(!mnp.Sign(keyMasternode, pubKeyMasternode))
    {
        printf("CActiveMasternode::SendMasternodePing -- ERROR: Couldn't sign Masternode Ping\n");
        return false;
    }

    // Update lastPing for our masternode in Masternode list
    if(mnodeman.IsMasternodePingedWithin(vin, MASTERNODE_MIN_MNP_SECONDS, mnp.sigTime))
    {
        printf("CActiveMasternode::SendMasternodePing -- Too early to send Masternode Ping\n");
        return false;
    }

    mnodeman.SetMasternodeLastPing(vin, mnp);

    printf("CActiveMasternode::SendMasternodePing -- Relaying ping, collateral=%s\n", vin.ToString().c_str());
    mnp.Relay();

    return true;
}

void CActiveMasternode::ManageStateInitial()
{
    printf("CActiveMasternode::ManageStateInitial -- status = %s, type = %s, pinger enabled = %d\n", GetStatus().c_str(), GetTypeString().c_str(), fPingerEnabled);

    // Check that our local network configuration is correct
    if (fNoListen)
    {
        // listen option is probably overwritten by smth else, no good
        nState = ACTIVE_MASTERNODE_NOT_CAPABLE;
        strNotCapableReason = "Masternode must accept connections from outside. Make sure listen configuration option is not overwritten by some another parameter.";
        printf("CActiveMasternode::ManageStateInitial -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
        return;
    }

    bool fFoundLocal = false;
    {
        LOCK(cs_vNodes);

        // First try to find whatever local address is specified by externalip option
        fFoundLocal = GetLocal(service) && CMasternode::IsValidNetAddr(service);
        if(!fFoundLocal)
        {
            // nothing and no live connections, can't do anything for now
            if (vNodes.empty())
            {
                nState = ACTIVE_MASTERNODE_NOT_CAPABLE;
                strNotCapableReason = "Can't detect valid external address. Will retry when there are some connections available.";
                printf("CActiveMasternode::ManageStateInitial -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
                return;
            }
            // We have some peers, let's try to find our local address from one of them
            for (CNode * pnode : vNodes)
            {
                if (pnode->fSuccessfullyConnected && pnode->addr.IsIPv4())
                {
                    fFoundLocal = GetLocal(service, &pnode->addr) && CMasternode::IsValidNetAddr(service);
                    if(fFoundLocal)
                    {
                        break;
                    }
                }
            }
        }
    }

    if(!fFoundLocal)
    {
        nState = ACTIVE_MASTERNODE_NOT_CAPABLE;
        strNotCapableReason = "Can't detect valid external address. Please consider using the externalip configuration option if problem persists. Make sure to use IPv4 address only.";
        printf("CActiveMasternode::ManageStateInitial -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
        return;
    }

    int mainnetDefaultPort = GetDefaultPort(false);
    if (!fTestNet)
    {
        if(service.GetPort() != mainnetDefaultPort)
        {
            nState = ACTIVE_MASTERNODE_NOT_CAPABLE;
            strNotCapableReason = strprintf("Invalid port: %u - only %d is supported on mainnet.", service.GetPort(), mainnetDefaultPort);
            printf("CActiveMasternode::ManageStateInitial -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
            return;
        }
    }
    else if(service.GetPort() == mainnetDefaultPort)
    {
        nState = ACTIVE_MASTERNODE_NOT_CAPABLE;
        strNotCapableReason = strprintf("Invalid port: %u - %d is only supported on mainnet.", service.GetPort(), mainnetDefaultPort);
        printf("CActiveMasternode::ManageStateInitial -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
        return;
    }

    printf("CActiveMasternode::ManageStateInitial -- Checking inbound connection to '%s'\n", service.ToString().c_str());

    if (!ConnectNode((CAddress)service, NULL, true))
    {
        nState = ACTIVE_MASTERNODE_NOT_CAPABLE;
        strNotCapableReason = "Could not connect to " + service.ToString();
        printf("CActiveMasternode::ManageStateInitial -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
        return;
    }

    // Default to REMOTE
    eType = MASTERNODE_REMOTE;

    // Check if wallet funds are available
    if(!pwalletMain)
    {
        printf("CActiveMasternode::ManageStateInitial -- %s: Wallet not available\n", GetStateString().c_str());
        return;
    }

    if(pwalletMain->IsLocked())
    {
        printf("CActiveMasternode::ManageStateInitial -- %s: Wallet is locked\n", GetStateString().c_str());
        return;
    }

    if (pwalletMain->GetBalance() < MASTERNODE_AMOUNT*COIN)
    {
        printf("CActiveMasternode::ManageStateInitial -- %s: Wallet balance is < %d BLOCKS\n", GetStateString().c_str(), MASTERNODE_AMOUNT);
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    // If collateral is found switch to LOCAL mode
    if(pwalletMain->GetMasternodeVinAndKeys(vin, pubKeyCollateral, keyCollateral))
    {
        eType = MASTERNODE_LOCAL;
    }

    printf("CActiveMasternode::ManageStateInitial -- End status = %s, type = %s, pinger enabled = %d\n", GetStatus().c_str(), GetTypeString().c_str(), fPingerEnabled);
}

void CActiveMasternode::ManageStateRemote()
{
    printf("CActiveMasternode::ManageStateRemote -- Start status = %s, type = %s, pinger enabled = %d, pubKeyMasternode.GetID() = %s\n",
             GetStatus().c_str(), fPingerEnabled, GetTypeString().c_str(), pubKeyMasternode.GetID().ToString().c_str());

    mnodeman.CheckMasternode(pubKeyMasternode);
    masternode_info_t infoMn = mnodeman.GetMasternodeInfo(pubKeyMasternode);
    if (infoMn.fInfoValid)
    {
        if (infoMn.nProtocolVersion != PROTOCOL_VERSION)
        {
            nState = ACTIVE_MASTERNODE_NOT_CAPABLE;
            strNotCapableReason = "Invalid protocol version";
            printf("CActiveMasternode::ManageStateRemote -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
            return;
        }
        if(service != infoMn.addr)
        {
            nState = ACTIVE_MASTERNODE_NOT_CAPABLE;
            strNotCapableReason = "Broadcasted IP doesn't match our external address. Make sure you issued a new broadcast if IP of this masternode changed recently.";
            printf("CActiveMasternode::ManageStateRemote -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
            return;
        }
        if  (!CMasternode::IsValidStateForAutoStart(infoMn.nActiveState))
        {
           nState = ACTIVE_MASTERNODE_NOT_CAPABLE;
            strNotCapableReason = strprintf("Masternode in %s state", CMasternode::StateToString(infoMn.nActiveState));
            printf("CActiveMasternode::ManageStateRemote -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
            return;
        }
        if (nState != ACTIVE_MASTERNODE_STARTED)
        {
            printf("CActiveMasternode::ManageStateRemote -- STARTED!\n");
            vin = infoMn.vin;
            service = infoMn.addr;
            fPingerEnabled = true;
            nState = ACTIVE_MASTERNODE_STARTED;
        }
    }
    else
    {
        nState = ACTIVE_MASTERNODE_NOT_CAPABLE;
        strNotCapableReason = "Masternode not in masternode list";
        printf("CActiveMasternode::ManageStateRemote -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
    }
}

void CActiveMasternode::ManageStateLocal()
{
    printf("CActiveMasternode::ManageStateLocal -- status = %s, type = %s, pinger enabled = %d\n", GetStatus().c_str(), GetTypeString().c_str(), fPingerEnabled);
    if (nState == ACTIVE_MASTERNODE_STARTED)
    {
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    if (pwalletMain->GetMasternodeVinAndKeys(vin, pubKeyCollateral, keyCollateral))
    {
        int nInputAge = GetInputDepthInMainChain(vin);
        if (nInputAge < nMasternodeMinimumConfirmations)
        {
            nState = ACTIVE_MASTERNODE_INPUT_TOO_NEW;
            strNotCapableReason = strprintf(_("%s - %d confirmations"), GetStatus(), nInputAge);
            printf("CActiveMasternode::ManageStateLocal -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
            return;
        }

        {
            LOCK(pwalletMain->cs_wallet);
            pwalletMain->LockCoin(vin.prevout);
        }

        CMasternodeBroadcast mnb;
        std::string strError;
        if (!CMasternodeBroadcast::Create(vin, service, keyCollateral, pubKeyCollateral, keyMasternode, pubKeyMasternode, strError, mnb))
        {
            nState = ACTIVE_MASTERNODE_NOT_CAPABLE;
            strNotCapableReason = "Error creating mastenode broadcast: " + strError;
            printf("CActiveMasternode::ManageStateLocal -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
            return;
        }

        fPingerEnabled = true;
        nState = ACTIVE_MASTERNODE_STARTED;

        // update to masternode list
        printf("CActiveMasternode::ManageStateLocal -- Update Masternode List\n");
        mnodeman.UpdateMasternodeList(mnb);
        mnodeman.NotifyMasternodeUpdates();

        //send to all peers
        printf("CActiveMasternode::ManageStateLocal -- Relay broadcast, vin=%s\n", vin.ToString().c_str());
        mnb.Relay();
    }
}
