#include "aliaslistpage.h"
#include "ui_aliaslistpage.h"

#include "aliastablemodel.h"
#include "optionsmodel.h"
#include "bitcoingui.h"
#include "bitcoinrpc.h"
#include "editaliasdialog.h"
#include "csvmodelwriter.h"
#include "guiutil.h"

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>

using namespace std;
using namespace json_spirit;

extern const CRPCTable tableRPC;
extern string JSONRPCReply(const Value& result, const Value& error, const Value& id);

AliasListPage::AliasListPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AliasListPage),
    model(0),
    optionsModel(0)
{
    ui->setupUi(this);

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->copyAlias->setIcon(QIcon());
    ui->exportButton->setIcon(QIcon());
#endif

    ui->labelExplanation->setText(tr("These are the first 500 registered Syscoin Aliases."));
	
    // Context menu actions
    QAction *copyAliasAction = new QAction(ui->copyAlias->text(), this);
    QAction *copyAliasValueAction = new QAction(tr("Copy Va&lue"), this);
    QAction *editAction = new QAction(tr("&Edit"), this);
    QAction *transferAliasAction = new QAction(tr("&Transfer Alias"), this);

    // Build context menu
    contextMenu = new QMenu();
    contextMenu->addAction(copyAliasAction);
    contextMenu->addAction(copyAliasValueAction);
    contextMenu->addAction(editAction);
    contextMenu->addSeparator();
    contextMenu->addAction(transferAliasAction);

    // Connect signals for context menu actions
    connect(copyAliasAction, SIGNAL(triggered()), this, SLOT(on_copyAlias_clicked()));
    connect(copyAliasValueAction, SIGNAL(triggered()), this, SLOT(onCopyAliasValueAction()));
   
    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));


    ui->lineEditAliasSearch->setPlaceholderText(tr("Enter an Alias to search"));
}

AliasListPage::~AliasListPage()
{
    delete ui;
}

void AliasListPage::setModel(AliasTableModel *model)
{
    this->model = model;
    if(!model) return;

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterRole(AliasTableModel::TypeRole);
    proxyModel->setFilterFixedString(AliasTableModel::Alias);
    ui->tableView->setModel(proxyModel);
    ui->tableView->sortByColumn(0, Qt::AscendingOrder);

    // Set column widths
#if QT_VERSION < 0x050000
    ui->tableView->horizontalHeader()->setResizeMode(AliasTableModel::Name, QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setResizeMode(AliasTableModel::Value, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setResizeMode(AliasTableModel::ExpirationDepth, QHeaderView::ResizeToContents);
#else
    ui->tableView->horizontalHeader()->setSectionResizeMode(AliasTableModel::Name, QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(AliasTableModel::Value, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(AliasTableModel::ExpirationDepth, QHeaderView::ResizeToContents);
#endif


    connect(ui->tableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(selectionChanged()));


    // Select row for newly created alias
    connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(selectNewAlias(QModelIndex,int,int)));

    selectionChanged();

}

void AliasListPage::setOptionsModel(OptionsModel *optionsModel)
{
    this->optionsModel = optionsModel;
}

void AliasListPage::on_copyAlias_clicked()
{
   
    GUIUtil::copyEntryData(ui->tableView, AliasTableModel::Name);
}

void AliasListPage::onCopyAliasValueAction()
{
    GUIUtil::copyEntryData(ui->tableView, AliasTableModel::Value);
}


void AliasListPage::onTransferAliasAction()
{
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AliasTableModel::Name);

    foreach (QModelIndex index, indexes)
    {
        QString alias = index.data().toString();
        emit transferAlias(alias);
    }
}

void AliasListPage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;

    if(table->selectionModel()->hasSelection())
    {
        ui->copyAlias->setEnabled(true);
    }
    else
    {
        ui->copyAlias->setEnabled(false);
    }
}

void AliasListPage::on_exportButton_clicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(
            this,
            tr("Export Alias Data"), QString(),
            tr("Comma separated file (*.csv)"));

    if (filename.isNull()) return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(proxyModel);
    writer.addColumn("Alias", AliasTableModel::Name, Qt::EditRole);
    writer.addColumn("Value", AliasTableModel::Value, Qt::EditRole);
    writer.addColumn("Expiration Depth", AliasTableModel::ExpirationDepth, Qt::EditRole);
    if(!writer.write())
    {
        QMessageBox::critical(this, tr("Error exporting"), tr("Could not write to file %1.").arg(filename),
                              QMessageBox::Abort, QMessageBox::Abort);
    }
}

void AliasListPage::on_transferAlias_clicked()
{

}

void AliasListPage::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tableView->indexAt(point);
    if(index.isValid()) {
        contextMenu->exec(QCursor::pos());
    }
}

void AliasListPage::selectNewAlias(const QModelIndex &parent, int begin, int /*end*/)
{
    QModelIndex idx = proxyModel->mapFromSource(model->index(begin, AliasTableModel::Name, parent));
    if(idx.isValid() && (idx.data(Qt::EditRole).toString() == newAliasToSelect))
    {
        // Select row of newly created alias, once
        ui->tableView->setFocus();
        ui->tableView->selectRow(idx.row());
        newAliasToSelect.clear();
    }
}

void AliasListPage::on_searchAlias_clicked()
{
    if(ui->lineEditAliasSearch->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, tr("Error Searching Alias"),
            tr("You haven't entered an Alias"),
            QMessageBox::Ok, QMessageBox::Ok);
        return;
    }
       Array params;
        Value valError;
        Object ret ;
        Value valResult;
        Array arr;
        Value valId;
        Value result ;
        string strReply;
        string strError;
        string strMethod = string("aliasfilter");
         
        params.push_back(ui->lineEditAliasSearch->text().toStdString());

        try {
            result = tableRPC.execute(strMethod, params);
        }
        catch (Object& objError)
        {
            strError = find_value(objError, "message").get_str();
            QMessageBox::critical(this, windowTitle(),
            tr("Error searching Alias: \"%1\"").arg(QString::fromStdString(strError)),
                QMessageBox::Ok, QMessageBox::Ok);
            return;
        }
        catch(std::exception& e)
        {
            QMessageBox::critical(this, windowTitle(),
                tr("General exception when searchig alias"),
                QMessageBox::Ok, QMessageBox::Ok);
            return;
        }
    
        if (result.type() == array_type)
        {
            
            
        }
        else
        {
            QMessageBox::critical(this, windowTitle(),
                tr("Error: Invalid response from aliasnew command"),
                QMessageBox::Ok, QMessageBox::Ok);
            return;
        }

}

bool AliasListPage::handleURI(const QString& strURI)
{
 
    return false;
}
