
#include "servicenodecore.h"
#include "servicenode-sync.h"
#include "servicenodeman.h"
#include "servicenode-payments.h"
#include "activeservicenode.h"
#include "util.h"


void ThreadServicenodeService()
{
    static bool fOneThread;
    if (fOneThread)
    {
        return;
    }
    fOneThread = true;

    // Make this thread recognisable as the PrivateSend thread
    RenameThread("blocknet-servicenode-service");

    unsigned int nTick = 0;
//    unsigned int nDoAutoNextRun = nTick + PRIVATESEND_AUTO_TIMEOUT_MIN;

    while (true)
    {
        MilliSleep(1000);

        // try to sync from all available nodes, one step at a time
        servicenodeSync.ProcessTick();

        if (servicenodeSync.IsBlockchainSynced() && !fShutdown)
        {
            nTick++;

            // make sure to check all servicenodes first
            mnodeman.Check();

            // check if we should activate or ping every few minutes,
            // slightly postpone first run to give net thread a chance to connect to some peers
            if (nTick % SERVICENODE_MIN_MNP_SECONDS == 15)
            {
                activeServicenode.ManageState();
            }

            if(nTick % 60 == 0)
            {
                mnodeman.ProcessServicenodeConnections();
                mnodeman.CheckAndRemove();
                mnpayments.CheckAndRemove();
//                instantsend.CheckAndRemove();
            }
            if (fServiceNode && (nTick % (60 * 5) == 0))
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
