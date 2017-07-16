// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "servicenode/activeservicenode.h"
// #include "darksend.h"
#include "init.h"
#include "main.h"
#include "servicenode/servicenode-payments.h"
#include "servicenode/servicenode-sync.h"
#include "servicenode/servicenodeconfig.h"
#include "servicenode/servicenodeman.h"
#include "rpc/bitcoinrpc.h"
// #include "util.h"
// #include "utilmoneystr.h"

#include <fstream>
#include <iomanip>

using namespace json_spirit;

void EnsureWalletIsUnlocked();

//Value privatesend(const Array & params, bool fHelp)
//{
//    if (fHelp || params.size() != 1)
//        throw std::runtime_error(
//            "privatesend \"command\"\n"
//            "\nArguments:\n"
//            "1. \"command\"        (string or set of strings, required) The command to execute\n"
//            "\nAvailable commands:\n"
//            "  start       - Start mixing\n"
//            "  stop        - Stop mixing\n"
//            "  reset       - Reset mixing\n"
//            );

//    if(params[0].get_str() == "start") {
//        {
//            LOCK(pwalletMain->cs_wallet);
//            EnsureWalletIsUnlocked();
//        }

//        if(fServiceNode)
//            return "Mixing is not supported from servicenodes";

//        fEnablePrivateSend = true;
//        bool result = darkSendPool.DoAutomaticDenominating();
//        return "Mixing " + (result ? "started successfully" : ("start failed: " + darkSendPool.GetStatus() + ", will retry"));
//    }

//    if(params[0].get_str() == "stop") {
//        fEnablePrivateSend = false;
//        return "Mixing was stopped";
//    }

//    if(params[0].get_str() == "reset") {
//        darkSendPool.ResetPool();
//        return "Mixing was reset";
//    }

//    return "Unknown command, please see \"help privatesend\"";
//}

//Value getpoolinfo(const Array & params, bool fHelp)
//{
//    if (fHelp || params.size() != 0)
//        throw std::runtime_error(
//            "getpoolinfo\n"
//            "Returns an object containing mixing pool related information.\n");

//    UniValue obj(UniValue::VOBJ);
//    obj.push_back(Pair("state",             darkSendPool.GetStateString()));
//    obj.push_back(Pair("mixing_mode",       fPrivateSendMultiSession ? "multi-session" : "normal"));
//    obj.push_back(Pair("queue",             darkSendPool.GetQueueSize()));
//    obj.push_back(Pair("entries",           darkSendPool.GetEntriesCount()));
//    obj.push_back(Pair("status",            darkSendPool.GetStatus()));

//    if (darkSendPool.pSubmittedToServicenode) {
//        obj.push_back(Pair("outpoint",      darkSendPool.pSubmittedToServicenode->vin.prevout.ToStringShort()));
//        obj.push_back(Pair("addr",          darkSendPool.pSubmittedToServicenode->addr.ToString()));
//    }

//    if (pwalletMain) {
//        obj.push_back(Pair("keys_left",     pwalletMain->nKeysLeftSinceAutoBackup));
//        obj.push_back(Pair("warnings",      pwalletMain->nKeysLeftSinceAutoBackup < PRIVATESEND_KEYS_THRESHOLD_WARNING
//                                                ? "WARNING: keypool is almost depleted!" : ""));
//    }

//    return obj;
//}


Value servicenode(const Array & params, bool fHelp)
{
    std::string strCommand;
    if (params.size() >= 1) {
        strCommand = params[0].get_str();
    }

    if (strCommand == "start-many")
        throw JSONRPCError(RPC_INVALID_PARAMETER, "DEPRECATED, please use start-all instead");

    if (fHelp  ||
        (strCommand != "start" && strCommand != "start-alias" && strCommand != "start-all" && strCommand != "start-missing" &&
         strCommand != "start-disabled" && strCommand != "list" && strCommand != "list-conf" && strCommand != "count" &&
         strCommand != "debug" && strCommand != "current" && strCommand != "winner" && strCommand != "winners" && strCommand != "genkey" &&
         strCommand != "connect" && strCommand != "outputs" && strCommand != "status"))
            throw std::runtime_error(
                "servicenode \"command\"...\n"
                "Set of commands to execute servicenode related actions\n"
                "\nArguments:\n"
                "1. \"command\"        (string or set of strings, required) The command to execute\n"
                "\nAvailable commands:\n"
                "  count        - Print number of all known servicenodes (optional: 'ps', 'enabled', 'all', 'qualify')\n"
                "  current      - Print info on current servicenode winner to be paid the next block (calculated locally)\n"
                "  debug        - Print servicenode status\n"
                "  genkey       - Generate new servicenodeprivkey\n"
                "  outputs      - Print servicenode compatible outputs\n"
                "  start        - Start local Hot servicenode configured in dash.conf\n"
                "  start-alias  - Start single remote servicenode by assigned alias configured in servicenode.conf\n"
                "  start-<mode> - Start remote servicenodes configured in servicenode.conf (<mode>: 'all', 'missing', 'disabled')\n"
                "  status       - Print servicenode status information\n"
                "  list         - Print list of all known servicenodes (see servicenodelist for more info)\n"
                "  list-conf    - Print servicenode.conf in JSON format\n"
                "  winner       - Print info on next servicenode winner to vote for\n"
                "  winners      - Print list of servicenode winners\n"
                );

    if (strCommand == "list")
    {
        Array newParams;
        // forward params but skip "list"
        for (unsigned int i = 1; i < params.size(); i++)
        {
            newParams.push_back(params[i]);
        }
        return servicenodelist(newParams, fHelp);
    }

    if(strCommand == "connect")
    {
        if (params.size() < 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Servicenode address required");

        std::string strAddress = params[1].get_str();

        CService addr = CService(strAddress);

        CNode *pnode = ConnectNode((CAddress)addr, NULL);
        if(!pnode)
            throw JSONRPCError(RPC_INTERNAL_ERROR, strprintf("Couldn't connect to servicenode %s", strAddress));

        return "successfully connected";
    }

    if (strCommand == "count")
    {
        if (params.size() > 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Too many parameters");

        if (params.size() == 1)
            return mnodeman.size();

        std::string strMode = params[1].get_str();

//        if (strMode == "ps")
//            return mnodeman.CountEnabled(MIN_PRIVATESEND_PEER_PROTO_VERSION);

        if (strMode == "enabled")
            return mnodeman.CountEnabled();

        int nCount;
        mnodeman.GetNextServicenodeInQueueForPayment(true, nCount);

        if (strMode == "qualify")
            return nCount;

//        if (strMode == "all")
//            return strprintf("Total: %d (PS Compatible: %d / Enabled: %d / Qualify: %d)",
//                mnodeman.size(), mnodeman.CountEnabled(MIN_PRIVATESEND_PEER_PROTO_VERSION),
//                mnodeman.CountEnabled(), nCount);
        if (strMode == "all")
            return strprintf("Total: %d (Enabled: %d / Qualify: %d)",
                mnodeman.size(), mnodeman.CountEnabled(), nCount);
    }

    if (strCommand == "current" || strCommand == "winner")
    {
        int nCount = 0;
        int nHeight = 0;
        CServicenode* winner = NULL;
        {
            LOCK(cs_main);
            nHeight = nBestHeight + (strCommand == "current" ? 1 : 10);
        }
        mnodeman.UpdateLastPaid();
        winner = mnodeman.GetNextServicenodeInQueueForPayment(nHeight, true, nCount);
        if(!winner) return "unknown";

        Object obj;

        obj.push_back(Pair("height",        nHeight));
        obj.push_back(Pair("IP:port",       winner->addr.ToString()));
        obj.push_back(Pair("protocol",      (int64_t)winner->nProtocolVersion));
        obj.push_back(Pair("vin",           winner->vin.prevout.ToString()));
        obj.push_back(Pair("payee",         CBitcoinAddress(winner->pubKeyCollateralAddress.GetID()).ToString()));
        obj.push_back(Pair("lastseen",      (winner->lastPing == CServicenodePing()) ? winner->sigTime :
                                                    winner->lastPing.sigTime));
        obj.push_back(Pair("activeseconds", (winner->lastPing == CServicenodePing()) ? 0 :
                                                    (winner->lastPing.sigTime - winner->sigTime)));
        return obj;
    }

    if (strCommand == "debug")
    {
        if(activeServicenode.nState != ACTIVE_SERVICENODE_INITIAL || !servicenodeSync.IsBlockchainSynced())
            return activeServicenode.GetStatus();

        CTxIn vin;
        CPubKey pubkey;
        CKey key;

        if(!pwalletMain || !pwalletMain->GetServicenodeVinAndKeys(vin, pubkey, key))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Missing servicenode input, please look at the documentation for instructions on servicenode creation");

        return activeServicenode.GetStatus();
    }

    if (strCommand == "start")
    {
        if(!fServiceNode)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "You must set servicenode=1 in the configuration");

        {
            LOCK(pwalletMain->cs_wallet);
            EnsureWalletIsUnlocked();
        }

        if(activeServicenode.nState != ACTIVE_SERVICENODE_STARTED){
            activeServicenode.nState = ACTIVE_SERVICENODE_INITIAL; // TODO: consider better way
            activeServicenode.ManageState();
        }

        return activeServicenode.GetStatus();
    }

    if (strCommand == "start-alias")
    {
        if (params.size() < 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Please specify an alias");

        {
            LOCK(pwalletMain->cs_wallet);
            EnsureWalletIsUnlocked();
        }

        std::string strAlias = params[1].get_str();

        bool fFound = false;

        Object statusObj;
        statusObj.push_back(Pair("alias", strAlias));

        for (CServicenodeConfig::CServicenodeEntry & mne : servicenodeConfig.getEntries())
        {
            if(mne.getAlias() == strAlias)
            {
                fFound = true;
                std::string strError;
                CServicenodeBroadcast mnb;

                bool fResult = CServicenodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb);

                statusObj.push_back(Pair("result", fResult ? "successful" : "failed"));
                if(fResult) {
                    mnodeman.UpdateServicenodeList(mnb);
                    mnb.Relay();
                } else {
                    statusObj.push_back(Pair("errorMessage", strError));
                }
                mnodeman.NotifyServicenodeUpdates();
                break;
            }
        }

        if(!fFound) {
            statusObj.push_back(Pair("result", "failed"));
            statusObj.push_back(Pair("errorMessage", "Could not find alias in config. Verify with list-conf."));
        }

        return statusObj;
    }

    if (strCommand == "start-all" || strCommand == "start-missing" || strCommand == "start-disabled")
    {
        {
            LOCK(pwalletMain->cs_wallet);
            EnsureWalletIsUnlocked();
        }

        if((strCommand == "start-missing" || strCommand == "start-disabled") && !servicenodeSync.IsServicenodeListSynced()) {
            throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "You can't use this command until servicenode list is synced");
        }

        int nSuccessful = 0;
        int nFailed = 0;

        Object resultsObj;

        for (CServicenodeConfig::CServicenodeEntry & mne : servicenodeConfig.getEntries())
        {
            std::string strError;

            CTxIn vin = CTxIn(uint256(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
            CServicenode * pmn = mnodeman.Find(vin);
            CServicenodeBroadcast mnb;

            if(strCommand == "start-missing" && pmn)
            {
                continue;
            }

            if(strCommand == "start-disabled" && pmn && pmn->IsEnabled())
            {
                continue;
            }

            bool fResult = CServicenodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb);

            Object statusObj;
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("result", fResult ? "successful" : "failed"));

            if (fResult)
            {
                nSuccessful++;
                mnodeman.UpdateServicenodeList(mnb);
                mnb.Relay();
            }
            else
            {
                nFailed++;
                statusObj.push_back(Pair("errorMessage", strError));
            }

            resultsObj.push_back(Pair("status", statusObj));
        }
        mnodeman.NotifyServicenodeUpdates();

        Object returnObj;
        returnObj.push_back(Pair("overall", strprintf("Successfully started %d servicenodes, failed to start %d, total %d", nSuccessful, nFailed, nSuccessful + nFailed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "genkey")
    {
        CKey key;
        key.MakeNewKey(true);

        bool compressed = false;
        CSecret secret = key.GetSecret(compressed);

        return CBitcoinSecret(secret, compressed).ToString();
    }

    if (strCommand == "list-conf")
    {
        Object resultObj;

        for (const CServicenodeConfig::CServicenodeEntry & mne : servicenodeConfig.getEntries())
        {
            CTxIn vin = CTxIn(uint256(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
            CServicenode *pmn = mnodeman.Find(vin);

            std::string strStatus = pmn ? pmn->GetStatus() : "MISSING";

            Object mnObj;
            mnObj.push_back(Pair("alias", mne.getAlias()));
            mnObj.push_back(Pair("address", mne.getIp()));
            mnObj.push_back(Pair("privateKey", mne.getPrivKey()));
            mnObj.push_back(Pair("txHash", mne.getTxHash()));
            mnObj.push_back(Pair("outputIndex", mne.getOutputIndex()));
            mnObj.push_back(Pair("status", strStatus));
            resultObj.push_back(Pair("servicenode", mnObj));
        }

        return resultObj;
    }

    if (strCommand == "outputs")
    {
        // Find possible candidates
        std::vector<COutput> vPossibleCoins;
        pwalletMain->AvailableCoins(vPossibleCoins, true, NULL, false, ONLY_SERVICENODE_AMOUNT);

        Object obj;
        for (const COutput & out : vPossibleCoins)
        {
            obj.push_back(Pair(out.tx->GetHash().ToString(), strprintf("%d", out.i)));
        }

        return obj;
    }

    if (strCommand == "status")
    {
        if (!fServiceNode)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "This is not a servicenode");

        Object mnObj;

        mnObj.push_back(Pair("vin", activeServicenode.vin.ToString()));
        mnObj.push_back(Pair("service", activeServicenode.service.ToString()));

        CServicenode mn;
        if(mnodeman.Get(activeServicenode.vin, mn))
        {
            mnObj.push_back(Pair("payee", CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString()));
        }

        mnObj.push_back(Pair("status", activeServicenode.GetStatus()));
        return mnObj;
    }

    if (strCommand == "winners")
    {
        int nHeight;
        {
            LOCK(cs_main);
            CBlockIndex * pindex = FindBlockByHeight(nBestHeight); // chainActive.Tip();
            if(!pindex)
            {
                return Object();
            }

            nHeight = pindex->nHeight;
        }

        int nLast = 10;
        std::string strFilter = "";

        if (params.size() >= 2)
        {
            nLast = atoi(params[1].get_str());
        }

        if (params.size() == 3) {
            strFilter = params[2].get_str();
        }

        if (params.size() > 3)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Correct usage is 'servicenode winners ( \"count\" \"filter\" )'");

        Object obj;

        for(int i = nHeight - nLast; i < nHeight + 20; i++)
        {
            std::string strPayment = GetRequiredPaymentsString(i);
            if (strFilter !="" && strPayment.find(strFilter) == std::string::npos)
            {
                continue;
            }

            obj.push_back(Pair(strprintf("%d", i), strPayment));
        }

        return obj;
    }

    return Value();
}

Value servicenodelist(const Array & params, bool fHelp)
{
    std::string strMode = "status";
    std::string strFilter = "";

    if (params.size() >= 1) strMode = params[0].get_str();
    if (params.size() == 2) strFilter = params[1].get_str();

    if (fHelp || (
                strMode != "activeseconds" && strMode != "addr" && strMode != "full" &&
                strMode != "lastseen" && strMode != "lastpaidtime" && strMode != "lastpaidblock" &&
                strMode != "protocol" && strMode != "payee" && strMode != "rank" && strMode != "status"))
    {
        throw std::runtime_error(
                "servicenodelist ( \"mode\" \"filter\" )\n"
                "Get a list of servicenodes in different modes\n"
                "\nArguments:\n"
                "1. \"mode\"      (string, optional/required to use filter, defaults = status) The mode to run list in\n"
                "2. \"filter\"    (string, optional) Filter results. Partial match by outpoint by default in all modes,\n"
                "                                    additional matches in some modes are also available\n"
                "\nAvailable modes:\n"
                "  activeseconds  - Print number of seconds servicenode recognized by the network as enabled\n"
                "                   (since latest issued \"servicenode start/start-many/start-alias\")\n"
                "  addr           - Print ip address associated with a servicenode (can be additionally filtered, partial match)\n"
                "  full           - Print info in format 'status protocol payee lastseen activeseconds lastpaidtime lastpaidblock IP'\n"
                "                   (can be additionally filtered, partial match)\n"
                "  lastpaidblock  - Print the last block height a node was paid on the network\n"
                "  lastpaidtime   - Print the last time a node was paid on the network\n"
                "  lastseen       - Print timestamp of when a servicenode was last seen on the network\n"
                "  payee          - Print Dash address associated with a servicenode (can be additionally filtered,\n"
                "                   partial match)\n"
                "  protocol       - Print protocol of a servicenode (can be additionally filtered, exact match))\n"
                "  rank           - Print rank of a servicenode based on current block\n"
                "  status         - Print servicenode status: PRE_ENABLED / ENABLED / EXPIRED / WATCHDOG_EXPIRED / NEW_START_REQUIRED /\n"
                "                   UPDATE_REQUIRED / POSE_BAN / OUTPOINT_SPENT (can be additionally filtered, partial match)\n"
                );
    }

    if (strMode == "full" || strMode == "lastpaidtime" || strMode == "lastpaidblock")
    {
        mnodeman.UpdateLastPaid();
    }

    Object obj;
    if (strMode == "rank")
    {
        std::vector<std::pair<int, CServicenode> > vServicenodeRanks = mnodeman.GetServicenodeRanks();
        for (PAIRTYPE(int, CServicenode) & s : vServicenodeRanks)
        {
            std::string strOutpoint = s.second.vin.prevout.ToString();
            if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos)
            {
                continue;
            }
            obj.push_back(Pair(strOutpoint, s.first));
        }
    }
    else
    {
        std::vector<CServicenode> vServicenodes = mnodeman.GetFullServicenodeVector();
        for (CServicenode & mn : vServicenodes)
        {
            std::string strOutpoint = mn.vin.prevout.ToString();
            if (strMode == "activeseconds")
            {
                if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, (int64_t)(mn.lastPing.sigTime - mn.sigTime)));
            }
            else if (strMode == "addr")
            {
                std::string strAddress = mn.addr.ToString();
                if (strFilter !="" && strAddress.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, strAddress));
            }
            else if (strMode == "full")
            {
                std::ostringstream streamFull;
                streamFull << std::setw(18) <<
                               mn.GetStatus() << " " <<
                               mn.nProtocolVersion << " " <<
                               CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString() << " " <<
                               (int64_t)mn.lastPing.sigTime << " " << std::setw(8) <<
                               (int64_t)(mn.lastPing.sigTime - mn.sigTime) << " " << std::setw(10) <<
                               mn.GetLastPaidTime() << " "  << std::setw(6) <<
                               mn.GetLastPaidBlock() << " " <<
                               mn.addr.ToString();
                std::string strFull = streamFull.str();
                if (strFilter !="" && strFull.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, strFull));
            }
            else if (strMode == "lastpaidblock")
            {
                if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, mn.GetLastPaidBlock()));
            }
            else if (strMode == "lastpaidtime")
            {
                if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, mn.GetLastPaidTime()));
            }
            else if (strMode == "lastseen")
            {
                if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, (int64_t)mn.lastPing.sigTime));
            }
            else if (strMode == "payee")
            {
                CBitcoinAddress address(mn.pubKeyCollateralAddress.GetID());
                std::string strPayee = address.ToString();
                if (strFilter !="" && strPayee.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, strPayee));
            }
            else if (strMode == "protocol")
            {
               if (strFilter !="" && strFilter != strprintf("%d", mn.nProtocolVersion) &&
                    strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, (int64_t)mn.nProtocolVersion));
            }
            else if (strMode == "status")
            {
                std::string strStatus = mn.GetStatus();
                if (strFilter !="" && strStatus.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.push_back(Pair(strOutpoint, strStatus));
            }
        }
    }
    return obj;
}

bool DecodeHexVecMnb(std::vector<CServicenodeBroadcast>& vecMnb, std::string strHexMnb)
{
    if (!IsHex(strHexMnb))
        return false;

    std::vector<unsigned char> mnbData(ParseHex(strHexMnb));
    CDataStream ssData(mnbData, SER_NETWORK, PROTOCOL_VERSION);
    try
    {
        ssData >> vecMnb;
    }
    catch (const std::exception&)
    {
        return false;
    }

    return true;
}

Value servicenodebroadcast(const Array & params, bool fHelp)
{
    std::string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "create-alias" && strCommand != "create-all" && strCommand != "decode" && strCommand != "relay"))
        throw std::runtime_error(
                "servicenodebroadcast \"command\"...\n"
                "Set of commands to create and relay servicenode broadcast messages\n"
                "\nArguments:\n"
                "1. \"command\"        (string or set of strings, required) The command to execute\n"
                "\nAvailable commands:\n"
                "  create-alias  - Create single remote servicenode broadcast message by assigned alias configured in servicenode.conf\n"
                "  create-all    - Create remote servicenode broadcast messages for all servicenodes configured in servicenode.conf\n"
                "  decode        - Decode servicenode broadcast message\n"
                "  relay         - Relay servicenode broadcast message to the network\n"
                );

    if (strCommand == "create-alias")
    {
        // wait for reindex and/or import to finish
//        if (fImporting || fReindex)
//            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        if (params.size() < 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Please specify an alias");

        {
            LOCK(pwalletMain->cs_wallet);
            EnsureWalletIsUnlocked();
        }

        bool fFound = false;
        std::string strAlias = params[1].get_str();

        Object statusObj;
        std::vector<CServicenodeBroadcast> vecMnb;

        statusObj.push_back(Pair("alias", strAlias));

        for (CServicenodeConfig::CServicenodeEntry & mne : servicenodeConfig.getEntries())
        {
           if(mne.getAlias() == strAlias) {
                fFound = true;
                std::string strError;
                CServicenodeBroadcast mnb;

                bool fResult = CServicenodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb, true);

                statusObj.push_back(Pair("result", fResult ? "successful" : "failed"));
                if(fResult) {
                    vecMnb.push_back(mnb);
                    CDataStream ssVecMnb(SER_NETWORK, PROTOCOL_VERSION);
                    ssVecMnb << vecMnb;
                    statusObj.push_back(Pair("hex", HexStr(ssVecMnb.begin(), ssVecMnb.end())));
                }
                else
                {
                    statusObj.push_back(Pair("errorMessage", strError));
                }
                break;
            }
        }

        if(!fFound) {
            statusObj.push_back(Pair("result", "not found"));
            statusObj.push_back(Pair("errorMessage", "Could not find alias in config. Verify with list-conf."));
        }

        return statusObj;
    }

    if (strCommand == "create-all")
    {
        // wait for reindex and/or import to finish
//        if (fImporting || fReindex)
//            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        {
            LOCK(pwalletMain->cs_wallet);
            EnsureWalletIsUnlocked();
        }

        std::vector<CServicenodeConfig::CServicenodeEntry> mnEntries;
        mnEntries = servicenodeConfig.getEntries();

        int nSuccessful = 0;
        int nFailed = 0;

        Object resultsObj;
        std::vector<CServicenodeBroadcast> vecMnb;

        for (CServicenodeConfig::CServicenodeEntry & mne : servicenodeConfig.getEntries())
        {
            std::string strError;
            CServicenodeBroadcast mnb;

            bool fResult = CServicenodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb, true);

            Object statusObj;
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("result", fResult ? "successful" : "failed"));

            if(fResult)
            {
                nSuccessful++;
                vecMnb.push_back(mnb);
            }
            else
            {
                nFailed++;
                statusObj.push_back(Pair("errorMessage", strError));
            }

            resultsObj.push_back(Pair("status", statusObj));
        }

        CDataStream ssVecMnb(SER_NETWORK, PROTOCOL_VERSION);
        ssVecMnb << vecMnb;
        Object returnObj;
        returnObj.push_back(Pair("overall", strprintf("Successfully created broadcast messages for %d servicenodes, failed to create %d, total %d", nSuccessful, nFailed, nSuccessful + nFailed)));
        returnObj.push_back(Pair("detail", resultsObj));
        returnObj.push_back(Pair("hex", HexStr(ssVecMnb.begin(), ssVecMnb.end())));

        return returnObj;
    }

    if (strCommand == "decode")
    {
        if (params.size() != 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Correct usage is 'servicenodebroadcast decode \"hexstring\"'");

        std::vector<CServicenodeBroadcast> vecMnb;

        if (!DecodeHexVecMnb(vecMnb, params[1].get_str()))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Servicenode broadcast message decode failed");

        int nSuccessful = 0;
        int nFailed = 0;
        int nDos = 0;
        Object returnObj;

        for (CServicenodeBroadcast & mnb : vecMnb)
        {
            Object resultObj;

            if(mnb.CheckSignature(nDos))
            {
                nSuccessful++;
                resultObj.push_back(Pair("vin", mnb.vin.ToString()));
                resultObj.push_back(Pair("addr", mnb.addr.ToString()));
                resultObj.push_back(Pair("pubKeyCollateralAddress", CBitcoinAddress(mnb.pubKeyCollateralAddress.GetID()).ToString()));
                resultObj.push_back(Pair("pubKeyServicenode", CBitcoinAddress(mnb.pubKeyServicenode.GetID()).ToString()));
                resultObj.push_back(Pair("vchSig", EncodeBase64(&mnb.vchSig[0], mnb.vchSig.size())));
                resultObj.push_back(Pair("sigTime", mnb.sigTime));
                resultObj.push_back(Pair("protocolVersion", mnb.nProtocolVersion));
                resultObj.push_back(Pair("nLastDsq", mnb.nLastDsq));

                Object lastPingObj;
                lastPingObj.push_back(Pair("vin", mnb.lastPing.vin.ToString()));
                lastPingObj.push_back(Pair("blockHash", mnb.lastPing.blockHash.ToString()));
                lastPingObj.push_back(Pair("sigTime", mnb.lastPing.sigTime));
                lastPingObj.push_back(Pair("vchSig", EncodeBase64(&mnb.lastPing.vchSig[0], mnb.lastPing.vchSig.size())));

                resultObj.push_back(Pair("lastPing", lastPingObj));
            }
            else
            {
                nFailed++;
                resultObj.push_back(Pair("errorMessage", "Servicenode broadcast signature verification failed"));
            }

            returnObj.push_back(Pair(mnb.GetHash().ToString(), resultObj));
        }

        returnObj.push_back(Pair("overall", strprintf("Successfully decoded broadcast messages for %d servicenodes, failed to decode %d, total %d", nSuccessful, nFailed, nSuccessful + nFailed)));

        return returnObj;
    }

    if (strCommand == "relay")
    {
        if (params.size() < 2 || params.size() > 3)
            throw JSONRPCError(RPC_INVALID_PARAMETER,   "servicenodebroadcast relay \"hexstring\" ( fast )\n"
                                                        "\nArguments:\n"
                                                        "1. \"hex\"      (string, required) Broadcast messages hex string\n"
                                                        "2. fast       (string, optional) If none, using safe method\n");

        std::vector<CServicenodeBroadcast> vecMnb;

        if (!DecodeHexVecMnb(vecMnb, params[1].get_str()))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Servicenode broadcast message decode failed");

        int nSuccessful = 0;
        int nFailed = 0;
        bool fSafe = params.size() == 2;
        Object returnObj;

        // verify all signatures first, bailout if any of them broken
        for (CServicenodeBroadcast & mnb : vecMnb)
        {
            Object resultObj;

            resultObj.push_back(Pair("vin", mnb.vin.ToString()));
            resultObj.push_back(Pair("addr", mnb.addr.ToString()));

            int nDos = 0;
            bool fResult;
            if (mnb.CheckSignature(nDos))
            {
                if (fSafe)
                {
                    fResult = mnodeman.CheckMnbAndUpdateServicenodeList(NULL, mnb, nDos);
                }
                else
                {
                    mnodeman.UpdateServicenodeList(mnb);
                    mnb.Relay();
                    fResult = true;
                }
                mnodeman.NotifyServicenodeUpdates();
            }
            else
            {
                fResult = false;
            }

            if(fResult)
            {
                nSuccessful++;
                resultObj.push_back(Pair(mnb.GetHash().ToString(), "successful"));
            }
            else
            {
                nFailed++;
                resultObj.push_back(Pair("errorMessage", "Servicenode broadcast signature verification failed"));
            }

            returnObj.push_back(Pair(mnb.GetHash().ToString(), resultObj));
        }

        returnObj.push_back(Pair("overall", strprintf("Successfully relayed broadcast messages for %d servicenodes, failed to relay %d, total %d", nSuccessful, nFailed, nSuccessful + nFailed)));

        return returnObj;
    }

    return Value();
}

Value mnsync(const Array & params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "mnsync [status|next|reset]\n"
            "Returns the sync status, updates to the next step or resets it entirely.\n"
        );

    std::string strMode = params[0].get_str();

    if (strMode == "status")
    {
        Object objStatus;
        objStatus.push_back(Pair("AssetID", servicenodeSync.GetAssetID()));
        objStatus.push_back(Pair("AssetName", servicenodeSync.GetAssetName()));
        objStatus.push_back(Pair("Attempt", servicenodeSync.GetAttempt()));
        objStatus.push_back(Pair("IsBlockchainSynced", servicenodeSync.IsBlockchainSynced()));
        objStatus.push_back(Pair("IsServicenodeListSynced", servicenodeSync.IsServicenodeListSynced()));
        objStatus.push_back(Pair("IsWinnersListSynced", servicenodeSync.IsWinnersListSynced()));
        objStatus.push_back(Pair("IsSynced", servicenodeSync.IsSynced()));
        objStatus.push_back(Pair("IsFailed", servicenodeSync.IsFailed()));
        return objStatus;
    }

    if(strMode == "next")
    {
        servicenodeSync.SwitchToNextAsset();
        return "sync updated to " + servicenodeSync.GetAssetName();
    }

    if(strMode == "reset")
    {
        servicenodeSync.Reset();
        return "success";
    }
    return "failure";
}
