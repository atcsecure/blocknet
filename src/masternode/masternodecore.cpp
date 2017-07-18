
#include "masternodecore.h"
#include "masternode-sync.h"
#include "masternodeman.h"
#include "masternode-payments.h"
#include "activemasternode.h"
#include "util.h"


void ThreadMasternodeService(void * /*parg*/)
{
    static bool fOneThread;
    if (fOneThread)
    {
        return;
    }
    fOneThread = true;

    // Make this thread recognisable as the PrivateSend thread
    RenameThread("blocknet-masternode-service");

    unsigned int nTick = 0;
//    unsigned int nDoAutoNextRun = nTick + PRIVATESEND_AUTO_TIMEOUT_MIN;

    while (true)
    {
        MilliSleep(1000);

        // try to sync from all available nodes, one step at a time
        masternodeSync.ProcessTick();

        if (masternodeSync.IsBlockchainSynced() && !fShutdown)
        {
            nTick++;

            // make sure to check all masternodes first
            mnodeman.Check();

            // check if we should activate or ping every few minutes,
            // slightly postpone first run to give net thread a chance to connect to some peers
            if (nTick % MASTERNODE_MIN_MNP_SECONDS == 15)
            {
                activeMasternode.ManageState();
            }

            if(nTick % 60 == 0)
            {
                mnodeman.ProcessMasternodeConnections();
                mnodeman.CheckAndRemove();
                mnpayments.CheckAndRemove();
//                instantsend.CheckAndRemove();
            }
            if (fMasterNode && (nTick % (60 * 5) == 0))
            {
                mnodeman.DoFullVerificationStep();
            }

//            if(nTick % (60 * 5) == 0)
//            {
//                governance.DoMaintenance();
//            }

//            darkSendPool.CheckTimeout();
//            darkSendPool.CheckForCompleteQueue();

//            if (nDoAutoNextRun == nTick)
//            {
//                darkSendPool.DoAutomaticDenominating();
//                nDoAutoNextRun = nTick + PRIVATESEND_AUTO_TIMEOUT_MIN + GetRandInt(PRIVATESEND_AUTO_TIMEOUT_MAX - PRIVATESEND_AUTO_TIMEOUT_MIN);
//            }
        }
    }
}
