//******************************************************************************
//******************************************************************************

#ifndef XBRIDGETRANSACTIONSVIEW_H
#define XBRIDGETRANSACTIONSVIEW_H

#include "xbridgetransactionsmodel.h"
#include "xbridgetransactiondialog.h"

// #include "../walletmodel.h"

#include <QWidget>
#include <QMenu>
#include <QSortFilterProxyModel>

class QTableView;
class QTextEdit;

//******************************************************************************
//******************************************************************************
class XBridgeTransactionsView : public QWidget
{
    Q_OBJECT
public:
    explicit XBridgeTransactionsView(QWidget *parent = 0);
    ~XBridgeTransactionsView();

signals:

public slots:

private:
    void setupUi();
    QMenu * setupContextMenu(QModelIndex & index);

private slots:
    void onNewTransaction();
    void onAcceptTransaction();
    void onCancelTransaction();
    void onRollbackTransaction();

    void onContextMenu(QPoint pt);

    void onShowLogs();
    void onLogString(const std::string str);

private:
    // WalletModel            * m_walletModel;

    XBridgeTransactionsModel m_txModel;
    QSortFilterProxyModel    m_proxy;

    XBridgeTransactionDialog m_dlg;

    QTableView  * m_transactionsList;

    QModelIndex   m_contextMenuIndex;

    QTextEdit   * m_logStrings;
};

#endif // XBRIDGETRANSACTIONSVIEW_H
