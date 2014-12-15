//*****************************************************************************
//*****************************************************************************

#ifndef MESSAGEDIALOG_H
#define MESSAGEDIALOG_H

#include "messagesmodel.h"
#include "messagedelegate.h"
#include "usersmodel.h"
#include "userdelegate.h"
#include "../../messagedb.h"

#include <QDialog>
#include <QTimer>
#include <QMenu>

#include <string>

//*****************************************************************************
//*****************************************************************************
namespace Ui {
class MessagesDialog;
}

class WalletModel;

//*****************************************************************************
//*****************************************************************************
class MessagesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MessagesDialog(QWidget *parent = 0);
    ~MessagesDialog();

public:
    void setWalletModel(WalletModel * model);

    void setFromAddress(const QString & address);
    void setToAddress(const QString & address);

signals:
    void newIncomingMessage(const QString & title, const QString & text);

public slots:
    void showForAddress(const QString & address);
    void incomingMessage(Message & message);

private slots:
    void on_pasteButton_SM_clicked();
    void on_pasteButtonTo_SM_clicked();
    void on_pasteButtonPublicKey_SM_clicked();
    void on_addressBookButton_SM_clicked();
    void on_addressBookButtonTo_SM_clicked();
    void on_sendButton_SM_clicked();
    void on_clearButton_SM_clicked();
    void on_addresses_SM_clicked(const QModelIndex &index);

    void requestUndeliveredMessages();

    void onReadTimer();

    void addrContextMenu(QPoint point);

private:
    std::vector<std::string> getLocalAddresses() const;

    bool loadMessages(const QString & address, std::vector<Message> & result);
    void saveMessages(const QString & address, const std::vector<Message> & messages);
    void clearMessages(const QString & address);
    void pushToUndelivered(const Message & m);

    bool checkAddress(const std::string & address) const;
    bool getKeyForAddress(const std::string & address, CKey & key) const;
    bool signMessage(CKey & key, const std::string & message,
                     std::vector<unsigned char> & signature) const;

    bool resendUndelivered(const std::vector<std::string> & addresses);

private:
    Ui::MessagesDialog * ui;

    ChatDb             & m_db;
    StoredPubKeysDb    & m_keys;

    MessagesModel        m_model;
    MessageDelegate      m_messageDelegate;

    UsersModel           m_users;
    UserDelegate         m_userDelegate;

    WalletModel        * m_walletModel;

    QTimer               m_readTimer;

    QMenu                m_userContextMenu;
    QAction            * m_addToAddressBookAction;
    QAction            * m_deleteAction;
};

#endif // MESSAGEDIALOG_H
