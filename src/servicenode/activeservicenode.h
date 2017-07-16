// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ACTIVESERVICENODE_H
#define ACTIVESERVICENODE_H

#include "serialize.h"
#include "net.h"
#include "key.h"
#include "wallet.h"

class CActiveServicenode;

static const int SERVICENODE_AMOUNT                  = 1000;

static const int ACTIVE_SERVICENODE_INITIAL          = 0; // initial state
static const int ACTIVE_SERVICENODE_SYNC_IN_PROCESS  = 1;
static const int ACTIVE_SERVICENODE_INPUT_TOO_NEW    = 2;
static const int ACTIVE_SERVICENODE_NOT_CAPABLE      = 3;
static const int ACTIVE_SERVICENODE_STARTED          = 4;

extern CActiveServicenode activeServicenode;

// Responsible for activating the Servicenode and pinging the network
class CActiveServicenode
{
public:
    enum servicenode_type_enum_t {
        SERVICENODE_UNKNOWN = 0,
        SERVICENODE_REMOTE  = 1,
        SERVICENODE_LOCAL   = 2
    };

private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    servicenode_type_enum_t eType;

    bool fPingerEnabled;

    /// Ping Servicenode
    bool SendServicenodePing();

public:
    // Keys for the active Servicenode
    CPubKey pubKeyServicenode;
    CKey keyServicenode;

    // Initialized while registering Servicenode
    CTxIn vin;
    CService service;

    int nState; // should be one of ACTIVE_SERVICENODE_XXXX
    std::string strNotCapableReason;

    CActiveServicenode()
        : eType(SERVICENODE_UNKNOWN),
          fPingerEnabled(false),
          pubKeyServicenode(),
          keyServicenode(),
          vin(),
          service(),
          nState(ACTIVE_SERVICENODE_INITIAL)
    {}

    /// Manage state of active Servicenode
    void ManageState();

    std::string GetStateString() const;
    std::string GetStatus() const;
    std::string GetTypeString() const;

private:
    void ManageStateInitial();
    void ManageStateRemote();
    void ManageStateLocal();
};

#endif
