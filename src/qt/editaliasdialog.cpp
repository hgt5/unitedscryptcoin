#include "editaliasdialog.h"
#include "ui_editaliasdialog.h"

#include "aliastablemodel.h"
#include "guiutil.h"

#include "bitcoingui.h"
#include "ui_interface.h"
#include "bitcoinrpc.h"
#include <QDataWidgetMapper>
#include <QMessageBox>
using namespace std;
using namespace json_spirit;
extern const CRPCTable tableRPC;
EditAliasDialog::EditAliasDialog(Mode mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditAliasDialog), mapper(0), mode(mode), model(0)
{
    ui->setupUi(this);

    GUIUtil::setupAddressWidget(ui->nameEdit, this);

    switch(mode)
    {
    case NewDataAlias:
        setWindowTitle(tr("New data alias"));
        //ui->aliasEdit->setEnabled(false);
        break;
    case NewAlias:
        setWindowTitle(tr("New alias"));
        break;
    case EditDataAlias:
        setWindowTitle(tr("Edit data alias"));
        break;
    case EditAlias:
        setWindowTitle(tr("Edit alias"));
        break;
    }

    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
}

EditAliasDialog::~EditAliasDialog()
{
    delete ui;
}

void EditAliasDialog::setModel(AliasTableModel *model)
{
    this->model = model;
    if(!model) return;

    mapper->addMapping(ui->aliasEdit, AliasTableModel::Name);
    mapper->addMapping(ui->nameEdit, AliasTableModel::Value);
	mapper->setModel(model);
    
}

void EditAliasDialog::loadRow(int row)
{
    mapper->setCurrentIndex(row);
}

bool EditAliasDialog::saveCurrentRow()
{
	// TODO do some input validation on all edit boxes in UI
   /* if(ui->nameEdit->text().trimmed().isEmpty())
    {
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(QString("<nobr>") + tr("You haven't provided an Alias Name.") + QString("</nobr>"));
        return;
    }
    if(ui->aliasEdit->text().trimmed().isEmpty())
    {
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(QString("<nobr>") + tr("You haven't provided an Alias Value.") + QString("</nobr>"));
        return;
    }*/
    if(!model) return false;
	Array params;
	string strMethod;
    switch(mode)
    {
    case NewDataAlias:
    case NewAlias:

		strMethod = string("aliasnew");
		params.push_back(ui->aliasEdit->text().toStdString());
		try {
            Value result = tableRPC.execute(strMethod, params);
			if (result.type() == array_type)
			{
				Array arr = result.get_array();
								
				strMethod = string("aliasactivate");
				params.clear();
				params.push_back(ui->aliasEdit->text().toStdString());
				params.push_back(arr[1].get_str());
				params.push_back(ui->nameEdit->text().toStdString());
				result = tableRPC.execute(strMethod, params);
				if (result.type() != null_type)
				{
					alias = model->addRow(AliasTableModel::Alias,
						ui->aliasEdit->text(),
						ui->nameEdit->text(),
						"N/A");
					this->model->updateEntry(ui->aliasEdit->text(), ui->nameEdit->text(), "N/A", MyAlias, CT_NEW);
					QMessageBox::information(this, windowTitle(),
					tr("New Alias created successfully! GUID for the new Alias is: \"%1\"").arg(QString::fromStdString(arr[1].get_str())),
					QMessageBox::Ok, QMessageBox::Ok);
				}	
			}
		}
		catch (Object& objError)
		{
			string strError = find_value(objError, "message").get_str();
			QMessageBox::critical(this, windowTitle(),
			tr("Error creating new Alias: \"%1\"").arg(QString::fromStdString(strError)),
				QMessageBox::Ok, QMessageBox::Ok);
			break;
		}
		catch(std::exception& e)
		{
			QMessageBox::critical(this, windowTitle(),
				tr("General exception creating new alias"),
				QMessageBox::Ok, QMessageBox::Ok);
			break;
		}							

        break;
    case EditDataAlias:
    case EditAlias:
        if(mapper->submit())
        {
			strMethod = string("aliasupdate");
			params.push_back(ui->nameEdit->text().toStdString());
			params.push_back(ui->aliasEdit->text().toStdString());
			
			try {
				Value result = tableRPC.execute(strMethod, params);
				if (result.type() != null_type)
				{
					string strResult = result.get_str();
					alias = ui->nameEdit->text() + ui->aliasEdit->text();


					this->model->updateEntry(ui->aliasEdit->text(), ui->nameEdit->text(), "N/A", MyAlias, CT_UPDATED);
					QMessageBox::information(this, windowTitle(),
					tr("Offer updated successfully! Transaction Id for the update is: \"%1\"").arg(QString::fromStdString(strResult)),
						QMessageBox::Ok, QMessageBox::Ok);
						
				}
			}
			catch (Object& objError)
			{
				string strError = find_value(objError, "message").get_str();
				QMessageBox::critical(this, windowTitle(),
				tr("Error updating offer: \"%1\"").arg(QString::fromStdString(strError)),
					QMessageBox::Ok, QMessageBox::Ok);
				break;
			}
			catch(std::exception& e)
			{
				QMessageBox::critical(this, windowTitle(),
					tr("General exception updating offer"),
					QMessageBox::Ok, QMessageBox::Ok);
				break;
			}	
        }
        break;
    }
    return !alias.isEmpty();
}

void EditAliasDialog::accept()
{
    if(!model) return;

    if(!saveCurrentRow())
    {
        switch(model->getEditStatus())
        {
        case AliasTableModel::OK:
            // Failed with unknown reason. Just reject.
            break;
        case AliasTableModel::NO_CHANGES:
            // No changes were made during edit operation. Just reject.
            break;
        case AliasTableModel::INVALID_ALIAS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered alias \"%1\" is not a valid Syscoin alias.").arg(ui->aliasEdit->text()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AliasTableModel::DUPLICATE_ALIAS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered alias \"%1\" is already taken.").arg(ui->aliasEdit->text()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AliasTableModel::WALLET_UNLOCK_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("Could not unlock wallet."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;

        }
        return;
    }
    QDialog::accept();
}

QString EditAliasDialog::getAlias() const
{
    return alias;
}

void EditAliasDialog::setAlias(const QString &alias)
{
    this->alias = alias;
    ui->aliasEdit->setText(alias);
}
