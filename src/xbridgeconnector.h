//******************************************************************************
//******************************************************************************

#ifndef XBRIDGECONNECTOR_H
#define XBRIDGECONNECTOR_H

#include "xbridgepacket.h"
#include "message.h"
#include "FastDelegate.h"

#include <boost/asio.hpp>
#include <boost/thread.hpp>

//******************************************************************************
//******************************************************************************
class XBridgeConnector
{
    friend XBridgeConnector & xbridge();

private:
    XBridgeConnector();

    enum
    {
        SERVER_LISTEN_PORT = 30330
    };

public:
    bool connect();
    bool isConnected() const;

    bool announceLocalAddresses();
    bool sendXChatMessage(const Message & m);

private:
    void run();

    void disconnect();

    void doReadHeader(XBridgePacketPtr packet,
                      const std::size_t offset = 0);
    void onReadHeader(XBridgePacketPtr packet,
                      const std::size_t offset,
                      const boost::system::error_code & error,
                      std::size_t transferred);

    void doReadBody(XBridgePacketPtr packet,
                    const std::size_t offset = 0);
    void onReadBody(XBridgePacketPtr packet,
                    const std::size_t offset,
                    const boost::system::error_code & error,
                    std::size_t transferred);

    bool encryptPacket(XBridgePacketPtr packet);
    bool decryptPacket(XBridgePacketPtr packet);
    bool processPacket(XBridgePacketPtr packet);

    bool processInvalid(XBridgePacketPtr packet);
    bool processXChatMessage(XBridgePacketPtr packet);

private:
    boost::asio::io_service      m_io;
    boost::asio::ip::tcp::socket m_socket;
    boost::thread                m_thread;


    typedef std::map<const int, fastdelegate::FastDelegate1<XBridgePacketPtr, bool> > PacketProcessorsMap;
    PacketProcessorsMap m_processors;
};

//******************************************************************************
//******************************************************************************
XBridgeConnector & xbridge();

#endif // XBRIDGECONNECTOR_H
