//*****************************************************************************
//*****************************************************************************

#ifndef MESSAGESMODEL_H
#define MESSAGESMODEL_H

#include "message.h"
#include "message_metatype.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>

#include <vector>


//*****************************************************************************
//*****************************************************************************
class MessagesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum
    {
        roleMessage = Qt::UserRole,
        roleIncoming,
        roleDateTime,
        roleDateTimeString
    };

public:
    explicit MessagesModel(QObject *parent = 0);

public:
    virtual int rowCount(const QModelIndex & parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void loadMessages(const std::vector<Message> & messages);
    void addMessage(const Message & message);
    void clear();

    const std::vector<Message> & plainData() const;

signals:

public slots:

private:
    std::vector<Message> m_messages;
};

#endif // MESSAGESMODEL_H
