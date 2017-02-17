//*****************************************************************************
//*****************************************************************************

#include "xbridgeapp.h"
#include "xbridgeexchange.h"
#include "util/xutil.h"
#include "util/logger.h"
#include "util/settings.h"
// #include "dht/dht.h"
#include "version.h"
#include "config.h"
#include "xuiconnector.h"
#include "bitcoinrpc.h"
// #include "bitcoinrpcconnection.h"

//#ifndef NO_GUI
//#include "../qt/xbridgeui/xbridgetransactionsview.h"
//#endif

#include <thread>
#include <chrono>
#include <assert.h>

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>

#include <openssl/rand.h>
#include <openssl/md5.h>

#define dht_fromAddress 0
#define dht_toAddress 1
#define dht_packet 2
#define dht_resendFlag 3

//*****************************************************************************
//*****************************************************************************
XUIConnector xuiConnector;

//*****************************************************************************
//*****************************************************************************
boost::mutex                                  XBridgeApp::m_txLocker;
std::map<uint256, XBridgeTransactionDescrPtr> XBridgeApp::m_pendingTransactions;
std::map<uint256, XBridgeTransactionDescrPtr> XBridgeApp::m_transactions;
boost::mutex                                  XBridgeApp::m_txUnconfirmedLocker;
std::map<uint256, XBridgeTransactionDescrPtr> XBridgeApp::m_unconfirmed;

//*****************************************************************************
//*****************************************************************************
void badaboom()
{
    int * a = 0;
    *a = 0;
}

//*****************************************************************************
//*****************************************************************************
XBridgeApp::XBridgeApp()
    : m_signalGenerate(false)
    , m_signalDump(false)
    , m_signalSearch(false)
    , m_signalSend(false)
    , m_ipv4(true)
    , m_ipv6(true)
    , m_dhtPort(Config::DHT_PORT)
    , m_serviceSession(new XBridgeSession)
{
}

//*****************************************************************************
//*****************************************************************************
XBridgeApp::~XBridgeApp()
{
#ifdef WIN32
    WSACleanup();
#endif
}

//*****************************************************************************
//*****************************************************************************
// static
XBridgeApp & XBridgeApp::instance()
{
    static XBridgeApp app;
    return app;
}

//*****************************************************************************
//*****************************************************************************
// static
std::string XBridgeApp::version()
{
    std::ostringstream o;
    o << XBRIDGE_VERSION_MAJOR
      << "." << XBRIDGE_VERSION_MINOR
      << "." << XBRIDGE_VERSION_DESCR
      << " [" << XBRIDGE_VERSION << "]";
    return o.str();
}

//*****************************************************************************
//*****************************************************************************
int XBridgeApp::exec()
{
//#ifdef NO_GUI
//    m_threads.join_all();
//    return 0;

//#else
//    MainWindow view;
//    view.show();

//    return m_app->exec();
//#endif
}

//*****************************************************************************
//*****************************************************************************
const unsigned char hash[20] =
{
    0x54, 0x57, 0x87, 0x89, 0xdf, 0xc4, 0x23, 0xee, 0xf6, 0x03,
    0x1f, 0x81, 0x94, 0xa9, 0x3a, 0x16, 0x98, 0x8b, 0x72, 0x7b
};

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::init(int argc, char *argv[])
{
#ifndef NO_GUI
    m_app.reset(new QApplication(argc, argv));
#endif
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::initDht()
{
    LOG() << "initialize v." << version();

#ifdef WIN32
    WSADATA wsa = {0};
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (rc != 0)
    {
        LOG() << "startup error";
        return false;
    }
#endif

    Settings & s = settings();
    m_dhtPort    = s.dhtPort();

    std::vector<std::string> peers = s.peers();
    for (std::vector<std::string>::iterator i = peers.begin(); i != peers.end(); ++i)
    {
        std::string peer = *i;
        std::string port = boost::lexical_cast<std::string>(Config::DHT_PORT);

        size_t idx = peer.find(':');
        if (idx != std::string::npos)
        {
            port = peer.substr(idx+1);
            peer = peer.substr(0, idx);
        }

        LOG() << "peer -> " << peer << ":" << port;

        addrinfo   hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_family   = !m_ipv6 ? AF_INET :
                            !m_ipv4 ? AF_INET6 : 0;

        addrinfo * info = 0;
        int rc = getaddrinfo(peer.c_str(), port.c_str(), &hints, &info);
        if (rc != 0)
        {
            LOG() << "getaddrinfo failed " << rc << gai_strerror(rc);
            continue;
        }

        addrinfo * infop = info;
        while(infop)
        {
            sockaddr_storage tmp;
            memcpy(&tmp, infop->ai_addr, infop->ai_addrlen);
            m_nodes.push_back(tmp);
            infop = infop->ai_next;
        }
        freeaddrinfo(info);
    }

    // start xbrige
    m_bridge = XBridgePtr(new XBridge());

    // start dht
    memset(&m_sin, 0, sizeof(m_sin));
    m_sin.sin_family = AF_INET;
    m_sin.sin_port = htons(static_cast<unsigned short>(m_dhtPort));

    memset(&m_sin6, 0, sizeof(m_sin6));
    m_sin6.sin6_family = AF_INET6;
    m_sin6.sin6_port = htons(static_cast<unsigned short>(m_dhtPort));

//    dht_debug = true;

    // start dht thread
    m_dhtStarted = false;
    m_dhtStop    = false;

    // m_threads.create_thread(boost::bind(&XBridgeApp::dhtThreadProc, this));
    // m_threads.create_thread(boost::bind(&XBridgeApp::bridgeThreadProc, this));

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::stop()
{
    LOG() << "stopping threads...";
    m_dhtStop = true;
    m_rpcStop = true;

    m_bridge->stop();

    m_threads.join_all();

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::initRpc()
{
    Settings & s = settings();
    if (!s.rpcEnabled())
    {
        return true;
    }

    assert(!"rpc not tested");
    return true;

    m_rpcStop = false;

    m_threads.create_thread(boost::bind(&XBridgeApp::rpcThreadProc, this));
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::signalRpcStopActive() const
{
    return m_rpcStop;
}

//*****************************************************************************
//*****************************************************************************
//void XBridgeApp::logMessage(const QString & msg)
//{
//    emit showLogMessage(msg);
//}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::onGenerate()
{
    m_signalGenerate = true;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::onDump()
{
    m_signalDump = true;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::onSearch(const std::string & id)
{
    m_searchStrings.push_back(id);
    m_signalSearch = true;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::onSend(const UcharVector & from, const UcharVector & message)
{
//    if (from.size() != 20)
//    {
//        return;
//    }

    // dht_fromAddress, dht_toAddress, dht_packet, dht_resendFlag
    m_messages.push_back(std::make_tuple(from, UcharVector(), message, false));
    m_signalSend = true;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::onSend(const UcharVector & from, const XBridgePacketPtr packet)
{
    UcharVector v;
    std::copy(packet->header(), packet->header()+packet->allSize(), std::back_inserter(v));
    onSend(from, v);
}

//*****************************************************************************
// send packet to xbridge network to specified id,
// or broadcast, when id is empty
//*****************************************************************************
void XBridgeApp::onSend(const UcharVector & from, const UcharVector & id, const UcharVector & message)
{
    // dht_toAddress, dht_packet, dht_resendFlag
    m_messages.push_back(std::make_tuple(from, id, message, false));
    m_signalSend = true;
}

//*****************************************************************************
void XBridgeApp::onSend(const UcharVector & from, const UcharVector & id, const XBridgePacketPtr packet)
{
    UcharVector v;
    std::copy(packet->header(), packet->header()+packet->allSize(), std::back_inserter(v));
    onSend(from, id, v);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::onMessageReceived(const UcharVector & id, const UcharVector & message)
{
    static UcharVector localid(m_myid, m_myid+20);

    XBridgePacketPtr packet(new XBridgePacket);
    packet->copyFrom(message);

    LOG() << "received message to" << util::base64_encode(std::string((char *)&id[0], 20)).c_str()
             << " command " << packet->command();

    if (!XBridgeSession::checkXBridgePacketVersion(packet))
    {
        ERR() << "incorrect protocol version <" << packet->version() << "> " << __FUNCTION__;
        return;
    }

    boost::mutex::scoped_lock l(m_sessionsLock);
    if (m_sessionAddrs.count(id))
    {
        // found local client
        XBridgeSessionPtr ptr = m_sessionAddrs[id];
        ptr->processPacket(packet);

        // ptr->sendXBridgeMessage(message);
    }

    // check service session
    else if (memcmp(m_serviceSession->sessionAddr(), &id[0], 20) == 0)
    {
        serviceSession()->processPacket(packet);
    }

    // check local address
    else if (id == localid)
    {
        // process packet
        // XBridgeSessionPtr ptr(new XBridgeSession);
        serviceSession()->processPacket(packet);
    }

    else
    {
        LOG() << "process message for unknown address";
    }
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionPtr XBridgeApp::serviceSession()
{
    return m_serviceSession;
}

//*****************************************************************************
//*****************************************************************************
//XBridgeSessionPtr XBridgeApp::queuedSession()
//{
//    // XBridgeSessionPtr ptr(new XBridgeSession);
//    XBridgeSessionPtr ptr = m_sessionQueue.front();

//    {
//        // TODO ????
//        // or process all packets in first session?
//        // or create service session?
//        boost::mutex::scoped_lock l(m_sessionsLock);
//        m_sessionQueue.push(m_sessionQueue.front());
//        m_sessionQueue.pop();
//    }

//    return ptr;
//}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::onBroadcastReceived(const std::vector<unsigned char> & message)
{
    // LOG() << "received broadcast message";

    // process message
    XBridgePacketPtr packet(new XBridgePacket);
    packet->copyFrom(message);

    LOG() << "broadcast message, command " << packet->command();

    if (!XBridgeSession::checkXBridgePacketVersion(packet))
    {
        ERR() << "incorrect protocol version <" << packet->version() << "> " << __FUNCTION__;
        return;
    }

    // XBridgeSessionPtr ptr(new XBridgeSession);
    serviceSession()->processPacket(packet);
}

//*****************************************************************************
//*****************************************************************************
// static
void XBridgeApp::sleep(const unsigned int umilliseconds)
{
//    QEventLoop loop;
//    QTimer::singleShot(umilliseconds, &loop, SLOT(quit()));
//    loop.exec(QEventLoop::ExcludeUserInputEvents);
    std::this_thread::sleep_for(std::chrono::milliseconds(umilliseconds));
}

//*****************************************************************************
/* The call-back function is called by the DHT whenever something
   interesting happens.  Right now, it only happens when we get a new value or
   when a search completes, but this may be extended in future versions. */
//*****************************************************************************
//void callback(void * closure, int event,
//              const unsigned char * /*info_hash*/,
//              const void * /*data*/, size_t data_len)
//{
//    XBridgeApp * app = static_cast<XBridgeApp *>(closure);

//    if (event == DHT_EVENT_SEARCH_DONE || event == DHT_EVENT_SEARCH_DONE6)
//    {
//        LOG() << ((event == DHT_EVENT_SEARCH_DONE6) ?
//                        "Search done(6)" : "Search done");

//        if (app->m_messages.size())
//        {
//            app->m_signalSend = true;
//        }
//    }

//    else if(event == DHT_EVENT_VALUES)
//    {
//        LOG() << "Received " << (int)(data_len / 6) << " values";
//    }
//    else if (event == DHT_EVENT_VALUES6)
//    {
//        LOG() << "Received " << (int)(data_len / 6) << " values(6)";
//    }
//}

//*****************************************************************************
//*****************************************************************************
//void XBridgeApp::dhtThreadProc()
//{
//    sleep(500);

//    LOG() << "started";

//    // generate random id
//    dht_random_bytes(m_myid, sizeof(m_myid));
//    LOG() << "generated id <"
//             << util::base64_encode(std::string((char *)m_myid, sizeof(m_myid))).c_str()
//             << ">";

//    // init s4
//    int s4 = m_ipv4 ? socket(PF_INET, SOCK_DGRAM, 0) : -1;
//    if (m_ipv4 && s4 == INVALID_SOCKET)
//    {
//        LOG() << "s4 error";
//    }

//    // init s6
//    int s6 = m_ipv6 ? socket(PF_INET6, SOCK_DGRAM, 0) : -1;
//    if (m_ipv6 && s6 == INVALID_SOCKET)
//    {
//        LOG() << "s6 error";
//    }

//    // check no sockets
//    if (s4 < 0 && s6 < 0)
//    {
//        LOG() << "no socket";
//        return;
//    }

//    int rc  = 0;
//    int rc6 = 0;

//    // bind s4
//    if (s4 >= 0)
//    {
//        rc = bind(s4, (sockaddr *)&m_sin, sizeof(m_sin));
//        if (rc < 0)
//        {
//            LOG() << "s4 bind error";
//        }
//    }

//    // bind s6
//    if (s6 >= 0)
//    {
//        int val = 1;
//        rc6 = setsockopt(s6, IPPROTO_IPV6, IPV6_V6ONLY,
//                        (char *)&val, sizeof(val));
//        if (rc6 < 0)
//        {
//            LOG() << "s6 set opt error";
//        }
//        else
//        {
//            /* BEP-32 mandates that we should bind this socket to one of our
//               global IPv6 addresses.  In this simple example, this only
//               happens if the user used the -b flag. */

//            rc6 = bind(s6, (struct sockaddr*)&m_sin6, sizeof(m_sin6));
//            if (rc6 < 0)
//            {
//                LOG() << "s6 bind error";
//            }
//        }
//    }

//    if (rc < 0 && rc6 < 0)
//    {
//#ifdef WIN32
//        closesocket(s6);
//        closesocket(s4);
//#else
//        close(s6);
//        close(s4);
//#endif
//        return;
//    }

//    rc = dht_init(s4, s6, m_myid);
//    if (rc < 0)
//    {
//        LOG() << "dht_init error";
//#ifdef WIN32
//        closesocket(s6);
//        closesocket(s4);
//#else
//        close(s6);
//        close(s4);
//#endif
//        return;
//    }

//    m_dhtStarted = true;

//    time_t tosleep = 0;
//    char buf[4096];

//    // ping nodes (bootstrap)
//    for (size_t i = 0; i < m_nodes.size(); ++i)
//    {
//        LOG() << "ping";
//        dht_ping_node((struct sockaddr*)&m_nodes[i], sizeof(m_nodes[i]));
//        sleep(rand() % 100);
//    }

//    while (!m_dhtStop)
//    {
//        // LOG() << "working";

//        fd_set readfds;

//        timeval tv;
//        // tv.tv_sec = tosleep;
//        tv.tv_sec = 1;
//        tv.tv_usec = rand() % 1000000;

//        FD_ZERO(&readfds);
//        if(s4 >= 0)
//        {
//            FD_SET(s4, &readfds);
//        }
//        if(s6 >= 0)
//        {
//            FD_SET(s6, &readfds);
//        }
//        rc = select(s4 > s6 ? s4 + 1 : s6 + 1, &readfds, NULL, NULL, &tv);
//        if (rc < 0)
//        {
//            if (errno != EINTR)
//            {
//                LOG() << "errno";
//                break;
//            }
//        }

//        sockaddr_storage from;
//        socklen_t fromlen = sizeof(from);

//        if (rc > 0)
//        {
//            if (s4 >= 0 && FD_ISSET(s4, &readfds))
//            {
//                rc = recvfrom(s4, buf, sizeof(buf) - 1, 0,
//                              (struct sockaddr*)&from, &fromlen);
//            }
//            else if(s6 >= 0 && FD_ISSET(s6, &readfds))
//            {
//                rc = recvfrom(s6, buf, sizeof(buf) - 1, 0,
//                              (struct sockaddr*)&from, &fromlen);
//            }
//            else
//            {
//                break;
//            }

//            // LOG() << "read";
//            // LOG() << buf;
//        }

//        if (rc > 0)
//        {
//            buf[rc] = '\0';
//            rc = dht_periodic((unsigned char *)buf, rc, (struct sockaddr*)&from, fromlen,
//                              &tosleep, callback, NULL);
//        }
//        else
//        {
//            rc = dht_periodic(NULL, 0, NULL, 0, &tosleep, callback, NULL);
//        }

//        if (rc < 0)
//        {
//            if(errno == EINTR)
//            {
//                LOG() << "continue";
//                continue;
//            }
//            else
//            {
//                LOG() << "dht_periodic";
//                if (rc == EINVAL || rc == EFAULT)
//                {
//                    break;
//                }
//                tosleep = 1;
//            }
//        }

//        if (m_signalGenerate)
//        {
//            LOG() << "generate new entity";
//            unsigned char e[20];
//            dht_random_bytes(e, sizeof(e));
//            dht_storage_store(e, (sockaddr *)&m_sin, m_dhtPort);

//            m_signalGenerate = false;
//        }

//        // This is how you trigger a search for a  hash.  If port
//        // (the second argument) is non-zero, it also performs an announce.
//        // Since peers expire announced data after 30 minutes, it's a good
//        // idea to reannounce every 28 minutes or so
//        else if (m_signalSearch)
//        {
//            while (m_searchStrings.size())
//            {
//                std::string str = m_searchStrings.front();
//                m_searchStrings.pop_front();

//                str = util::base64_decode(str);
//                if (!str.length())
//                {
//                    LOG() << "searching, skipped empty or error data";
//                    continue;
//                }

//                LOG() << "searching " << str.length() << " bytes";
//                if (s4 >= 0)
//                {
//                    dht_search((const unsigned char *)str.c_str(), 0, AF_INET, callback, this);
//                }
//                if (s6 >= 0)
//                {
//                    dht_search((const unsigned char *)str.c_str(), 0, AF_INET6, callback, this);
//                }
//            }

//            m_signalSearch = false;
//        }

//        if (m_signalSend)
//        {
//            // LOG() << "sendind";

//            if (m_messages.size())
//            {
//                // TODO sync
//                std::list<MessagePair> messages = m_messages;
//                m_messages.clear();

//                while (messages.size())
//                {
//                    MessagePair mpair = messages.front();
//                    messages.pop_front();

//                    std::vector<unsigned char> & from    = std::get<dht_fromAddress>(mpair);
//                    std::vector<unsigned char> & id      = std::get<dht_toAddress>(mpair);
//                    std::vector<unsigned char> & message = std::get<dht_packet>(mpair);

//                    if (isKnownMessage(message))
//                    {
//                        continue;
//                    }

//                    // check broadcast
//                    if (id.empty())
//                    {
//                        // add to known
//                        boost::mutex::scoped_lock l(m_messagesLock);
//                        m_processedMessages.insert(Hash(message.begin(), message.end()));

//                        // send to all local clients
//                        {
//                            boost::mutex::scoped_lock l(m_sessionsLock);
//                            for (SessionIdMap::iterator i = m_sessionIds.begin(); i != m_sessionIds.end(); ++i)
//                            {
//                                // not for sender
//                                XBridgeSessionPtr s = std::get<1>(*i);
//                                if ((from.size() != 20) || (memcmp(s->sessionAddr(), &from[0], 20) != 0))
//                                {
//                                    s->takeXBridgeMessage(message);
//                                }
//                            }
//                        }

//                        // send to xbridge network
//                        dht_send_broadcast(&message[0], message.size());
//                    }

//                    else
//                    {
//                        bool isFoundLocal = false;

//                        // check local
//                        {
//                            boost::mutex::scoped_lock l(m_sessionsLock);
//                            if (m_sessionAddrs.count(id))
//                            {
//                                // found local client
//                                XBridgeSessionPtr ptr = m_sessionAddrs[id];
//                                ptr->takeXBridgeMessage(message);

//                                isFoundLocal = true;

//                                // add to known
//                                boost::mutex::scoped_lock l(m_messagesLock);
//                                m_processedMessages.insert(Hash(message.begin(), message.end()));
//                            }
//                        }

//                        if (!isFoundLocal)
//                        {
//                            // not local
//                            int err = dht_send_message(&id[0], &message[0], message.size());
//                            if (!err)
//                            {
//                                // add to known
//                                boost::mutex::scoped_lock l(m_messagesLock);
//                                m_processedMessages.insert(Hash(message.begin(), message.end()));
//                            }

//                            if (err != 0 && err != DHT_NETWORK_BUFFER_OWERFLOW)
//                            {
//                                // not send - go to search peer
//                                std::string _id;
//                                std::copy(id.begin(), id.end(), std::back_inserter(_id));

//                                if (std::get<dht_resendFlag>(mpair))
//                                {
//                                    // error resend after search, drop this message
//                                    LOG() << "drop message to <"
//                                             << _id.c_str()
//                                             << "> (not found)";
//                                }
//                                else
//                                {
//                                    // return message back and try search
//                                    std::get<dht_resendFlag>(mpair) = true;
//                                    m_messages.push_back(mpair);
//                                    m_searchStrings.push_back(util::base64_encode(_id));
//                                    m_signalSearch = true;
//                                }
//                            }
//                            else if (err == DHT_NETWORK_BUFFER_OWERFLOW)
//                            {
//                                LOG() << "NETWORK_BUFFER_OWERFLOW";
//                            }
//                        }
//                    }
//                }
//            }

//            m_signalSend = false;
//        }

//        // For debugging, or idle curiosity
//        else if (m_signalDump)
//        {
//            LOG() << "dumping";
//            std::string dump;
//            dht_dump_tables(dump);
//            LOG() << dump.c_str();
//            m_signalDump = false;
//        }
//    }

//    {
//        struct sockaddr_in sin[500];
//        struct sockaddr_in6 sin6[500];
//        int tmpNodes  = 500;
//        int tmpNodes6 = 500;

//        int i = dht_get_nodes(sin, &tmpNodes, sin6, &tmpNodes6);
//        LOG() << "Found " << i << "(" << tmpNodes << " + " << tmpNodes6 << ") good nodes";
//    }

//    dht_uninit();

//#ifdef WIN32
//        closesocket(s6);
//        closesocket(s4);
//#else
//        close(s6);
//        close(s4);
//#endif

//    LOG() << "stopped";
//}

//*****************************************************************************
//*****************************************************************************
//int dht_blacklisted(const struct sockaddr * /*sa*/, int /*salen*/)
//{
//    return 0;
//}

#include <stdio.h>

//*****************************************************************************
// We need to provide a reasonably strong cryptographic hashing function.
// Here's how we'd do it if we had RSA's MD5 code.
//*****************************************************************************
//#if 1 // 0
//void dht_hash(void *hash_return, int hash_size,
//         const void *v1, int len1,
//         const void *v2, int len2,
//         const void *v3, int len3)
//{
//    static MD5_CTX ctx;
//    MD5_Init(&ctx);
//    MD5_Update(&ctx, v1, len1);
//    MD5_Update(&ctx, v2, len2);
//    MD5_Update(&ctx, v3, len3);

//    unsigned char md_buf[MD5_DIGEST_LENGTH];
//    MD5_Final(md_buf, &ctx);

//    if(hash_size > MD5_DIGEST_LENGTH)
//        memset((char*)hash_return + MD5_DIGEST_LENGTH, 0, hash_size - MD5_DIGEST_LENGTH);
//    memcpy(hash_return, md_buf, hash_size > 16 ? 16 : hash_size);
//}
//#else
///* But for this example, we might as well use something weaker. */
//void dht_hash(void * /*hash_return*/, int /*hash_size*/,
//         const void * /*v1*/, int /*len1*/,
//         const void * /*v2*/, int /*len2*/,
//         const void * /*v3*/, int /*len3*/)
//{
////    const char *c1 = v1, *c2 = v2, *c3 = v3;
////    char key[9];                /* crypt is limited to 8 characters */
////    int i;

////    memset(key, 0, 9);
////#define CRYPT_HAPPY(c) ((c % 0x60) + 0x20)

////    for(i = 0; i < 2 && i < len1; i++)
////        key[i] = CRYPT_HAPPY(c1[i]);
////    for(i = 0; i < 4 && i < len1; i++)
////        key[2 + i] = CRYPT_HAPPY(c2[i]);
////    for(i = 0; i < 2 && i < len1; i++)
////        key[6 + i] = CRYPT_HAPPY(c3[i]);
////    strncpy(hash_return, crypt(key, "jc"), hash_size);
//}
//#endif

//*****************************************************************************
//*****************************************************************************
//int dht_random_bytes(unsigned char * buf, size_t size)
//{
//    return RAND_bytes(buf, size);
//}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::bridgeThreadProc()
{
    m_bridge->run();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::rpcThreadProc()
{
    assert(!"rpc server");
    // rpc::threadRPCServer();
}

//*****************************************************************************
//*****************************************************************************
XBridgeSessionPtr XBridgeApp::sessionByCurrency(const std::string & currency) const
{
    boost::mutex::scoped_lock l(m_sessionsLock);
    if (m_sessionIds.count(currency))
    {
        return m_sessionIds.at(currency);
    }

    return XBridgeSessionPtr();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::addSession(XBridgeSessionPtr session)
{
    storageStore(session, session->sessionAddr());

    boost::mutex::scoped_lock l(m_sessionsLock);
    m_sessionQueue.push(session);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::storageStore(XBridgeSessionPtr session, const unsigned char * data)
{
    if (!data)
    {
        return;
    }

    std::vector<unsigned char> id(data, data+20);

    // TODO :)
    // if (m_sessionAddrs.contains(id))

    boost::mutex::scoped_lock l(m_sessionsLock);
    m_sessionAddrs[id] = session;
    m_sessionIds[session->currency()] = session;

//    dht_storage_store(data, (sockaddr *)&m_sin, m_dhtPort);
//    dht_storage_store(data, (sockaddr *)&m_sin6, m_dhtPort);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::storageClean(XBridgeSessionPtr session)
{
    boost::mutex::scoped_lock l(m_sessionsLock);
    for (auto i = m_sessionAddrs.begin(); i != m_sessionAddrs.end();)
    {
        if (i->second == session)
        {
            m_sessionAddrs.erase(i++);
        }
        else
        {
            ++i;
        }
    }
    for (auto i = m_sessionIds.begin(); i != m_sessionIds.end();)
    {
        if (i->second == session)
        {
            m_sessionIds.erase(i++);
        }
        else
        {
            ++i;
        }
    }
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::isLocalAddress(const std::vector<unsigned char> & id)
{
    static UcharVector localid(m_myid, m_myid+20);

    boost::mutex::scoped_lock l(m_sessionsLock);
    if (m_sessionAddrs.count(id))
    {
        return true;
    }

    // check service session address
    else if (memcmp(m_serviceSession->sessionAddr(), &id[0], 20) == 0)
    {
        return true;
    }

    // check local address
    else if (id == localid)
    {
        // process packet
        return true;
    }

    return false;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeApp::isKnownMessage(const std::vector<unsigned char> & message)
{
    boost::mutex::scoped_lock l(m_messagesLock);
    return m_processedMessages.count(Hash(message.begin(), message.end())) > 0;
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::addToKnown(const std::vector<unsigned char> & message)
{
    // add to known
    boost::mutex::scoped_lock l(m_messagesLock);
    m_processedMessages.insert(Hash(message.begin(), message.end()));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::storeAddressBookEntry(const std::string & currency,
                                       const std::string & name,
                                       const std::string & address)
{
    // TODO fix this, potentially deadlock
    // boost::mutex::try_lock l(m_addressBookLock);
    // boost::mutex::scoped_lock l(m_addressBookLock);
    // if (l.lock())
    {
        if (!m_addresses.count(address))
        {
            m_addresses.insert(address);
            m_addressBook.push_back(std::make_tuple(currency, name, address));
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::resendAddressBook()
{
    boost::mutex::scoped_lock l(m_addressBookLock);

    for (SessionIdMap::iterator i = m_sessionIds.begin(); i != m_sessionIds.end(); ++i)
    {
        for (AddressBook::iterator ii = m_addressBook.begin(); ii != m_addressBook.end(); ++ii)
        {
            i->second->sendAddressbookEntry(std::get<0>(*ii), std::get<1>(*ii), std::get<2>(*ii));
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::getAddressBook()
{
    boost::mutex::scoped_lock l(m_addressBookLock);

    for (SessionIdMap::iterator i = m_sessionIds.begin(); i != m_sessionIds.end(); ++i)
    {
        i->second->requestAddressBook();
    }
}

//*****************************************************************************
//*****************************************************************************
void XBridgeApp::checkUnconfirmedTx()
{
    boost::mutex::scoped_lock l(m_addressBookLock);

    for (SessionIdMap::iterator i = m_sessionIds.begin(); i != m_sessionIds.end(); ++i)
    {
        i->second->requestUnconfirmedTx();
    }
}

//******************************************************************************
//******************************************************************************
uint256 XBridgeApp::sendXBridgeTransaction(const std::string & from,
                                           const std::string & fromCurrency,
                                           const uint64_t & fromAmount,
                                           const std::string & to,
                                           const std::string & toCurrency,
                                           const uint64_t & toAmount)
{
    if (fromCurrency.size() > 8 || toCurrency.size() > 8)
    {
        assert(false && "invalid currency");
        return uint256();
    }

    boost::uint32_t timestamp = time(0);
    uint256 id = Hash(from.begin(), from.end(),
                      fromCurrency.begin(), fromCurrency.end(),
                      BEGIN(fromAmount), END(fromAmount),
                      to.begin(), to.end(),
                      toCurrency.begin(), toCurrency.end(),
                      BEGIN(toAmount), END(toAmount),
                      BEGIN(timestamp), END(timestamp));

    XBridgeTransactionDescrPtr ptr(new XBridgeTransactionDescr);
    ptr->id           = id;
    ptr->from         = from;
    ptr->fromCurrency = fromCurrency;
    ptr->fromAmount   = fromAmount;
    ptr->to           = to;
    ptr->toCurrency   = toCurrency;
    ptr->toAmount     = toAmount;

    {
        boost::mutex::scoped_lock l(m_txLocker);
        m_pendingTransactions[id] = ptr;
    }

    // try send immediatelly
    sendPendingTransaction(ptr);

    return id;
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::sendPendingTransaction(XBridgeTransactionDescrPtr & ptr)
{
    // if (!ptr->packet)
    {
        if (ptr->from.size() == 0 || ptr->to.size() == 0)
        {
            // TODO temporary
            return false;
        }

        if (ptr->packet && ptr->packet->command() != xbcTransaction)
        {
            // not send pending packets if not an xbcTransaction
            return true;
        }

        ptr->packet.reset(new XBridgePacket(xbcTransaction));

        // field length must be 8 bytes
        std::vector<unsigned char> fc(8, 0);
        std::copy(ptr->fromCurrency.begin(), ptr->fromCurrency.end(), fc.begin());

        // field length must be 8 bytes
        std::vector<unsigned char> tc(8, 0);
        std::copy(ptr->toCurrency.begin(), ptr->toCurrency.end(), tc.begin());

        // 32 bytes - id of transaction
        // 2x
        // 34 bytes - address
        //  8 bytes - currency
        //  8 bytes - amount
        ptr->packet->append(ptr->id.begin(), 32);
        ptr->packet->append(ptr->from);
        ptr->packet->append(fc);
        ptr->packet->append(ptr->fromAmount);
        ptr->packet->append(ptr->to);
        ptr->packet->append(tc);
        ptr->packet->append(ptr->toAmount);
    }

    onSend(std::vector<unsigned char>(m_myid, m_myid+20), ptr->packet);

    ptr->state = XBridgeTransactionDescr::trPending;

    xuiConnector.NotifyXBridgeTransactionStateChanged(ptr->id, XBridgeTransactionDescr::trPending);

    return true;
}

//******************************************************************************
//******************************************************************************
uint256 XBridgeApp::acceptXBridgeTransaction(const uint256 & id,
                                             const std::string & from,
                                             const std::string & to)
{
    XBridgeTransactionDescrPtr ptr;

    {
        boost::mutex::scoped_lock l(m_txLocker);
        if (!m_pendingTransactions.count(id))
        {
            return uint256();
        }
        ptr = m_pendingTransactions[id];
        ptr->from = from;
        ptr->to   = to;
        std::swap(ptr->fromCurrency, ptr->toCurrency);
        std::swap(ptr->fromAmount,   ptr->toAmount);
    }

    // try send immediatelly
    sendAcceptingTransaction(ptr);

    return id;
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::sendAcceptingTransaction(XBridgeTransactionDescrPtr & ptr)
{
    ptr->packet.reset(new XBridgePacket(xbcTransactionAccepting));

    // field length must be 8 bytes
    std::vector<unsigned char> fc(8, 0);
    std::copy(ptr->fromCurrency.begin(), ptr->fromCurrency.end(), fc.begin());

    // field length must be 8 bytes
    std::vector<unsigned char> tc(8, 0);
    std::copy(ptr->toCurrency.begin(), ptr->toCurrency.end(), tc.begin());

    std::vector<unsigned char> thisAddress(m_myid, m_myid+20);

    // 20 bytes - id of transaction
    // 2x
    // 34 bytes - address
    //  8 bytes - currency
    //  4 bytes - amount
    ptr->packet->append(ptr->hubAddress);
    ptr->packet->append(ptr->id.begin(), 32);
    ptr->packet->append(ptr->from);
    ptr->packet->append(fc);
    ptr->packet->append(ptr->fromAmount);
    ptr->packet->append(ptr->to);
    ptr->packet->append(tc);
    ptr->packet->append(ptr->toAmount);

    onSend(thisAddress, ptr->hubAddress, ptr->packet);

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::cancelXBridgeTransaction(const uint256 & id,
                                          const TxCancelReason & reason)
{
    {
        boost::mutex::scoped_lock l(m_txLocker);
        m_pendingTransactions.erase(id);
        if (m_transactions.count(id))
        {
            m_transactions[id]->state = XBridgeTransactionDescr::trCancelled;
        }
    }

    return sendCancelTransaction(id, reason);
}

//******************************************************************************
//******************************************************************************
bool XBridgeApp::sendCancelTransaction(const uint256 & txid,
                                       const TxCancelReason & reason)
{
    XBridgePacketPtr reply(new XBridgePacket(xbcTransactionCancel));
    reply->append(txid.begin(), 32);
    reply->append(static_cast<uint32_t>(reason));

    onSend(std::vector<unsigned char>(), reply);

    // cancelled
    return true;
}

//******************************************************************************
//******************************************************************************
//int XBridgeApp::peersCount() const
//{
//    return dht_get_count(0, 0);
//}

//******************************************************************************
//******************************************************************************
void XBridgeApp::handleRpcRequest(rpc::AcceptedConnection * conn)
{
    // m_threads.create_thread(boost::bind(&XBridgeApp::rpcHandlerProc, this, conn));
}

