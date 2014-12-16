//******************************************************************************
//******************************************************************************

#ifndef XBRIDGEPACKET_H
#define XBRIDGEPACKET_H

#include <vector>
#include <deque>
#include <memory>
#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>

//******************************************************************************
//******************************************************************************
enum XBridgeCommand
{
    xbcInvalid = 0,
    xbcAnnounceAddresses,
    xbcXChatMessage
};

//******************************************************************************
//******************************************************************************
typedef boost::uint32_t crc_t;

//******************************************************************************
//******************************************************************************
class XBridgePacket
{
    std::vector<unsigned char> m_body;

public:
    enum
    {
        headerSize = 8,
        commandSize = sizeof(boost::uint32_t)
    };

    std::size_t     size()    const     { return sizeField(); }
    std::size_t     allSize() const     { return m_body.size(); }

    crc_t           crc()     const
    {
        // TODO implement this
        assert(false || "not implemented");
        return (0);
        // return crcField();
    }

    XBridgeCommand command() const      { return static_cast<XBridgeCommand>(commandField()); }

    void    alloc()             { m_body.resize(8 + size()); }

    unsigned char  * header()            { return &m_body[0]; }
    unsigned char  * data()              { return &m_body[8]; }

    // boost::int32_t int32Data() const { return field32<2>(); }

    void    clear()
    {
        m_body.resize(8);
        commandField() = 0;
        sizeField() = 0;

        // TODO crc
        // crcField() = 0;
    }

    void resize(const int size)
    {
        int s = size < 0 ? 0 : size;
        m_body.resize(s+8);
        sizeField() = s;
    }

    void    setData(const unsigned char data)
    {
        m_body.resize(sizeof(data) + 8);
        sizeField() = sizeof(data);
        m_body[8] = data;
    }

    void    setData(const boost::int32_t data)
    {
        m_body.resize(sizeof(data) + 8);
        sizeField() = sizeof(data);
        field32<2>() = data;
    }

    void    setData(const std::string & data)
    {
        m_body.resize(data.size() + 8);
        sizeField() = data.size();
        if (data.size())
        {
            data.copy((char *)(&m_body[8]), data.size());
        }
    }

    void    setData(const std::vector<unsigned char> & data, const int offset = 0)
    {
        setData(&data[0], data.size(), offset);
    }

    void    setData(const unsigned char * data, const int size, const int offset = 0)
    {
        int off = offset < 0 ? 0 : offset;
        off += 8;
        if (size)
        {
            if (m_body.size() < size+off)
            {
                m_body.resize(size+off);
                sizeField() = size+off-8;
            }
            memcpy(&m_body[off], data, size);
        }
    }

    XBridgePacket() : m_body(8, 0)
    {}

    explicit XBridgePacket(const std::string& raw) : m_body(raw.begin(), raw.end())
    {}

    XBridgePacket(const XBridgePacket & other)
    {
        m_body = other.m_body;
    }

    XBridgePacket(XBridgeCommand c) : m_body(8, 0)
    {
        commandField() = static_cast<boost::uint32_t>(c);
    }

    XBridgePacket & operator = (const XBridgePacket & other)
    {
        m_body    = other.m_body;

        return *this;
    }

private:
    template<std::size_t INDEX>
    boost::uint32_t &       field32()
        { return *static_cast<boost::uint32_t *>(static_cast<void *>(&m_body[INDEX * 4])); }

    template<std::size_t INDEX>
    boost::uint32_t const& field32() const
        { return *static_cast<boost::uint32_t const*>(static_cast<void const*>(&m_body[INDEX * 4])); }

    boost::uint32_t &       commandField()       { return field32<0>(); }
    boost::uint32_t const & commandField() const { return field32<0>(); }
    boost::uint32_t &       sizeField()          { return field32<1>(); }
    boost::uint32_t const & sizeField() const    { return field32<1>(); }

    // TODO implement this
//    boost::uint32_t &       crcField()           { return field32<0>(); }
//    boost::uint32_t const & crcField() const     { return field32<0>(); }
};

typedef boost::shared_ptr<XBridgePacket> XBridgePacketPtr;
typedef std::deque<XBridgePacketPtr>   XBridgePacketQueue;

#endif // XBRIDGEPACKET_H
