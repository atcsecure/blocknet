//*****************************************************************************
//*****************************************************************************

#include "addressbookpage.h"
#include "ui_addressbookpage.h"

#include "addresstablemodel.h"
#include "optionsmodel.h"
#include "bitcoingui.h"
#include "editaddressdialog.h"
#include "csvmodelwriter.h"
#include "guiutil.h"

#ifdef USE_QRCODE
#include "qrcodedialog.h"
#endif

#include "../util/verify.h"
#include "../key.h"
#include "../base58.h"
#include "../wallet.h"
#include "../init.h"

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>

//*****************************************************************************
//*****************************************************************************
AddressBookPage::AddressBookPage(Mode mode, Tabs tab, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddressBookPage),
    model(0),
    optionsModel(0),
    mode(mode),
    tab(tab)
{
    ui->setupUi(this);

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->newAddressButton->setIcon(QIcon());
    ui->copyToClipboard->setIcon(QIcon());
    ui->deleteButton->setIcon(QIcon());
#endif

#ifndef USE_QRCODE
    ui->showQRCode->setVisible(false);
#endif

    switch(mode)
    {
    case ForSending:
        connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(accept()));
        ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->tableView->setFocus();
        break;
    case ForEditing:
        ui->buttonBox->setVisible(false);
        break;
    }
    switch(tab)
    {
    case SendingTab:
        ui->labelExplanation->setVisible(false);
        ui->deleteButton->setVisible(true);
        ui->signMessage->setVisible(false);
        break;
    case ReceivingTab:
        ui->deleteButton->setVisible(false);
        ui->signMessage->setVisible(true);
        break;
    }

    // Context menu actions
    QAction *copyLabelAction = new QAction(tr("Copy &Label"), this);
    QAction *copyAddressAction = new QAction(ui->copyToClipboard->text(), this);
    QAction * copyPubKey = new QAction(trUtf8("Copy public &key"), this);
    QAction *editAction = new QAction(tr("&Edit"), this);
    QAction *showQRCodeAction = new QAction(ui->showQRCode->text(), this);
    QAction *signMessageAction = new QAction(ui->signMessage->text(), this);
    QAction *verifyMessageAction = new QAction(ui->verifyMessage->text(), this);
    QAction * messagesAction = new QAction(trUtf8("M&essages"), this);
    deleteAction = new QAction(ui->deleteButton->text(), this);

    // Build context menu
    contextMenu = new QMenu();
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyPubKey);
    contextMenu->addAction(editAction);
    if(tab == SendingTab)
        contextMenu->addAction(deleteAction);
    contextMenu->addSeparator();
    contextMenu->addAction(showQRCodeAction);
    if(tab == ReceivingTab)
        contextMenu->addAction(signMessageAction);
    else if(tab == SendingTab)
    {
        contextMenu->addAction(verifyMessageAction);
        contextMenu->addAction(messagesAction);
    }

    // Connect signals for context menu actions
    VERIFY(connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(on_copyToClipboard_clicked())));
    VERIFY(connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(onCopyLabelAction())));
    VERIFY(connect(copyPubKey, SIGNAL(triggered()), this, SLOT(onCopyPublicKeyAction())));
    VERIFY(connect(editAction, SIGNAL(triggered()), this, SLOT(onEditAction())));
    VERIFY(connect(deleteAction, SIGNAL(triggered()), this, SLOT(on_deleteButton_clicked())));
    VERIFY(connect(showQRCodeAction, SIGNAL(triggered()), this, SLOT(on_showQRCode_clicked())));
    VERIFY(connect(signMessageAction, SIGNAL(triggered()), this, SLOT(on_signMessage_clicked())));
    VERIFY(connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(on_verifyMessage_clicked())));
    VERIFY(connect(messagesAction, SIGNAL(triggered()), this, SLOT(on_messages_clicked())));

    VERIFY(connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint))));

    // Pass through accept action from button box
    VERIFY(connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept())));
}

//*****************************************************************************
//*****************************************************************************
AddressBookPage::~AddressBookPage()
{
    delete ui;
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::setModel(AddressTableModel *model)
{
    this->model = model;
    if(!model)
        return;

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    switch(tab)
    {
    case ReceivingTab:
        // Receive filter
        proxyModel->setFilterRole(AddressTableModel::TypeRole);
        proxyModel->setFilterFixedString(AddressTableModel::Receive);
        break;
    case SendingTab:
        // Send filter
        proxyModel->setFilterRole(AddressTableModel::TypeRole);
        proxyModel->setFilterFixedString(AddressTableModel::Send);
        break;
    }
    ui->tableView->setModel(proxyModel);
    ui->tableView->sortByColumn(0, Qt::AscendingOrder);

    // Set column widths
    ui->tableView->horizontalHeader()->resizeSection(
            AddressTableModel::Address, 320);
    // TODO check in QT5
//    ui->tableView->horizontalHeader()->setResizeMode(
//            AddressTableModel::Label, QHeaderView::Stretch);

    connect(ui->tableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(selectionChanged()));

    // Select row for newly created address
    connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(selectNewAddress(QModelIndex,int,int)));

    selectionChanged();
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::setOptionsModel(OptionsModel *optionsModel)
{
    this->optionsModel = optionsModel;
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::on_copyToClipboard_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, AddressTableModel::Address);
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::onCopyLabelAction()
{
    GUIUtil::copyEntryData(ui->tableView, AddressTableModel::Label);
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::onCopyPublicKeyAction()
{
    if (!ui->tableView->selectionModel())
    {
        QMessageBox::warning(this, "", trUtf8("No selected rows"));
        return;
    }

    QModelIndexList indexes = ui->tableView->selectionModel()->selectedRows(AddressTableModel::Address);
    if (indexes.isEmpty())
    {
        QMessageBox::warning(this, "", trUtf8("No selected rows"));
        return;
    }

    QModelIndex idx = indexes.at(0);
    QString address = idx.data().toString();

    CPubKey pubKey;

    StoredPubKeysDb & keysDb = StoredPubKeysDb::instance();

    // check stored key
    if (keysDb.load(address.toStdString(), pubKey))
    {
        // found
        QApplication::clipboard()->setText(QString::fromStdString(EncodeBase58(pubKey.Raw())));
        return;
    }

    // not found stored key, check address
    CBitcoinAddress addr(address.toStdString());
    if (!addr.IsValid())
    {
        QMessageBox::warning(this, "", trUtf8("Invalid bitcoin address"));
        return;
    }

    CKeyID id;
    CKey key;
    if (!addr.GetKeyID(id) || !pwalletMain->GetKey(id, key))
    {
        QMessageBox::information(this, "", trUtf8("Key not found"));
        return;
    }

    pubKey = key.GetPubKey();
    if (!pubKey.IsValid())
    {
        QMessageBox::information(this, "", trUtf8("Public key not found"));
        return;
    }

    QApplication::clipboard()->setText(QString::fromStdString(EncodeBase58(pubKey.Raw())));
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::onEditAction()
{
    if(!ui->tableView->selectionModel())
        return;
    QModelIndexList indexes = ui->tableView->selectionModel()->selectedRows();
    if(indexes.isEmpty())
        return;

    EditAddressDialog dlg(
            tab == SendingTab ?
            EditAddressDialog::EditSendingAddress :
            EditAddressDialog::EditReceivingAddress);
    dlg.setModel(model);
    QModelIndex origIndex = proxyModel->mapToSource(indexes.at(0));
    dlg.loadRow(origIndex.row());
    dlg.exec();
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::on_signMessage_clicked()
{
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);
    QString addr;

    for (QModelIndex & index : indexes)
    {
        QVariant address = index.data();
        addr = address.toString();
    }

    emit signMessage(addr);
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::on_verifyMessage_clicked()
{
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);
    QString addr;

    for (QModelIndex & index : indexes)
    {
        QVariant address = index.data();
        addr = address.toString();
    }

    emit verifyMessage(addr);
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::on_messages_clicked()
{
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);
    QString addr;

    for (QModelIndex & index : indexes)
    {
        QVariant address = index.data();
        addr = address.toString();
    }

    emit showMessages(addr);
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::on_newAddressButton_clicked()
{
    if(!model)
        return;
    EditAddressDialog dlg(
            tab == SendingTab ?
            EditAddressDialog::NewSendingAddress :
            EditAddressDialog::NewReceivingAddress, this);
    dlg.setModel(model);
    if(dlg.exec())
    {
        newAddressToSelect = dlg.getAddress();
    }
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::on_deleteButton_clicked()
{
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;
    QModelIndexList indexes = table->selectionModel()->selectedRows();
    if(!indexes.isEmpty())
    {
        table->model()->removeRow(indexes.at(0).row());
    }
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;

    if(table->selectionModel()->hasSelection())
    {
        switch(tab)
        {
        case SendingTab:
            // In sending tab, allow deletion of selection
            ui->deleteButton->setEnabled(true);
            ui->deleteButton->setVisible(true);
            deleteAction->setEnabled(true);
            ui->signMessage->setEnabled(false);
            ui->signMessage->setVisible(false);
            ui->verifyMessage->setEnabled(true);
            ui->verifyMessage->setVisible(true);
            break;
        case ReceivingTab:
            // Deleting receiving addresses, however, is not allowed
            ui->deleteButton->setEnabled(false);
            ui->deleteButton->setVisible(false);
            deleteAction->setEnabled(false);
            ui->signMessage->setEnabled(true);
            ui->signMessage->setVisible(true);
            ui->verifyMessage->setEnabled(false);
            ui->verifyMessage->setVisible(false);
            break;
        }
        ui->copyToClipboard->setEnabled(true);
        ui->showQRCode->setEnabled(true);
    }
    else
    {
        ui->deleteButton->setEnabled(false);
        ui->showQRCode->setEnabled(false);
        ui->copyToClipboard->setEnabled(false);
        ui->signMessage->setEnabled(false);
        ui->verifyMessage->setEnabled(false);
    }
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::done(int retval)
{
    // When this is a tab/widget and not a modal dialog, ignore "done"
    if (mode == ForEditing)
    {
        return;
    }

    // if rejected - done
    if (retval == Rejected)
    {
        QDialog::done(retval);
        return;
    }

    QTableView *table = ui->tableView;

    // if no model - reject
    if(!table->selectionModel() || !table->model())
    {
        QDialog::done(Rejected);
        return;
    }

    // Figure out which address was selected, and return it
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);

    for (QModelIndex & index : indexes)
    {
        QVariant address = table->model()->data(index);
        returnValue = address.toString();
    }

    if(returnValue.isEmpty())
    {
        // If no address entry selected, return rejected
        retval = Rejected;
    }

    QDialog::done(retval);
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::exportClicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(
            this,
            tr("Export Address Book Data"), QString(),
            tr("Comma separated file (*.csv)"));

    if (filename.isNull()) return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(proxyModel);
    writer.addColumn("Label", AddressTableModel::Label, Qt::EditRole);
    writer.addColumn("Address", AddressTableModel::Address, Qt::EditRole);

    if(!writer.write())
    {
        QMessageBox::critical(this, tr("Error exporting"), tr("Could not write to file %1.").arg(filename),
                              QMessageBox::Abort, QMessageBox::Abort);
    }
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::on_showQRCode_clicked()
{
#ifdef USE_QRCODE
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);

    for (QModelIndex & index : indexes)
    {
        QString address = index.data().toString(), label = index.sibling(index.row(), 0).data(Qt::EditRole).toString();

        QRCodeDialog *dialog = new QRCodeDialog(address, label, tab == ReceivingTab, this);
        if(optionsModel)
            dialog->setModel(optionsModel);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    }
#endif
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tableView->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

//*****************************************************************************
//*****************************************************************************
void AddressBookPage::selectNewAddress(const QModelIndex &parent, int begin, int end)
{
    QModelIndex idx = proxyModel->mapFromSource(model->index(begin, AddressTableModel::Address, parent));
    if(idx.isValid() && (idx.data(Qt::EditRole).toString() == newAddressToSelect))
    {
        // Select row of newly created address, once
        ui->tableView->setFocus();
        ui->tableView->selectRow(idx.row());
        newAddressToSelect.clear();
    }
}
