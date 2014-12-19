//*****************************************************************************
//*****************************************************************************

#include "messagedialog.h"
#include "ui_messagedialog.h"
#include "../walletmodel.h"
#include "../addresstablemodel.h"
#include "../addressbookpage.h"
#include "../editaddressdialog.h"
#include "../util/verify.h"
#include "../../key.h"
#include "../../base58.h"
#include "../../wallet.h"
#include "../../init.h"

#include <QDateTime>
#include <QMessageBox>
#include <QClipboard>
#include <QTimer>

#include <vector>

//*****************************************************************************
//*****************************************************************************
MessagesDialog::MessagesDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MessagesDialog)
    , m_db(ChatDb::instance())
    , m_keys(StoredPubKeysDb::instance())
    , m_model(0)
    , m_walletModel(0)
{
    ui->setupUi(this);

    ui->messages_SM->setModel(&m_model);
    ui->messages_SM->setItemDelegate(&m_messageDelegate);
    ui->messages_SM->setAutoScroll(true);

    ui->addresses_SM->setModel(&m_users);
    ui->addresses_SM->setItemDelegate(&m_userDelegate);

    ui->outMessage_SM->setFocus();

    std::vector<std::string> addrs;
    m_db.loadAddresses(addrs);
    m_users.loadAddresses(addrs);

    m_addToAddressBookAction = new QAction(trUtf8("Add to address book"), this);
    m_userContextMenu.addAction(m_addToAddressBookAction);
    m_deleteAction = new QAction(trUtf8("Delete"), this);
    m_userContextMenu.addAction(m_deleteAction);

    VERIFY(connect(ui->addresses_SM, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(addrContextMenu(QPoint))));
    VERIFY(connect(ui->outMessage_SM, SIGNAL(returnPressed()), this, SLOT(on_sendButton_SM_clicked())));
}

//*****************************************************************************
//*****************************************************************************
MessagesDialog::~MessagesDialog()
{
    delete ui;
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::setWalletModel(WalletModel * model)
{
    m_walletModel = model;
    m_users.setAddressTableModel(model->getAddressTableModel());

    // set from addr if empty
    if (ui->addressIn_SM->text().isEmpty())
    {
        AddressTableModel * atm = m_walletModel->getAddressTableModel();
        if (atm)
        {
            quint32 count = atm->rowCount(QModelIndex());
            for (quint32 i = 0; i < count; ++i)
            {
                QModelIndex idx = atm->index(i, AddressTableModel::Address, QModelIndex());
                if (atm->data(idx, AddressTableModel::TypeRole).toString() == AddressTableModel::Receive)
                {
                    setFromAddress(atm->data(idx, Qt::DisplayRole).toString());
                    break;
                }
            }
        }
    }

    QTimer::singleShot(5000, this, SLOT(requestUndeliveredMessages()));
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::setFromAddress(const QString & address)
{
    ui->addressIn_SM->setText(address);
    ui->outMessage_SM->setFocus();
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::setToAddress(const QString & address)
{
    setWindowTitle(QString("Messages <%1>").arg(m_users.labelForAddressWithAddress(address)));

    ui->pubKeyTo_SM->clear();

    // TODO disable for blocknet testing
    // CBitcoinAddress addr(address.toStdString());
    // if (addr.IsValid())
    {
        CPubKey pubKey;
        m_keys.load(address.toStdString(), pubKey);

        if (pubKey.IsValid())
        {
            ui->pubKeyTo_SM->setText(QString::fromStdString(EncodeBase58(pubKey.Raw())));
        }

//        CKeyID id;
//        if (addr.GetKeyID(id))
//        {
//            CKey key;
//            if (pwalletMain->GetKey(id, key))
//            {
//                CPubKey pubKey = key.GetPubKey();
//                if (pubKey.IsValid())
//                {
//                    std::vector<unsigned char> raw = pubKey.Raw();
//                    QByteArray arr(static_cast<char *>(static_cast<void *>(&raw[0])), raw.size());
//                    ui->pubKeyTo_SM->setText(QString(arr.toBase64()));
//                }
//            }
//        }
    }

    // load messages for address
    std::vector<Message> messages;
    loadMessages(address, messages);
    m_model.loadMessages(messages);

    // set remote address
    ui->addressTo_SM->setText(address);

    // set local address
    if (messages.size() > 0)
    {
        Message & m = messages.back();
        bool isIncoming = m.appliesToMe();

        ui->addressIn_SM->setText(QString::fromStdString(!isIncoming ? m.from : m.to));
    }

    ui->messages_SM->scrollToBottom();
    ui->outMessage_SM->setFocus();
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::showForAddress(const QString & address)
{
    m_readTimer.stop();

    setToAddress(address);
    // show();

    ui->messages_SM->scrollToBottom();

    m_readTimer.singleShot(3000, this, SLOT(onReadTimer()));
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::incomingMessage(Message & message)
{
    // before process this message try to send undelivered
    {
        std::vector<std::string> addrs;
        addrs.push_back(message.from);
        resendUndelivered(addrs);
    }

    // if message is empty - done
    if (message.isEmpty())
    {
        return;
    }

    CKey key;
    if (!getKeyForAddress(message.to, key))
    {
        QMessageBox::warning(this, "", QString("reseived message from <%1>,\nbut key for <%2> not found")
                             .arg(QString::fromStdString(message.from),
                                  QString::fromStdString(message.to)));
        return;
    }

    bool forMy = false;
    CPubKey senderPubKey;
    if (!message.decrypt(key, forMy, senderPubKey))
    {
        if (forMy == true)
        {
            QMessageBox::warning(this, "", QString("cannot encrypt message from <%1>")
                                 .arg(QString::fromStdString(message.from)));
        }
        return;
    }

    QString from = QString::fromStdString(message.from);

    std::vector<Message> messages;
    loadMessages(from, messages);
    messages.push_back(message);
    saveMessages(from, messages);

    // save sender pub key
    if (senderPubKey.IsValid())
    {
        m_keys.store(message.from, senderPubKey);
    }

    // add from address to model
    // or move to top
    m_users.addAddress(from.toStdString());

    if (ui->addresses_SM->currentIndex().data(UsersModel::roleAddress).toString() == from ||
        !ui->addresses_SM->currentIndex().isValid())
    {
        showForAddress(from);
    }

    emit newIncomingMessage(m_users.labelForAddress(from), QString::fromStdString(message.text));
}

//*****************************************************************************
//*****************************************************************************
bool MessagesDialog::loadMessages(const QString & address, std::vector<Message> & result)
{
    result.clear();

    std::vector<Message> messages;
    if (!m_db.load(address.toStdString(), messages))
    {
        // QMessageBox::warning(this, "", trUtf8("Error when load messages for <%1>").arg(m_users.labelForAddress(address)));
        // return;
    }

    BOOST_FOREACH(Message & msg, messages)
    {
        if (!msg.isExpired())
        {
            result.push_back(msg);
        }
    }

    if (messages.size() != result.size())
    {
        m_db.save(address.toStdString(), result);
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::saveMessages(const QString & address, const std::vector<Message> & messages)
{
    m_db.save(address.toStdString(), messages);
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::clearMessages(const QString & address)
{
    m_db.save(address.toStdString(), std::vector<Message>());
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::pushToUndelivered(const Message & m)
{
    UndeliveredMap messages;
    m_db.loadUndelivered(messages);

    // check expired messages
    for (UndeliveredMap::iterator i = messages.begin(); i != messages.end(); )
    {
        if (i->second.isExpired())
        {
            messages.erase(i++);
        }
        else
        {
            ++i;
        }
    }

    uint256 hash = m.getStaticHash();
    messages.insert(std::make_pair(hash, m));

    m_db.saveUndelivered(messages);
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::on_pasteButton_SM_clicked()
{
    setFromAddress(QApplication::clipboard()->text());
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::on_pasteButtonTo_SM_clicked()
{
    setToAddress(QApplication::clipboard()->text());
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::on_pasteButtonPublicKey_SM_clicked()
{
    ui->pubKeyTo_SM->setText(QApplication::clipboard()->text());
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::on_addressBookButton_SM_clicked()
{
    if (!m_walletModel)
    {
        // TODO
        // alert
        return;
    }

    AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::ReceivingTab, this);
    dlg.setModel(m_walletModel->getAddressTableModel());
    if (dlg.exec() != QDialog::Rejected)
    {
        setFromAddress(dlg.getReturnValue());
    }
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::on_addressBookButtonTo_SM_clicked()
{
    if (!m_walletModel)
    {
        // TODO
        // alert
        return;
    }

    AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::SendingTab, this);
    dlg.setModel(m_walletModel->getAddressTableModel());
    if (dlg.exec() != QDialog::Rejected)
    {
        setToAddress(dlg.getReturnValue());
    }
}

//*****************************************************************************
//*****************************************************************************
bool MessagesDialog::checkAddress(const std::string & address) const
{
    CBitcoinAddress addr(address);
    if (!addr.IsValid())
    {
        // TODO
        // alert
        return false;
    }
    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
    {
        // TODO
        // alert
        return false;
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
bool MessagesDialog::getKeyForAddress(const std::string & address, CKey & key) const
{
    if (!m_walletModel || !pwalletMain)
    {
        return false;
    }

    CBitcoinAddress addr(address);
    if (!addr.IsValid())
    {
        return false;
    }

    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
    {
        return false;
    }

    // check unlock wallet and unlock
    if (!pwalletMain->IsCrypted() || !pwalletMain->IsLocked())
    {
        // unlocked
        if (!pwalletMain->GetKey(keyID, key))
        {
            return false;
        }
    }
    else
    {
        // locked
        WalletModel::UnlockContext ctx = m_walletModel->requestUnlock();
        if (!ctx.isValid())
        {
            return false;
        }

        if (!pwalletMain->GetKey(keyID, key))
        {
            return false;
        }
    }

    return true;
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::on_sendButton_SM_clicked()
{
    Message m;

    // check empty message
    m.text = ui->outMessage_SM->text().toStdString();
    if (m.text.length() == 0)
    {
        QMessageBox::warning(this, "", "empty message");
        return;
    }

    if (m.text.size() > Message::maxMessageSize)
    {
        m.text.resize(Message::maxMessageSize);
    }

    // check source addr
    m.from = ui->addressIn_SM->text().toStdString();
    if (!checkAddress(m.from))
    {
        ui->addressIn_SM->setValid(false);
        QMessageBox::warning(this, "", "invalid address");
        return;
    }

    // check destination addr
    m.to = ui->addressTo_SM->text().toStdString();

    // TODO disable for blocknet testing
//    if (!checkAddress(m.to))
//    {
//        ui->addressTo_SM->setValid(false);
//        QMessageBox::warning(this, "", "invalid address");
//        return;
//    }

    CKey myKey;
    if (!getKeyForAddress(m.from, myKey))
    {
        QMessageBox::warning(this, "", "key not found");
        return;
    }

    if (!m.sign(myKey))
    {
        QMessageBox::warning(this, "", "sign error");
        return;
    }

    std::vector<unsigned char> vkey;
    if (!ui->pubKeyTo_SM->text().isEmpty())
    {
        DecodeBase58(ui->pubKeyTo_SM->text().toStdString(), vkey);
    }

    // check input key
    CKey destKey;
    CPubKey destPubKey;
    if (vkey.size() && destKey.SetPubKey(vkey))
    {
        destPubKey = destKey.GetPubKey();

        // key is correct - store it
        m_keys.store(m.to, destPubKey);
    }
    else
    {
        // not valid, check stored key
        if (!m_keys.load(m.to, destPubKey))
        {
            QMessageBox::warning(this, "", "invalid destination public key");
            return;
        }
    }

    m.date = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss").toStdString();

    // before send this message try to send undelivered
//    {
//        std::vector<std::string> addrs;
//        addrs.push_back(m.to);
//        resendUndelivered(addrs);
//    }

    // send message
    {
        Message mCopy = m;
        if (!mCopy.encrypt(destPubKey))
        {
            QMessageBox::warning(this, "", "message encryption failed");
            return;
        }
        mCopy.send();
        pushToUndelivered(mCopy);
    }

    m_model.addMessage(m);
    saveMessages(ui->addressTo_SM->text(), m_model.plainData());

    ui->outMessage_SM->clear();
    ui->messages_SM->scrollToBottom();

    m_users.addAddress(m.to, false);
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::on_clearButton_SM_clicked()
{
    QModelIndex idx = ui->addresses_SM->currentIndex();
    if (!idx.isValid())
    {
        return;
    }

    if (QMessageBox::warning(this, "", trUtf8("Clear all messages?"), QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
    {
        return;
    }

    QString addr = idx.data(UsersModel::roleAddress).toString();

    clearMessages(addr);
    m_model.loadMessages(std::vector<Message>());
}

//*****************************************************************************
//*****************************************************************************
std::vector<std::string> MessagesDialog::getLocalAddresses() const
{
    std::vector<std::string> result;

    if (!m_walletModel)
    {
        return result;
    }

    AddressTableModel * addressesModel = m_walletModel->getAddressTableModel();
    int rows = addressesModel->rowCount(QModelIndex());

    for (int i = 0; i < rows; ++i)
    {
        QModelIndex idx = addressesModel->index(i, AddressTableModel::Address, QModelIndex());
        QString type = idx.data(AddressTableModel::TypeRole).toString();
        if (type != AddressTableModel::Receive)
        {
            continue;
        }

        result.push_back(idx.data(Qt::DisplayRole).toString().toStdString());
    }

    return result;
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::requestUndeliveredMessages()
{
    static quint64 requestCounter = 0;

    std::vector<std::string> addrs = getLocalAddresses();
    if (!addrs.size())
    {
        return;
    }

    // send empty message for all addresses
    BOOST_FOREACH(std::string addr, addrs)
    {
        Message m;
        m.from = addr;
        m.date = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss").toStdString();
        m.send();
    }

    QTimer::singleShot(60000 * (++requestCounter > 8 ? 2 : 10 ), this, SLOT(requestUndeliveredMessages()));
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::on_addresses_SM_clicked(const QModelIndex & index)
{
    if (!index.isValid())
    {
        return;
    }

    QString addr = index.data(UsersModel::roleAddress).toString();
    showForAddress(addr);
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::onReadTimer()
{
    if (ui->addresses_SM->currentIndex().isValid())
    {
        m_users.onRead(ui->addresses_SM->currentIndex());
    }
    else if (m_users.rowCount() > 0)
    {
        m_users.onRead(m_users.index(0));
    }
}

//*****************************************************************************
//*****************************************************************************
void MessagesDialog::addrContextMenu(QPoint point)
{
    QModelIndex index = ui->addresses_SM->indexAt(point);
    if (!index.isValid())
    {
        return;
    }

    AddressTableModel * atm = m_walletModel->getAddressTableModel();
    if (!atm)
    {
        return;
    }

    QString addr  = index.data(UsersModel::roleAddress).toString();
    QString label = atm->labelForAddress(addr);

    m_addToAddressBookAction->setVisible(label.isEmpty());

    QAction * a = m_userContextMenu.exec(QCursor::pos());

    if (a == m_addToAddressBookAction)
    {
        EditAddressDialog dlg(EditAddressDialog::NewSendingAddress);
        dlg.setModel(atm);
        dlg.setAddress(addr);
        if (dlg.exec())
        {
            m_users.addAddress(addr.toStdString(), false);
        }
    }
    else if (a == m_deleteAction)
    {
        int result = QMessageBox::information(this,
                                              trUtf8("Confirm delete"),
                                              trUtf8("Are you sure?"),
                                              QMessageBox::Yes | QMessageBox::Cancel);
        if (result == QMessageBox::Yes)
        {
            m_users.deleteAddress(addr.toStdString());
            m_db.erase(addr.toStdString());

            m_model.clear();
        }
    }
}

//*****************************************************************************
//*****************************************************************************
// static
bool MessagesDialog::resendUndelivered(const std::vector<std::string> & addresses)
{
    ChatDb & db = ChatDb::instance();

    UndeliveredMap map;
    if (!db.loadUndelivered(map))
    {
        return false;
    }

    bool needToSaveUndelivered = false;
    BOOST_FOREACH(const std::string addr, addresses)
    {
        for (UndeliveredMap::iterator i = map.begin(); i != map.end(); )
        {
            if (i->second.isExpired())
            {
                // expired, delete
                map.erase(i++);
            }
            else
            {
                if (i->second.to == addr)
                {
                    // update timestamp and resend message
                    Message mcopy = i->second;
                    // mcopy.date = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss").toStdString();
                    mcopy.timestamp = std::time(0);
                    mcopy.send();
                }
                ++i;
            }
        }
    }

    if (needToSaveUndelivered)
    {
        db.saveUndelivered(map);
    }

    return true;
}
