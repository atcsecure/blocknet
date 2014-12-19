//*****************************************************************************
//*****************************************************************************

#include "usersmodel.h"
#include "../addresstablemodel.h"

#include <QDebug>

//*****************************************************************************
//*****************************************************************************
UsersModel::UsersModel(QObject *parent) :
    QAbstractListModel(parent)
  , m_addrTableModel(0)
{
}

//*****************************************************************************
//*****************************************************************************
// virtual
int UsersModel::rowCount(const QModelIndex & parent) const
{
    Q_UNUSED(parent)
    return m_items.size();
}

//*****************************************************************************
//*****************************************************************************
// virtual
QVariant UsersModel::data(const QModelIndex &index, int role) const
{
    quint32 row = static_cast<quint32>(index.row());
    if (row >= m_items.size())
    {
        return QVariant();
    }

    const Item & i = m_items.at(row);

    QString addr = QString::fromStdString(i.addr);
    QString label = labelForAddressWithAddress(addr);

    if (role == Qt::DisplayRole)
    {
        return (!i.isRead ? "* " : "") + label;
    }
    else if (role == Qt::ToolTipRole)
    {
        return label;
    }
    else if (role == roleAddress)
    {
        return addr;
    }
    else if (role == roleLabel)
    {
        return labelForAddress(addr);
    }
    else if (role == roleIsRead)
    {
        return i.isRead;
    }

    return QVariant();
}

//*****************************************************************************
//*****************************************************************************
void UsersModel::loadAddresses(const std::vector<std::string> & addresses)
{
    emit beginResetModel();
    // m_items = addresses;
    for (std::vector<std::string>::const_iterator i = addresses.begin(); i != addresses.end(); ++i)
    {
        m_items.push_back(Item(*i, true));
    }
    emit endResetModel();
}

//*****************************************************************************
//*****************************************************************************
void UsersModel::addAddress(const std::string & address, bool isNewMessage)
{
    emit beginResetModel();
    m_items.erase(std::remove(m_items.begin(), m_items.end(), address), m_items.end());
    m_items.insert(m_items.begin(), Item(address, !isNewMessage));
    emit endResetModel();
}

//*****************************************************************************
//*****************************************************************************
void UsersModel::deleteAddress(const std::string & address)
{
    emit beginResetModel();
    m_items.erase(std::remove(m_items.begin(), m_items.end(), address), m_items.end());
    emit endResetModel();
}

//*****************************************************************************
//*****************************************************************************
//const std::vector<Message> & UsersModel::plainData() const
//{
//    return m_messages;
//}

//*****************************************************************************
//*****************************************************************************
void UsersModel::onRead(const QModelIndex & index)
{
    if (!index.isValid())
    {
        return;
    }
    if (index.row() > m_items.size())
    {
        return;
    }
    m_items[index.row()].isRead = true;
    emit dataChanged(index, index);
}

//*****************************************************************************
//*****************************************************************************
void UsersModel::setAddressTableModel(AddressTableModel * model)
{
    m_addrTableModel = model;
}

//*****************************************************************************
//*****************************************************************************
QString UsersModel::labelForAddress(const QString & address) const
{
    static const QString emptyLabel = trUtf8("(no label)");

    if (!m_addrTableModel)
    {
        return emptyLabel;
    }

    QString label = m_addrTableModel->labelForAddress(address);
    return label.isEmpty() ? emptyLabel : label;
}

//*****************************************************************************
//*****************************************************************************
QString UsersModel::labelForAddressWithAddress(const QString & address) const
{
    if (!m_addrTableModel)
    {
        return address;
    }

    QString label = m_addrTableModel->labelForAddress(address);
    return label.isEmpty() ? address : label + '(' + address + ')';
}
