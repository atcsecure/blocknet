//*****************************************************************************
//*****************************************************************************

#include "messagesmodel.h"

#include <QDebug>

//*****************************************************************************
//*****************************************************************************
MessagesModel::MessagesModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

//*****************************************************************************
//*****************************************************************************
// virtual
int MessagesModel::rowCount(const QModelIndex & parent) const
{
    Q_UNUSED(parent)
    return m_messages.size();
}

//*****************************************************************************
//*****************************************************************************
// virtual
QVariant MessagesModel::data(const QModelIndex &index, int role) const
{
    quint32 row = static_cast<quint32>(index.row());
    if (row >= m_messages.size())
    {
        return QVariant();
    }

    const Message & m = m_messages[row];
    if (role == Qt::DisplayRole)
    {
        return QString::fromStdString(m.text);
    }
    else if (role == roleMessage)
    {
        return QVariant::fromValue(m);
    }
    else if (role == roleIncoming)
    {
        return m.appliesToMe();
    }
    else if (role == roleDateTime)
    {
        return QDateTime::fromTime_t(m.getTime());
    }
    else if (role == roleDateTimeString)
    {
        return QDateTime::fromTime_t(m.getTime()).toString("yyyy-MM-dd hh:mm:ss");
    }

    return QVariant();
}

//*****************************************************************************
//*****************************************************************************
void MessagesModel::loadMessages(const std::vector<Message> & messages)
{
    emit beginResetModel();
    m_messages = messages;
    std::sort(m_messages.begin(), m_messages.end());
    emit endResetModel();
}

//*****************************************************************************
//*****************************************************************************
void MessagesModel::addMessage(const Message & message)
{
    size_t size = m_messages.size();
    beginInsertRows(QModelIndex(), size, size+1);
    m_messages.push_back(message);
    endInsertRows();
}

//*****************************************************************************
//*****************************************************************************
void MessagesModel::clear()
{
    emit beginResetModel();
    m_messages.clear();
    emit endResetModel();
}

//*****************************************************************************
//*****************************************************************************
const std::vector<Message> & MessagesModel::plainData() const
{
    return m_messages;
}
