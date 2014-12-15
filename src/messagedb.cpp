//*****************************************************************************
//*****************************************************************************

#include "messagedb.h"

//*****************************************************************************
//*****************************************************************************
ChatDb::ChatDb()
    : CDB("chat.dat", "rw")
{
}

//*****************************************************************************
//*****************************************************************************
// static
ChatDb & ChatDb::instance()
{
    static ChatDb db;
    return db;
}

//*****************************************************************************
//*****************************************************************************
bool ChatDb::load(const std::string & address, std::vector<Message> & messages)
{
    messages.clear();

    {
        LOCK(m_cs);

        if (!Read(address, messages))
        {
            return false;
        }
    }

    // TODO
    // crypto
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool ChatDb::save(const std::string & address, const std::vector<Message> & messages)
{
    // TODO
    // crypto

    LOCK(m_cs);
    return Write(address, messages);
}

//*****************************************************************************
//*****************************************************************************
bool ChatDb::erase(const std::string & address)
{
    LOCK(m_cs);
    return Erase(address);
}

//*****************************************************************************
//*****************************************************************************
// static
std::string ChatDb::m_undelivered("undelivered");

//*****************************************************************************
//*****************************************************************************
bool ChatDb::loadUndelivered(UndeliveredMap & messages)
{
    messages.clear();
    {
        LOCK(m_cs);
        if (!Read(m_undelivered, messages))
        {
            return false;
        }
    }
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool ChatDb::saveUndelivered(const UndeliveredMap & messages)
{
    LOCK(m_cs);
    return Write(m_undelivered, messages);
}

//*****************************************************************************
//*****************************************************************************
bool ChatDb::loadAddresses(std::vector<std::string> & addresses)
{
    addresses.clear();

    Dbc * cur = GetCursor();
    if (!cur)
    {
        return false;
    }

    bool success = true;
    while (success)
    {
        CDataStream key(SER_DISK, CLIENT_VERSION);
        CDataStream value(SER_DISK, CLIENT_VERSION);

        int ret = ReadAtCursor(cur, key, value, DB_NEXT);

        if (ret == DB_NOTFOUND)
        {
            break;
        }
        else if (ret != 0)
        {
            success = false;
            break;
        }

        std::string addr;
        key >> addr;

        if (addr == m_undelivered)
        {
            continue;
        }

        addresses.push_back(addr);
    }

    cur->close();

    return success;
}


//*****************************************************************************
//*****************************************************************************
StoredPubKeysDb::StoredPubKeysDb()
    : CDB("pubkeys.dat", "rw")
{
}

//*****************************************************************************
//*****************************************************************************
// static
StoredPubKeysDb & StoredPubKeysDb::instance()
{
    static StoredPubKeysDb db;
    return db;
}

//*****************************************************************************
//*****************************************************************************
bool StoredPubKeysDb::load(const std::string & address, CPubKey & key)
{
    LOCK(m_cs);
    return Read(address, key);
}

//*****************************************************************************
//*****************************************************************************
bool StoredPubKeysDb::store(const std::string & address, const CPubKey & key)
{
    LOCK(m_cs);
    return Write(address, key);
}
