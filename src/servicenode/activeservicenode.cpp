// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeservicenode.h"
#include "servicenode.h"
#include "servicenode-sync.h"
#include "servicenodeman.h"
#include "servicenodeconfig.h"
#include "protocol.h"

extern CWallet* pwalletMain;

// Keep track of the active Servicenode
CActiveServicenode activeServicenode;

void CActiveServicenode::ManageState()
{
    printf("CActiveServicenode::ManageState -- Start\n");
    if(!fServiceNode)
    {
        printf("CActiveServicenode::ManageState -- Not a servicenode, returning\n");
        return;
    }

    if (!servicenodeSync.IsBlockchainSynced())
    {
        nState = ACTIVE_SERVICENODE_SYNC_IN_PROCESS;
        printf("CActiveServicenode::ManageState -- %s: %s\n", GetStateString().c_str(), GetStatus().c_str());
        return;
    }

    if (nState == ACTIVE_SERVICENODE_SYNC_IN_PROCESS)
    {
        nState = ACTIVE_SERVICENODE_INITIAL;
    }

    printf("CActiveServicenode::ManageState -- status = %s, type = %s, pinger enabled = %d\n", GetStatus().c_str(), GetTypeString().c_str(), fPingerEnabled);

    if (eType == SERVICENODE_UNKNOWN)
    {
        ManageStateInitial();
    }

    if(eType == SERVICENODE_REMOTE)
    {
        ManageStateRemote();
    }
    else if(eType == SERVICENODE_LOCAL)
    {
        // Try Remote Start first so the started local servicenode can be restarted without recreate servicenode broadcast.
        ManageStateRemote();
        if(nState != ACTIVE_SERVICENODE_STARTED)
        {
            ManageStateLocal();
        }
    }

    SendServicenodePing();
}

std::string CActiveServicenode::GetStateString() const
{
    switch (nState) {
        case ACTIVE_SERVICENODE_INITIAL:         return "INITIAL";
        case ACTIVE_SERVICENODE_SYNC_IN_PROCESS: return "SYNC_IN_PROCESS";
        case ACTIVE_SERVICENODE_INPUT_TOO_NEW:   return "INPUT_TOO_NEW";
        case ACTIVE_SERVICENODE_NOT_CAPABLE:     return "NOT_CAPABLE";
        case ACTIVE_SERVICENODE_STARTED:         return "STARTED";
        default:                                return "UNKNOWN";
    }
}

std::string CActiveServicenode::GetStatus() const
{
    switch (nState) {
        case ACTIVE_SERVICENODE_INITIAL:         return "Node just started, not yet activated";
        case ACTIVE_SERVICENODE_SYNC_IN_PROCESS: return "Sync in progress. Must wait until sync is complete to start Servicenode";
        case ACTIVE_SERVICENODE_INPUT_TOO_NEW:   return strprintf("Servicenode input must have at least %d confirmations", nServicenodeMinimumConfirmations);
        case ACTIVE_SERVICENODE_NOT_CAPABLE:     return "Not capable servicenode: " + strNotCapableReason;
        case ACTIVE_SERVICENODE_STARTED:         return "Servicenode successfully started";
        default:                                return "Unknown";
    }
}

std::string CActiveServicenode::GetTypeString() const
{
    std::string strType;
    switch(eType) {
    case SERVICENODE_UNKNOWN:
        strType = "UNKNOWN";
        break;
    case SERVICENODE_REMOTE:
        strType = "REMOTE";
        break;
    case SERVICENODE_LOCAL:
        strType = "LOCAL";
        break;
    default:
        strType = "UNKNOWN";
        break;
    }
    return strType;
}

bool CActiveServicenode::SendServicenodePing()
{
    if(!fPingerEnabled)
    {
        printf("CActiveServicenode::SendServicenodePing -- %s: servicenode ping service is disabled, skipping...\n", GetStateString().c_str());
        return false;
    }

    if(!mnodeman.Has(vin))
    {
        strNotCapableReason = "Servicenode not in servicenode list";
        nState = ACTIVE_SERVICENODE_NOT_CAPABLE;
        printf("CActiveServicenode::SendServicenodePing -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
        return false;
    }

    CServicenodePing mnp(vin);
    if(!mnp.Sign(keyServicenode, pubKeyServicenode))
    {
        printf("CActiveServicenode::SendServicenodePing -- ERROR: Couldn't sign Servicenode Ping\n");
        return false;
    }

    // Update lastPing for our servicenode in Servicenode list
    if(mnodeman.IsServicenodePingedWithin(vin, SERVICENODE_MIN_MNP_SECONDS, mnp.sigTime))
    {
        printf("CActiveServicenode::SendServicenodePing -- Too early to send Servicenode Ping\n");
        return false;
    }

    mnodeman.SetServicenodeLastPing(vin, mnp);

    printf("CActiveServicenode::SendServicenodePing -- Relaying ping, collateral=%s\n", vin.ToString().c_str());
    mnp.Relay();

    return true;
}

void CActiveServicenode::ManageStateInitial()
{
    printf("CActiveServicenode::ManageStateInitial -- status = %s, type = %s, pinger enabled = %d\n", GetStatus().c_str(), GetTypeString().c_str(), fPingerEnabled);

    // Check that our local network configuration is correct
    if (fNoListen)
    {
        // listen option is probably overwritten by smth else, no good
        nState = ACTIVE_SERVICENODE_NOT_CAPABLE;
        strNotCapableReason = "Servicenode must accept connections from outside. Make sure listen configuration option is not overwritten by some another parameter.";
        printf("CActiveServicenode::ManageStateInitial -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
        return;
    }

    bool fFoundLocal = false;
    {
        LOCK(cs_vNodes);

        // First try to find whatever local address is specified by externalip option
        fFoundLocal = GetLocal(service) && CServicenode::IsValidNetAddr(service);
        if(!fFoundLocal)
        {
            // nothing and no live connections, can't do anything for now
            if (vNodes.empty())
            {
                nState = ACTIVE_SERVICENODE_NOT_CAPABLE;
                strNotCapableReason = "Can't detect valid external address. Will retry when there are some connections available.";
                printf("CActiveServicenode::ManageStateInitial -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
                return;
            }
            // We have some peers, let's try to find our local address from one of them
            for (CNode * pnode : vNodes)
            {
                if (pnode->fSuccessfullyConnected && pnode->addr.IsIPv4())
                {
                    fFoundLocal = GetLocal(service, &pnode->addr) && CServicenode::IsValidNetAddr(service);
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
        nState = ACTIVE_SERVICENODE_NOT_CAPABLE;
        strNotCapableReason = "Can't detect valid external address. Please consider using the externalip configuration option if problem persists. Make sure to use IPv4 address only.";
        printf("CActiveServicenode::ManageStateInitial -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
        return;
    }

    int mainnetDefaultPort = GetDefaultPort(false);
    if (!fTestNet)
    {
        if(service.GetPort() != mainnetDefaultPort)
        {
            nState = ACTIVE_SERVICENODE_NOT_CAPABLE;
            strNotCapableReason = strprintf("Invalid port: %u - only %d is supported on mainnet.", service.GetPort(), mainnetDefaultPort);
            printf("CActiveServicenode::ManageStateInitial -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
            return;
        }
    }
    else if(service.GetPort() == mainnetDefaultPort)
    {
        nState = ACTIVE_SERVICENODE_NOT_CAPABLE;
        strNotCapableReason = strprintf("Invalid port: %u - %d is only supported on mainnet.", service.GetPort(), mainnetDefaultPort);
        printf("CActiveServicenode::ManageStateInitial -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
        return;
    }

    printf("CActiveServicenode::ManageStateInitial -- Checking inbound connection to '%s'\n", service.ToString().c_str());

    if (!ConnectNode((CAddress)service, NULL, true))
    {
        nState = ACTIVE_SERVICENODE_NOT_CAPABLE;
        strNotCapableReason = "Could not connect to " + service.ToString();
        printf("CActiveServicenode::ManageStateInitial -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
        return;
    }

    // Default to REMOTE
    eType = SERVICENODE_REMOTE;

    // Check if wallet funds are available
    if(!pwalletMain)
    {
        printf("CActiveServicenode::ManageStateInitial -- %s: Wallet not available\n", GetStateString().c_str());
        return;
    }

    if(pwalletMain->IsLocked())
    {
        printf("CActiveServicenode::ManageStateInitial -- %s: Wallet is locked\n", GetStateString().c_str());
        return;
    }

    if (pwalletMain->GetBalance() < SERVICENODE_AMOUNT*COIN)
    {
        printf("CActiveServicenode::ManageStateInitial -- %s: Wallet balance is < %d BLOCKS\n", GetStateString().c_str(), SERVICENODE_AMOUNT);
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    // If collateral is found switch to LOCAL mode
    if(pwalletMain->GetServicenodeVinAndKeys(vin, pubKeyCollateral, keyCollateral))
    {
        eType = SERVICENODE_LOCAL;
    }

    printf("CActiveServicenode::ManageStateInitial -- End status = %s, type = %s, pinger enabled = %d\n", GetStatus().c_str(), GetTypeString().c_str(), fPingerEnabled);
}

void CActiveServicenode::ManageStateRemote()
{
    printf("CActiveServicenode::ManageStateRemote -- Start status = %s, type = %s, pinger enabled = %d, pubKeyServicenode.GetID() = %s\n",
             GetStatus().c_str(), GetTypeString().c_str(), fPingerEnabled, pubKeyServicenode.GetID().ToString().c_str());

    mnodeman.CheckServicenode(pubKeyServicenode);
    servicenode_info_t infoMn = mnodeman.GetServicenodeInfo(pubKeyServicenode);
    if (infoMn.fInfoValid)
    {
        if (infoMn.nProtocolVersion != PROTOCOL_VERSION)
        {
            nState = ACTIVE_SERVICENODE_NOT_CAPABLE;
            strNotCapableReason = "Invalid protocol version";
            printf("CActiveServicenode::ManageStateRemote -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
            return;
        }
        if(service != infoMn.addr)
        {
            nState = ACTIVE_SERVICENODE_NOT_CAPABLE;
            strNotCapableReason = "Broadcasted IP doesn't match our external address. Make sure you issued a new broadcast if IP of this servicenode changed recently.";
            printf("CActiveServicenode::ManageStateRemote -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
            return;
        }
        if  (!CServicenode::IsValidStateForAutoStart(infoMn.nActiveState))
        {
           nState = ACTIVE_SERVICENODE_NOT_CAPABLE;
            strNotCapableReason = strprintf("Servicenode in %s state", CServicenode::StateToString(infoMn.nActiveState));
            printf("CActiveServicenode::ManageStateRemote -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
            return;
        }
        if (nState != ACTIVE_SERVICENODE_STARTED)
        {
            printf("CActiveServicenode::ManageStateRemote -- STARTED!\n");
            vin = infoMn.vin;
            service = infoMn.addr;
            fPingerEnabled = true;
            nState = ACTIVE_SERVICENODE_STARTED;
        }
    }
    else
    {
        nState = ACTIVE_SERVICENODE_NOT_CAPABLE;
        strNotCapableReason = "Servicenode not in servicenode list";
        printf("CActiveServicenode::ManageStateRemote -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
    }
}

void CActiveServicenode::ManageStateLocal()
{
    printf("CActiveServicenode::ManageStateLocal -- status = %s, type = %s, pinger enabled = %d\n", GetStatus().c_str(), GetTypeString().c_str(), fPingerEnabled);
    if (nState == ACTIVE_SERVICENODE_STARTED)
    {
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    if (pwalletMain->GetServicenodeVinAndKeys(vin, pubKeyCollateral, keyCollateral))
    {
        int nInputAge = GetInputDepthInMainChain(vin);
        if (nInputAge < nServicenodeMinimumConfirmations)
        {
            nState = ACTIVE_SERVICENODE_INPUT_TOO_NEW;
            strNotCapableReason = strprintf(_("%s - %d confirmations"), GetStatus(), nInputAge);
            printf("CActiveServicenode::ManageStateLocal -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
            return;
        }

        {
            LOCK(pwalletMain->cs_wallet);
            pwalletMain->LockCoin(vin.prevout);
        }

        CServicenodeBroadcast mnb;
        std::string strError;
        if (!CServicenodeBroadcast::Create(vin, service, keyCollateral, pubKeyCollateral, keyServicenode, pubKeyServicenode, strError, mnb))
        {
            nState = ACTIVE_SERVICENODE_NOT_CAPABLE;
            strNotCapableReason = "Error creating mastenode broadcast: " + strError;
            printf("CActiveServicenode::ManageStateLocal -- %s: %s\n", GetStateString().c_str(), strNotCapableReason.c_str());
            return;
        }

        fPingerEnabled = true;
        nState = ACTIVE_SERVICENODE_STARTED;

        // update to servicenode list
        printf("CActiveServicenode::ManageStateLocal -- Update Servicenode List\n");
        mnodeman.UpdateServicenodeList(mnb);
        mnodeman.NotifyServicenodeUpdates();

        //send to all peers
        printf("CActiveServicenode::ManageStateLocal -- Relay broadcast, vin=%s\n", vin.ToString().c_str());
        mnb.Relay();
    }
}
