//*****************************************************************************
//*****************************************************************************

#ifndef CHATDB_H
#define CHATDB_H

#include "message.h"
#include "db.h"

#include <string>
#include <vector>
#include <map>

typedef std::map<uint256, Message> UndeliveredMap;

//*****************************************************************************
//*****************************************************************************
class ChatDb : public CDB
{
protected:
    ChatDb();

public:
    static ChatDb & instance();

public:
    bool load(const std::string & address, std::vector<Message> & messages);
    bool save(const std::string & address, const std::vector<Message> & messages);
    bool erase(const std::string & address);

    bool loadUndelivered(UndeliveredMap & messages);
    bool saveUndelivered(const UndeliveredMap & messages);

    bool loadAddresses(std::vector<std::string> & addresses);

private:
    CCriticalSection m_cs;

    static std::string m_undelivered;
};

//*****************************************************************************
//*****************************************************************************
class StoredPubKeysDb : public CDB
{
protected:
    StoredPubKeysDb();

public:
    static StoredPubKeysDb & instance();

public:
    bool load(const std::string & address, CPubKey & key);
    bool store(const std::string & address, const CPubKey & key);

private:
    CCriticalSection m_cs;
};

#endif // CHATDB_H
