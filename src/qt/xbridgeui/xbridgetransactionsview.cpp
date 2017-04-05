//******************************************************************************
//******************************************************************************

#include "xbridgetransactionsview.h"
// #include "../xbridgeapp.h"
// #include "xbridgetransactiondialog.h"
#include "util/verify.h"
#include "xbridge/xbridgeexchange.h"
#include "xbridge/xuiconnector.h"
#include "xbridge/util/logger.h"

#include <QTableView>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QLabel>
#include <QTextEdit>

//******************************************************************************
//******************************************************************************
XBridgeTransactionsView::XBridgeTransactionsView(QWidget *parent)
    : QWidget(parent)
    // , m_walletModel(0)
    , m_dlg(m_txModel, this)
{
    setupUi();
}

//******************************************************************************
//******************************************************************************
XBridgeTransactionsView::~XBridgeTransactionsView()
{
    xuiConnector.NotifyLogMessage.disconnect
            (boost::bind(&XBridgeTransactionsView::onLogString, this, _1));
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::setupUi()
{
    QVBoxLayout * vbox = new QVBoxLayout;

    QLabel * l = new QLabel(tr("Blocknet Decentralized Exchange"), this);
    vbox->addWidget(l);

    m_proxy.setSourceModel(&m_txModel);
    m_proxy.setDynamicSortFilter(true);

    m_transactionsList = new QTableView(this);
    m_transactionsList->setModel(&m_proxy);
    m_transactionsList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_transactionsList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_transactionsList->setSortingEnabled(true);

    VERIFY(connect(m_transactionsList, SIGNAL(customContextMenuRequested(QPoint)),
                   this,               SLOT(onContextMenu(QPoint))));


    QHeaderView * header = m_transactionsList->horizontalHeader();
#if QT_VERSION <0x050000
    header->setResizeMode(XBridgeTransactionsModel::AddressFrom, QHeaderView::Stretch);
#else
    header->setSectionResizeMode(XBridgeTransactionsModel::AddressFrom, QHeaderView::Stretch);
#endif
    header->resizeSection(XBridgeTransactionsModel::AmountFrom,  80);
#if QT_VERSION <0x050000
    header->setResizeMode(XBridgeTransactionsModel::AddressTo, QHeaderView::Stretch);
#else
    header->setSectionResizeMode(XBridgeTransactionsModel::AddressTo, QHeaderView::Stretch);
#endif
    header->resizeSection(XBridgeTransactionsModel::AmountTo,    80);
    header->resizeSection(XBridgeTransactionsModel::State,       128);
    header->resizeSection(XBridgeTransactionsModel::Tax,         64);
    vbox->addWidget(m_transactionsList);

    QHBoxLayout * hbox = new QHBoxLayout;

    XBridgeExchange & e = XBridgeExchange::instance();
    if (!e.isEnabled())
    {
        QPushButton * addTxBtn = new QPushButton(trUtf8("New Transaction"), this);
        // addTxBtn->setIcon(QIcon("qrc://"))
        VERIFY(connect(addTxBtn, SIGNAL(clicked()), this, SLOT(onNewTransaction())));
        hbox->addWidget(addTxBtn);
    }
    else
    {
        QPushButton * addTxBtn = new QPushButton(trUtf8("Exchange node"), this);
        addTxBtn->setEnabled(false);
        hbox->addWidget(addTxBtn);
    }

    hbox->addStretch();

    QPushButton * showHideButton = new QPushButton(">>", this);
    VERIFY(connect(showHideButton, SIGNAL(clicked()), this, SLOT(onShowLogs())));
    hbox->addWidget(showHideButton);

    vbox->addLayout(hbox);

    m_logStrings = new QTextEdit(this);
    m_logStrings->setMinimumHeight(64);
    m_logStrings->setReadOnly(true);
    m_logStrings->setVisible(false);
    vbox->addWidget(m_logStrings);

    setLayout(vbox);

    xuiConnector.NotifyLogMessage.connect
            (boost::bind(&XBridgeTransactionsView::onLogString, this, _1));
}

//******************************************************************************
//******************************************************************************
QMenu * XBridgeTransactionsView::setupContextMenu(QModelIndex & index)
{
    QMenu * contextMenu = new QMenu();

    if (!m_txModel.isMyTransaction(index.row()))
    {
        QAction * acceptTransaction = new QAction(tr("&Accept transaction"), this);
        contextMenu->addAction(acceptTransaction);

        VERIFY(connect(acceptTransaction,   SIGNAL(triggered()),
                       this,                SLOT(onAcceptTransaction())));
    }
    else
    {
        QAction * cancelTransaction = new QAction(tr("&Cancel transaction"), this);
        contextMenu->addAction(cancelTransaction);

        VERIFY(connect(cancelTransaction,   SIGNAL(triggered()),
                       this,                SLOT(onCancelTransaction())));
    }

    if (false)
    {
        QAction * rollbackTransaction = new QAction(tr("&Rollback transaction"), this);
        contextMenu->addAction(rollbackTransaction);

        VERIFY(connect(rollbackTransaction, SIGNAL(triggered()),
                       this,                SLOT(onRollbackTransaction())));
    }

    return contextMenu;
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onNewTransaction()
{
    m_dlg.setPendingId(uint256(), std::vector<unsigned char>());
    m_dlg.show();
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onAcceptTransaction()
{
    if (!m_contextMenuIndex.isValid())
    {
        return;
    }

    if (m_txModel.isMyTransaction(m_contextMenuIndex.row()))
    {
        return;
    }

    XBridgeTransactionDescr d = m_txModel.item(m_contextMenuIndex.row());
    if (d.state != XBridgeTransactionDescr::trPending)
    {
        return;
    }

    m_dlg.setPendingId(d.id, d.hubAddress);
    m_dlg.setFromAmount((double)d.toAmount / XBridgeTransactionDescr::COIN);
    m_dlg.setToAmount((double)d.fromAmount / XBridgeTransactionDescr::COIN);
    m_dlg.setFromCurrency(QString::fromStdString(d.toCurrency));
    m_dlg.setToCurrency(QString::fromStdString(d.fromCurrency));
    m_dlg.show();
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onCancelTransaction()
{
    if (!m_contextMenuIndex.isValid())
    {
        return;
    }

    if (QMessageBox::warning(this,
                             trUtf8("Cancel transaction"),
                             trUtf8("Are you sure?"),
                             QMessageBox::Yes | QMessageBox::Cancel,
                             QMessageBox::Cancel) != QMessageBox::Yes)
    {
        return;
    }

    if (!m_txModel.cancelTransaction(m_txModel.item(m_contextMenuIndex.row()).id))
    {
        QMessageBox::warning(this,
                             trUtf8("Cancel transaction"),
                             trUtf8("Error send cancel request"));
    }
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onRollbackTransaction()
{

}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onContextMenu(QPoint /*pt*/)
{
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        return;
    }

    m_contextMenuIndex = m_transactionsList->selectionModel()->currentIndex();
    if (!m_contextMenuIndex.isValid())
    {
        return;
    }

    m_contextMenuIndex = m_proxy.mapToSource(m_contextMenuIndex);
    if (!m_contextMenuIndex.isValid())
    {
        return;
    }

    QMenu * contextMenu = setupContextMenu(m_contextMenuIndex);

    contextMenu->exec(QCursor::pos());
    contextMenu->deleteLater();
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onShowLogs()
{
    QPushButton * btn = qobject_cast<QPushButton *>(sender());

    bool visible = m_logStrings->isVisible();
    btn->setText(visible ? ">>" : "<<");

    m_logStrings->setVisible(!visible);

    if (!visible)
    {
        m_logStrings->clear();

        // show, load all logs
        QFile f(QString::fromStdString(LOG::logFileName()));
        if (f.open(QIODevice::ReadOnly))
        {
            m_logStrings->insertPlainText(f.readAll());
        }
    }
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionsView::onLogString(const std::string str)
{
    const QString qstr = QString::fromStdString(str);
    m_logStrings->insertPlainText(qstr);

//    QTextCursor c = m_logStrings->textCursor();
//    c.movePosition(QTextCursor::End);
//    m_logStrings->setTextCursor(c);

//    m_logStrings->ensureCursorVisible();
}
