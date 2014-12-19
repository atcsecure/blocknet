//******************************************************************************
//******************************************************************************

#include "xbridgeconnector.h"
#include "base58.h"

#include <boost/foreach.hpp>
#include <boost/thread.hpp>

// TODO remove this
#include <QtDebug>

//******************************************************************************
//******************************************************************************
std::vector<std::string> getLocalBitcoinAddresses();

//******************************************************************************
//******************************************************************************
XBridgeConnector & xbridge()
{
    static XBridgeConnector connector;
    return connector;
}

//******************************************************************************
//******************************************************************************
XBridgeConnector::XBridgeConnector()
    : m_socket(m_io)
{
    m_processors[xbcInvalid]          .bind(this, &XBridgeConnector::processInvalid);
    m_processors[xbcXChatMessage]     .bind(this, &XBridgeConnector::processXChatMessage);
}

//******************************************************************************
//******************************************************************************
void XBridgeConnector::run()
{
    doReadHeader(XBridgePacketPtr(new XBridgePacket));
    m_io.run();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeConnector::disconnect()
{
    m_socket.close();
}

//*****************************************************************************
//*****************************************************************************
void XBridgeConnector::doReadHeader(XBridgePacketPtr packet,
                                  const std::size_t offset)
{
    m_socket.async_read_some(
                boost::asio::buffer(packet->header()+offset,
                                    packet->headerSize-offset),
                boost::bind(&XBridgeConnector::onReadHeader,
                            this,
                            packet, offset,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeConnector::onReadHeader(XBridgePacketPtr packet,
                                  const std::size_t offset,
                                  const boost::system::error_code & error,
                                  std::size_t transferred)
{
    if (error)
    {
        qDebug() << "ERROR <" << error.value() << "> " << error.message().c_str();
        disconnect();
        return;
    }

    if (offset + transferred != packet->headerSize)
    {
        qDebug() << "partially read header, read " << transferred
                 << " of " << packet->headerSize << " bytes";

        doReadHeader(packet, offset + transferred);
        return;
    }

    packet->alloc();
    doReadBody(packet);
}

//*****************************************************************************
//*****************************************************************************
void XBridgeConnector::doReadBody(XBridgePacketPtr packet,
                const std::size_t offset)
{
    m_socket.async_read_some(
                boost::asio::buffer(packet->data()+offset,
                                    packet->size()-offset),
                boost::bind(&XBridgeConnector::onReadBody,
                            this,
                            packet, offset,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
}

//*****************************************************************************
//*****************************************************************************
void XBridgeConnector::onReadBody(XBridgePacketPtr packet,
                                const std::size_t offset,
                                const boost::system::error_code & error,
                                std::size_t transferred = 0)
{
    if (error)
    {
        qDebug() << "ERROR <" << error.value() << "> " << error.message().c_str();
        disconnect();
        return;
    }

    if (offset + transferred != packet->size())
    {
        qDebug() << "partially read packet, read " << transferred
                 << " of " << packet->size() << " bytes";

        doReadBody(packet, offset + transferred);
        return;
    }

    if (!decryptPacket(packet))
    {
        qDebug() << "packet decoding error " << __FUNCTION__;
        return;
    }

    if (!processPacket(packet))
    {
        qDebug() << "packet processing error " << __FUNCTION__;
    }

    doReadHeader(XBridgePacketPtr(new XBridgePacket));
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeConnector::encryptPacket(XBridgePacketPtr /*packet*/)
{
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeConnector::decryptPacket(XBridgePacketPtr /*packet*/)
{
    // TODO implement this
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeConnector::processPacket(XBridgePacketPtr packet)
{
    XBridgeCommand c = packet->command();

    if (m_processors.count(c) == 0)
    {
        m_processors[xbcInvalid](packet);
        qDebug() << "incorrect command code <" << c << "> ";
        return false;
    }

    if (!m_processors[c](packet))
    {
        qDebug() << "packet processing error <" << c << "> ";
        return false;
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeConnector::connect()
{
    // connect to localhost
    boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("127.0.0.1"), SERVER_LISTEN_PORT);

    boost::system::error_code error;
    m_socket.connect(ep, error);
    if (error)
    {
        // connect error
        return false;
    }

    // start internal handlers
    m_thread = boost::thread(&XBridgeConnector::run, this);

    announceLocalAddresses();

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeConnector::isConnected() const
{
    return m_socket.is_open();
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeConnector::processInvalid(XBridgePacketPtr /*packet*/)
{
    qDebug() << "xbcInvalid command processed";
    return true;
}

//*****************************************************************************
//*****************************************************************************
bool XBridgeConnector::processXChatMessage(XBridgePacketPtr packet)
{
    // size must be > 20 bytes (160bit)
    if (packet->size() <= 20)
    {
        qDebug() << "invalid packet size for xbcXChatMessage " << __FUNCTION__;
        return false;
    }

    // skip 20 bytes dest address
    CDataStream stream((const char *)(packet->data()+20),
                       (const char *)(packet->data()+packet->size()));

    Message m;
    stream >> m;

    bool isForMe = false;
    if (!m.process(isForMe))
    {
        // TODO need relay?
        // relay, if message not for me
        // m.broadcast();
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeConnector::announceLocalAddresses()
{
    if (!isConnected())
    {
        return false;
    }

    std::vector<std::string> addresses = getLocalBitcoinAddresses();

    BOOST_FOREACH(const std::string & addr, addresses)
    {
        std::vector<unsigned char> tmp;
        DecodeBase58Check(addr, tmp);
        if (tmp.empty())
        {
            continue;
        }

        // size of tmp must be 21 byte
        XBridgePacket p(xbcAnnounceAddresses);
        p.setData(&tmp[1], tmp.size()-1);

        // TODO encryption
//        if (!encryptPacket(p))
//        {
//            // TODO logs or signal to gui
//            return false;
//        }

        boost::system::error_code error;
        m_socket.send(boost::asio::buffer(p.header(), p.allSize()), 0, error);
        if (error)
        {
            qDebug() << "send address error <"
                     << error.value() << "> " << error.message().c_str()
                     << " " << __FUNCTION__;
        }
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XBridgeConnector::sendXChatMessage(const Message & m)
{
    CDataStream stream;
    stream << m;

    std::vector<unsigned char> addr;
    DecodeBase58Check(m.to, addr);
    if (addr.empty())
    {
        // incorrect address
        return false;
    }

    XBridgePacket p(xbcXChatMessage);

    // copy dest address
    assert(addr.size() ==  21 || "address length must be 20 bytes + 1");
    p.setData(&addr[1], addr.size()-1);

    // copy message
    std::vector<unsigned char> message;
    std::copy(stream.begin(), stream.end(), std::back_inserter(message));
    p.setData(message, 20);

    // TODO encryption
//        if (!encryptPacket(p))
//        {
//            // TODO logs or signal to gui
//            return false;
//        }

    boost::system::error_code error;
    m_socket.send(boost::asio::buffer(p.header(), p.allSize()), 0, error);
    if (error)
    {
        qDebug() << "send address error <"
                 << error.value() << "> " << error.message().c_str()
                 << " " << __FUNCTION__;
    }

    return true;
}
