/*
 * Qt5 bitcoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2012
 */
#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "transactiondescdialog.h"
#include "addresstablemodel.h"
#include "transactionview.h"
#include "overviewpage.h"
#include "statisticspage.h"
#include "blockbrowser.h"
//#include "tradepage.h"
#include "bitcoinunits.h"
#include "guiconstants.h"
#include "askpassphrasedialog.h"
#include "notificator.h"
#include "guiutil.h"
#include "rpcconsole.h"
#include "wallet.h"
#include "bitcoinrpc.h"
#include "messagedialog/messagedialog.h"
#include "xbridgeui/xbridgetransactionsview.h"
#include "xbridge/xbridgeexchange.h"
#include "util/verify.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QLocale>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QStackedWidget>
#include <QDateTime>
#include <QMovie>
#include <QFileDialog>
#include <QDesktopServices>
#include <QTimer>
#include <QDragEnterEvent>
#include <QUrl>
#include <QStyle>
#include <QMessageBox>

#include <iostream>

//*****************************************************************************
//*****************************************************************************
extern CWallet* pwalletMain;
extern int64_t nLastCoinStakeSearchInterval;
extern unsigned int nTargetSpacing;
double GetPoSKernelPS();

//*****************************************************************************
//*****************************************************************************
BitcoinGUI::BitcoinGUI(QWidget *parent):
    QMainWindow(parent),
    clientModel(0),
    walletModel(0),
    encryptWalletAction(0),
    changePassphraseAction(0),
    unlockWalletAction(0),
    lockWalletAction(0),
    aboutQtAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0)
{
    setFixedSize(970, 550);

    QString title = tr("blocknet") + " " + tr("Wallet");
    XBridgeExchange & e = XBridgeExchange::instance();
    if (e.isEnabled())
    {
        title += QString(" [%1] ").arg(tr("exchange mode"));
    }
    setWindowTitle(title);

    qApp->setStyleSheet("QMainWindow { background-image:url(:images/bkg);border:none;font-family:'Open Sans,sans-serif'; } #frame { } QToolBar QLabel { padding-top:15px;padding-bottom:10px;margin:0px; } #spacer { background:rgb(16,19,65);border:none; } #toolbar3 { border:none;width:1px; background-color: rgb(56,56,56); } #toolbar2 { border:none;width:28px; background-color:rgb(56,56,56); } #toolbar { border:none;height:100%;padding-top:20px; background: rgb(16,19,65); text-align: left; color: white;min-width:150px;max-width:150px;} QToolBar QToolButton:hover {background-color:qlineargradient(x1: 0, y1: 0, x2: 2, y2: 2,stop: 0 rgb(16,19,65), stop: 1 rgb(216,252,251),stop: 2 rgb(59,62,65));} QToolBar QToolButton { font-family:Century Gothic;padding-left:20px;padding-right:150px;padding-top:10px;padding-bottom:10px; width:100%; color: white; text-align: left; background-color: rgb(16,19,65) } #labelMiningIcon { padding-left:5px;font-family:Century Gothic;width:100%;font-size:10px;text-align:center;color:white; } QMenu { background: rgb(16,19,65); color:white; padding-bottom:10px; } QMenu::item { color:white; background-color: transparent; } QMenu::item:selected { background-color:qlineargradient(x1: 0, y1: 0, x2: 0.5, y2: 0.5,stop: 0 rgb(16,19,65), stop: 1 rgb(149,204,244)); } QMenuBar { background: rgb(16,19,65); color:white; } QMenuBar::item { font-size:12px;padding-bottom:8px;padding-top:8px;padding-left:15px;padding-right:15px;color:white; background-color: transparent; } QMenuBar::item:selected { background-color:qlineargradient(x1: 0, y1: 0, x2: 0.5, y2: 0.5,stop: 0 rgb(16,19,65), stop: 1 rgb(149,204,244)); }");
#ifndef Q_OS_MAC
    qApp->setWindowIcon(QIcon(":icons/bitcoin"));
    setWindowIcon(QIcon(":icons/bitcoin"));
#else
    setUnifiedTitleAndToolBarOnMac(true);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create the tray icon (or setup the dock icon)
    createTrayIcon();

    // Create tabs
    overviewPage = new OverviewPage();
    statisticsPage = new StatisticsPage(this);
//    tradePage = new TradePage(this);
	blockBrowser = new BlockBrowser(this);

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    transactionView = new TransactionView(this);
    vbox->addWidget(transactionView);
    transactionsPage->setLayout(vbox);

    addressBookPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab);

    receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab);

    sendCoinsPage = new SendCoinsDialog(this);

    signVerifyMessageDialog = new SignVerifyMessageDialog(this);

    // create messages dialog
    messagesPage = new MessagesDialog(this);

    // xbridge page
    xbridgePage = new XBridgeTransactionsView(this);

    centralWidget = new QStackedWidget(this);
    centralWidget->addWidget(overviewPage);
    centralWidget->addWidget(statisticsPage);
//    centralWidget->addWidget(tradePage);
	centralWidget->addWidget(blockBrowser);
	centralWidget->addWidget(transactionsPage);
    centralWidget->addWidget(addressBookPage);
    centralWidget->addWidget(receiveCoinsPage);
    centralWidget->addWidget(sendCoinsPage);
    centralWidget->addWidget(messagesPage);
    centralWidget->addWidget(xbridgePage);
    setCentralWidget(centralWidget);


    // Status bar notification icons

    labelEncryptionIcon = new QLabel();
    labelStakingIcon = new QLabel();
    labelConnectionsIcon = new QLabel();
    labelBlocksIcon = new QLabel();
    //actionConvertIcon = new QAction(QIcon(":/icons/statistics"), tr(""), this);

    if (GetBoolArg("-staking", true))
    {
        QTimer *timerStakingIcon = new QTimer(labelStakingIcon);
        connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(updateStakingIcon()));
        timerStakingIcon->start(30 * 1000);
        updateStakingIcon();
    }

	// Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new QProgressBar();
    addToolBarBreak(Qt::LeftToolBarArea);
    QToolBar *toolbar2 = addToolBar(tr("Tabs toolbar"));
    addToolBar(Qt::LeftToolBarArea,toolbar2);
    toolbar2->setOrientation(Qt::Vertical);
    toolbar2->setMovable( false );
    toolbar2->setObjectName("toolbar2");
    toolbar2->setFixedWidth(28);
    toolbar2->setIconSize(QSize(28,28));
    //toolbar2->addAction(actionConvertIcon);
    toolbar2->addWidget(labelEncryptionIcon);
    toolbar2->addWidget(labelStakingIcon);
    toolbar2->addWidget(labelConnectionsIcon);
    toolbar2->addWidget(labelBlocksIcon);
    toolbar2->setStyleSheet("#toolbar2 QToolButton { border:none;padding:0px;margin:0px;height:20px;width:28px;margin-top:36px; }");
	
	addToolBarBreak(Qt::TopToolBarArea);
    QToolBar *toolbar3 = addToolBar(tr("Green bar"));
    addToolBar(Qt::TopToolBarArea,toolbar3);
    toolbar3->setOrientation(Qt::Horizontal);
    toolbar3->setMovable( false );
    toolbar3->setObjectName("toolbar3");
    toolbar3->setFixedHeight(2);

    syncIconMovie = new QMovie(":/movies/update_spinner", "gif", this);
	
	
    // Clicking on a transaction on the overview page simply sends you to transaction history page
    VERIFY(connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage())));
    VERIFY(connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex))));

    // Double-clicking on a transaction on the transaction history page shows details
    VERIFY(connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails())));

    rpcConsole = new RPCConsole(this);
    VERIFY(connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show())));

    // Clicking on "Verify Message" in the address book sends you to the verify message tab
    VERIFY(connect(addressBookPage, SIGNAL(verifyMessage(QString)), this, SLOT(gotoVerifyMessageTab(QString))));
    // click on "messages" in the address book
    VERIFY(connect(addressBookPage, SIGNAL(showMessages(const QString)), this, SLOT(gotoMessagesPage(const QString))));

    // Clicking on "Sign Message" in the receive coins page sends you to the sign message tab
    VERIFY(connect(receiveCoinsPage, SIGNAL(signMessage(QString)), this, SLOT(gotoSignMessageTab(QString))));

    // incoming message notifications
    VERIFY(connect(messagesPage, SIGNAL(newIncomingMessage(QString,QString)), this, SLOT(incomingMessageNotify(QString,QString))));
    VERIFY(connect(&messageNotifyTimer, SIGNAL(timeout()), this, SLOT(onMessageNotifyTimer())));

    gotoOverviewPage();
}

//*****************************************************************************
//*****************************************************************************
BitcoinGUI::~BitcoinGUI()
{
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
#endif
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setToolTip(tr("Show general overview of wallet"));
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    statisticsAction = new QAction(QIcon(":/icons/statistics"), tr("&Statistics"), this);
    statisticsAction->setToolTip(tr("View statistics"));
    statisticsAction->setCheckable(true);
    tabGroup->addAction(statisticsAction);

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send coins"), this);
    sendCoinsAction->setToolTip(tr("Send coins to a blocknet address"));
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receive coins"), this);
    receiveCoinsAction->setToolTip(tr("Show the list of addresses for receiving payments"));
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setToolTip(tr("Browse transaction history"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

    addressBookAction = new QAction(QIcon(":/icons/address-book"), tr("&Address Book"), this);
    addressBookAction->setToolTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(addressBookAction);
	
//	tradeAction = new QAction(QIcon(":/icons/ex"), tr("&Exchanges"), this);
//    tradeAction->setToolTip(tr("Bittrex & Cryptsy"));
//    tradeAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
//    tradeAction->setCheckable(true);
//    tabGroup->addAction(tradeAction);
	
	blockAction = new QAction(QIcon(":/icons/block"), tr("&Block Explorer"), this);
    blockAction->setToolTip(tr("Explore the BlockChain"));
    blockAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
    blockAction->setCheckable(true);
    tabGroup->addAction(blockAction);

    // TODO icons
    messagesAction = new QAction(QIcon(":/icons/block"), tr("&Messages"), this);
    messagesAction->setToolTip(tr("Show chat"));
    // messagesAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
    messagesAction->setCheckable(true);
    tabGroup->addAction(messagesAction);

    // TODO icons
    xbridgeAction = new QAction(QIcon(":/icons/block"), tr("&XBridge"), this);
    xbridgeAction->setToolTip(tr("Show xbridge dialog"));
    // xbridgeAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
    xbridgeAction->setCheckable(true);
    tabGroup->addAction(xbridgeAction);

	connect(blockAction, SIGNAL(triggered()), this, SLOT(gotoBlockBrowser()));
    VERIFY(connect(overviewAction,     SIGNAL(triggered()), this, SLOT(showNormalIfMinimized())));
    VERIFY(connect(overviewAction,     SIGNAL(triggered()), this, SLOT(gotoOverviewPage())));
	connect(statisticsAction, SIGNAL(triggered()), this, SLOT(gotoStatisticsPage()));
//    connect(tradeAction, SIGNAL(triggered()), this, SLOT(gotoTradePage()));
    VERIFY(connect(sendCoinsAction,    SIGNAL(triggered()), this, SLOT(showNormalIfMinimized())));
    VERIFY(connect(sendCoinsAction,    SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage())));
    VERIFY(connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized())));
    VERIFY(connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage())));
    VERIFY(connect(historyAction,      SIGNAL(triggered()), this, SLOT(showNormalIfMinimized())));
    VERIFY(connect(historyAction,      SIGNAL(triggered()), this, SLOT(gotoHistoryPage())));
    VERIFY(connect(addressBookAction,  SIGNAL(triggered()), this, SLOT(showNormalIfMinimized())));
    VERIFY(connect(addressBookAction,  SIGNAL(triggered()), this, SLOT(gotoAddressBookPage())));
    VERIFY(connect(messagesAction,     SIGNAL(triggered()), this, SLOT(showNormalIfMinimized())));
    VERIFY(connect(messagesAction,     SIGNAL(triggered()), this, SLOT(gotoMessagesPage())));
    VERIFY(connect(xbridgeAction,      SIGNAL(triggered()), this, SLOT(gotoXBridgePage())));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setToolTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(QIcon(":/icons/bitcoin"), tr("&About blocknet"), this);
    aboutAction->setToolTip(tr("Show information about blocknet"));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutQtAction = new QAction(QIcon(":/icons/qtlogo"), tr("About &Qt"), this);
    aboutQtAction->setToolTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);

    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setToolTip(tr("Modify configuration options for blocknet"));
    optionsAction->setMenuRole(QAction::PreferencesRole);

    toggleHideAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Show / Hide"), this);

    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setToolTip(tr("Encrypt or decrypt wallet"));
    encryptWalletAction->setCheckable(true);

    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setToolTip(tr("Backup wallet to another location"));

    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setToolTip(tr("Change the passphrase used for wallet encryption"));
    unlockWalletAction = new QAction(QIcon(":/icons/mint_open"), tr("&Unlock"), this);
    unlockWalletAction->setToolTip(tr("Unlock Mining"));
    lockWalletAction = new QAction(QIcon(":/icons/mint_closed"), tr("&Lock"), this);
    lockWalletAction->setToolTip(tr("Lock Mining"));
    signMessageAction = new QAction(QIcon(":/icons/edit"), tr("Sign &message..."), this);

    verifyMessageAction = new QAction(QIcon(":/icons/transaction_0"), tr("&Verify message..."), this);

    exportAction = new QAction(QIcon(":/icons/export"), tr("&Export..."), this);
    exportAction->setToolTip(tr("Export the data in the current tab to a file"));

    openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setToolTip(tr("Open debugging and diagnostic console"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), this, SLOT(encryptWallet(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(unlockWalletAction, SIGNAL(triggered()), this, SLOT(unlockWallet()));
    connect(lockWalletAction, SIGNAL(triggered()), this, SLOT(lockWallet()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    file->addAction(backupWalletAction);
    file->addAction(exportAction);
    file->addAction(signMessageAction);
    file->addAction(verifyMessageAction);
    file->addSeparator();
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    settings->addAction(encryptWalletAction);
    settings->addAction(changePassphraseAction);
//    settings->addAction(unlockWalletAction);
//    settings->addAction(lockWalletAction);
    settings->addSeparator();
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::createToolBars()
{
    QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
    toolbar->setObjectName("toolbar");
    addToolBar(Qt::LeftToolBarArea,toolbar);
    toolbar->setOrientation(Qt::Vertical);
    toolbar->setMovable( false );
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setIconSize(QSize(50,25));
    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(addressBookAction);
//	toolbar->addAction(tradeAction);
	toolbar->addAction(statisticsAction);
	toolbar->addAction(blockAction);
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolbar->addWidget(spacer);
    spacer->setObjectName("spacer");
	toolbar->addAction(unlockWalletAction);
	toolbar->addAction(lockWalletAction);
    toolbar->addAction(messagesAction);
    toolbar->addAction(xbridgeAction);


}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if(clientModel)
    {
        // Replace some strings and icons, when using the testnet
        if(clientModel->isTestNet())
        {
            setWindowTitle(windowTitle() + QString(" ") + tr("[testnet]"));
#ifndef Q_OS_MAC
            qApp->setWindowIcon(QIcon(":icons/bitcoin_testnet"));
            setWindowIcon(QIcon(":icons/bitcoin_testnet"));
#else
            MacDockIconHandler::instance()->setIcon(QIcon(":icons/bitcoin_testnet"));
#endif
            if(trayIcon)
            {
                trayIcon->setToolTip(tr("blocknet client") + QString(" ") + tr("[testnet]"));
                trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
                toggleHideAction->setIcon(QIcon(":/icons/toolbar_testnet"));
            }

            aboutAction->setIcon(QIcon(":/icons/toolbar_testnet"));
        }

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        VERIFY(connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int))));

        setNumBlocks(clientModel->getNumBlocks(), clientModel->getNumBlocksOfPeers());
        VERIFY(connect(clientModel, SIGNAL(numBlocksChanged(int,int)), this, SLOT(setNumBlocks(int,int))));

        // Report errors from network/worker thread
        VERIFY(connect(clientModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool))));

        //incoming messages
        VERIFY(connect(clientModel, SIGNAL(newMessage(QVariant)), this, SLOT(incomingMessage(QVariant))));

        rpcConsole->setClientModel(clientModel);
        addressBookPage->setOptionsModel(clientModel->getOptionsModel());
        receiveCoinsPage->setOptionsModel(clientModel->getOptionsModel());
    }
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
    if(walletModel)
    {
        // Report errors from wallet thread
        connect(walletModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        // Put transaction list in tabs
        transactionView->setModel(walletModel);

        overviewPage->setModel(walletModel);
        addressBookPage->setModel(walletModel->getAddressTableModel());
        receiveCoinsPage->setModel(walletModel->getAddressTableModel());
        sendCoinsPage->setModel(walletModel);
        signVerifyMessageDialog->setModel(walletModel);
        messagesPage->setWalletModel(walletModel);

        statisticsPage->setModel(clientModel);
//        tradePage->setModel(clientModel);
		blockBrowser->setModel(clientModel);
        setEncryptionStatus(walletModel->getEncryptionStatus());
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(setEncryptionStatus(int)));

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(incomingTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));
    }
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::createTrayIcon()
{
    QMenu *trayIconMenu;
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip(tr("blocknet client"));
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon->show();
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow *)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif

    notificator = new Notificator(qApp->applicationName(), trayIcon);
}

//*****************************************************************************
//*****************************************************************************
#ifndef Q_OS_MAC
void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHideAction->trigger();
    }
}
#endif

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;
    OptionsDialog dlg;
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::aboutClicked()
{
    AboutDialog dlg;
    dlg.setModel(clientModel);
    dlg.exec();
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::setNumConnections(int count)
{
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(28,54));
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to blocknet network", "", count));
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::setNumBlocks(int count, int nTotalBlocks)
{
    // don't show / hide progress bar and its label if we have no connection to the network
    if (!clientModel || clientModel->getNumConnections() == 0)
    {
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);

        return;
    }

    QString strStatusBarWarnings = clientModel->getStatusBarWarnings();
    QString tooltip;

    if(count < nTotalBlocks)
    {
        int nRemainingBlocks = nTotalBlocks - count;
        float nPercentageDone = count / (nTotalBlocks * 0.01f);

        if (strStatusBarWarnings.isEmpty())
        {
            progressBarLabel->setText(tr("Synchronizing with network..."));
            progressBarLabel->setVisible(true);
            progressBar->setFormat(tr("%1%").arg(nPercentageDone));
            progressBar->setMaximum(nTotalBlocks);
            progressBar->setValue(count);
            progressBar->setVisible(true);
        }

        tooltip = tr("Downloaded %1 of %2 blocks of transaction history (%3% done).").arg(count).arg(nTotalBlocks).arg(nPercentageDone, 0, 'f', 2);
    }
    else
    {
        if (strStatusBarWarnings.isEmpty())
            progressBarLabel->setVisible(false);

        progressBar->setVisible(false);
        tooltip = tr("Downloaded %1 blocks of transaction history.").arg(count);
    }

    // Override progressBarLabel text and hide progress bar, when we have warnings to display
    if (!strStatusBarWarnings.isEmpty())
    {
        progressBarLabel->setText(strStatusBarWarnings);
        progressBarLabel->setVisible(true);
        progressBar->setVisible(false);
    }

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    int secs = lastBlockDate.secsTo(QDateTime::currentDateTime());
    QString text;

    // Represent time from last generated block in human readable text
    if(secs <= 0)
    {
        // Fully up to date. Leave text empty.
    }
    else if(secs < 60)
    {
        text = tr("%n second(s) ago","",secs);
    }
    else if(secs < 60*60)
    {
        text = tr("%n minute(s) ago","",secs/60);
    }
    else if(secs < 24*60*60)
    {
        text = tr("%n hour(s) ago","",secs/(60*60));
    }
    else
    {
        text = tr("%n day(s) ago","",secs/(60*60*24));
    }

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60 && count >= nTotalBlocks)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(28,54));

        overviewPage->showOutOfSyncWarning(false);
    }
    else
    {
        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        labelBlocksIcon->setMovie(syncIconMovie);
        syncIconMovie->start();

        overviewPage->showOutOfSyncWarning(true);
    }

    if(!text.isEmpty())
    {
        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1.").arg(text);
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::error(const QString &title, const QString &message, bool modal)
{
    // Report errors from network/worker thread
    if(modal)
    {
        QMessageBox::critical(this, title, message, QMessageBox::Ok, QMessageBox::Ok);
    } else {
        notificator->notify(Notificator::Critical, title, message);
    }
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::closeEvent(QCloseEvent *event)
{
    if(clientModel)
    {
#ifndef Q_OS_MAC // Ignored on Mac
        if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
           !clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            qApp->quit();
        }
#endif
    }
    QMainWindow::closeEvent(event);
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::askFee(qint64 nFeeRequired, bool *payFee)
{
    QString strMessage =
        tr("This transaction is over the size limit.  You can still send it for a fee of %1, "
          "which goes to the nodes that process your transaction and helps to support the network.  "
          "Do you want to pay the fee?").arg(
                BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nFeeRequired));
    QMessageBox::StandardButton retval = QMessageBox::question(
          this, tr("Confirm transaction fee"), strMessage,
          QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
    *payFee = (retval == QMessageBox::Yes);
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::incomingTransaction(const QModelIndex & parent, int start, int end)
{
    if(!walletModel || !clientModel)
        return;
    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
                    .data(Qt::EditRole).toULongLong();
    if(!clientModel->inInitialBlockDownload())
    {
        // On new transaction, make an info balloon
        // Unless the initial block download is in progress, to prevent balloon-spam
        QString date = ttm->index(start, TransactionTableModel::Date, parent)
                        .data().toString();
        QString type = ttm->index(start, TransactionTableModel::Type, parent)
                        .data().toString();
        QString address = ttm->index(start, TransactionTableModel::ToAddress, parent)
                        .data().toString();
        QIcon icon = qvariant_cast<QIcon>(ttm->index(start,
                            TransactionTableModel::ToAddress, parent)
                        .data(Qt::DecorationRole));

        notificator->notify(Notificator::Information,
                            (amount)<0 ? tr("Sent transaction") :
                                         tr("Incoming transaction"),
                              tr("Date: %1\n"
                                 "Amount: %2\n"
                                 "Type: %3\n"
                                 "Address: %4\n")
                              .arg(date)
                              .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), amount, true))
                              .arg(type)
                              .arg(address), icon);
    }
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    centralWidget->setCurrentWidget(overviewPage);
    centralWidget->setMaximumWidth(980);
    centralWidget->setMaximumHeight(570);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

//void BitcoinGUI::gotoTradePage()
//{
//    tradeAction->setChecked(true);
//    centralWidget->setCurrentWidget(tradePage);
//	centralWidget->setMaximumWidth(850);
//	centralWidget->setMaximumHeight(520);
//
//    exportAction->setEnabled(false);
//    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
//}

void BitcoinGUI::gotoBlockBrowser()
{
    blockAction->setChecked(true);
    centralWidget->setCurrentWidget(blockBrowser);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoStatisticsPage()
{
    statisticsAction->setChecked(true);
    centralWidget->setCurrentWidget(statisticsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    centralWidget->setCurrentWidget(transactionsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), transactionView, SLOT(exportClicked()));
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::gotoAddressBookPage()
{
    addressBookAction->setChecked(true);
    centralWidget->setCurrentWidget(addressBookPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), addressBookPage, SLOT(exportClicked()));
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(receiveCoinsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), receiveCoinsPage, SLOT(exportClicked()));
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::gotoSendCoinsPage()
{
    sendCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(sendCoinsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::gotoSignMessageTab(QString addr)
{
    // call show() in showTab_SM()
    signVerifyMessageDialog->showTab_SM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::gotoVerifyMessageTab(QString addr)
{
    // call show() in showTab_VM()
    signVerifyMessageDialog->showTab_VM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::gotoMessagesPage(const QString & addr)
{
    // stop notify
    {
        messageNotifyTimer.stop();
        messagesAction->setIcon(QIcon(":/icons/address-book"));
    }

    messagesPage->showForAddress(addr);

    messagesAction->setChecked(true);
    centralWidget->setCurrentWidget(messagesPage);

    exportAction->setEnabled(false);

    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::gotoXBridgePage()
{
    xbridgeAction->setChecked(true);
    centralWidget->setCurrentWidget(xbridgePage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        int nValidUrisFound = 0;
        QList<QUrl> uris = event->mimeData()->urls();
        for (const QUrl & uri : uris)
        {
            if (sendCoinsPage->handleURI(uri.toString()))
                nValidUrisFound++;
        }

        // if valid URIs were found
        if (nValidUrisFound)
            gotoSendCoinsPage();
        else
            notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid blocknet address or malformed URI parameters."));
    }

    event->acceptProposedAction();
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::handleURI(QString strURI)
{
    // URI has to be valid
    if (sendCoinsPage->handleURI(strURI))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
    }
    else
        notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid blocknet address or malformed URI parameters."));
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_open").pixmap(28,54));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_closed").pixmap(28,54));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(true);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt:
                                     AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    setEncryptionStatus(walletModel->getEncryptionStatus());
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::backupWallet()
{
    // TODO use for QT5, need add correct preprocessor directive
#if 1
    QString saveDir;
    QStringList saveDirs = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
    if (saveDirs.size())
    {
        saveDir = saveDirs.first();
    }
#else
    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#endif

    QString filename = QFileDialog::getSaveFileName(this, tr("Backup Wallet"), saveDir, tr("Wallet Data (*.dat)"));
    if(!filename.isEmpty()) {
        if(!walletModel->backupWallet(filename)) {
            QMessageBox::warning(this, tr("Backup Failed"), tr("There was an error trying to save the wallet data to the new location."));
        }
    }
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void BitcoinGUI::unlockWallet()
{
    if(!walletModel)
        return;
    // Unlock wallet when requested by wallet model
    if(walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog::Mode mode = sender() == unlockWalletAction ?
              AskPassphraseDialog::UnlockStaking : AskPassphraseDialog::Unlock;
        AskPassphraseDialog dlg(mode, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::lockWallet()
{
    if(!walletModel)
        return;

    walletModel->setWalletLocked(true);
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void BitcoinGUI::updateStakingIcon()
{
    uint64_t nMinWeight = 0, nMaxWeight = 0, nWeight = 0;
    if (pwalletMain)
        pwalletMain->GetStakeWeight(*pwalletMain, nMinWeight, nMaxWeight, nWeight);

    if (nLastCoinStakeSearchInterval && nWeight)
    {
        uint64_t nNetworkWeight = GetPoSKernelPS();
        unsigned nEstimateTime = nTargetSpacing * nNetworkWeight / nWeight;

        QString text;
        if (nEstimateTime < 60)
        {
            text = tr("%n second(s)", "", nEstimateTime);
        }
        else if (nEstimateTime < 60*60)
        {
            text = tr("%n minute(s)", "", nEstimateTime/60);
        }
        else if (nEstimateTime < 24*60*60)
        {
            text = tr("%n hour(s)", "", nEstimateTime/(60*60));
        }
        else
        {
            text = tr("%n day(s)", "", nEstimateTime/(60*60*24));
        }

        labelStakingIcon->setPixmap(QIcon(":/icons/staking_on").pixmap(28,54));
        labelStakingIcon->setToolTip(tr("Staking.<br>Your weight is %1<br>Network weight is %2<br>Expected time to earn reward is %3").arg(nWeight).arg(nNetworkWeight).arg(text));
    }
    else
    {
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_off").pixmap(28,54));
        if (pwalletMain && pwalletMain->IsLocked())
            labelStakingIcon->setToolTip(tr("Not staking because wallet is locked"));
        else if (vNodes.empty())
            labelStakingIcon->setToolTip(tr("Not staking because wallet is offline"));
        else if (IsInitialBlockDownload())
            labelStakingIcon->setToolTip(tr("Not staking because wallet is syncing"));
        else if (!nWeight)
            labelStakingIcon->setToolTip(tr("Not staking because you don't have mature coins"));
        else
            labelStakingIcon->setToolTip(tr("Not staking"));
    }
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::incomingMessage(const QVariant message)
{
    // TODO
    // process incoming messages
    // change icon for messages page or show page

    Message msg = message.value<Message>();
    messagesPage->incomingMessage(msg);
}
//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::incomingMessageNotify(const QString & title, const QString & text)
{
    if (trayIcon->supportsMessages())
    {
        trayIcon->showMessage(title, text);
    }

    if (centralWidget->currentWidget() != messagesPage)
    {
        messageNotifyTimer.start(1000);
    }
}

//*****************************************************************************
//*****************************************************************************
void BitcoinGUI::onMessageNotifyTimer()
{
    static bool icon = false;

    if (!icon)
    {
        messagesAction->setIcon(QIcon(":/icons/receiving_addresses"));
    }
    else
    {
        messagesAction->setIcon(QIcon(":/icons/address-book"));
    }

    icon = !icon;
}
