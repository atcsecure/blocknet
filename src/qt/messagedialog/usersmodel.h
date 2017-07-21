//*****************************************************************************
//*****************************************************************************

#ifndef USERSMODEL_H
#define USERSMODEL_H

// #include "message.h"
// #include "message_metatype.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>

#include <vector>

class AddressTableModel;

//*****************************************************************************
//*****************************************************************************
class UsersModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum
    {
        roleAddress = Qt::UserRole,
        roleLabel,
        roleIsRead
    };

    struct Item
    {
        std::string addr;
        bool        isRead;

        Item(const std::string &_addr,
             const bool _isRead = false)
        {
            addr   = _addr;
            isRead = _isRead;
        }

        bool operator == (const std::string & _addr) const
        {
            return addr == _addr;
        }

        bool operator == (const Item & item) const
        {
            return addr == item.addr;
        }
    };

public:
    explicit UsersModel(QObject *parent = 0);

public:
    virtual int rowCount(const QModelIndex & parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void loadAddresses(const std::vector<std::string> & addresses);
    void addAddress(const std::string & address, bool isNewMessage = true);
    void deleteAddress(const std::string & address);

    void setAddressTableModel(AddressTableModel * model);
    // const std::vector<Message> & plainData() const;

    QString labelForAddress(const QString & address) const;
    QString labelForAddressWithAddress(const QString & address) const;

signals:

public slots:
    void onRead(const QModelIndex & index);

private:
    std::vector<Item>   m_items;

    AddressTableModel * m_addrTableModel;
};

#endif // USERSMODEL_H
