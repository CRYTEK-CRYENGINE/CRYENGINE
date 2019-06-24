#include "StdAfx.h"

#include "LodGeneratorAllDialog.h"
#include "../UIs/ui_clodgeneratoralldialog.h"

CLodGeneratorAllDialog::CLodGeneratorAllDialog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CLodGeneratorAllDialog)
{
    ui->setupUi(this);

	GetIEditor()->RegisterNotifyListener(this);
}

CLodGeneratorAllDialog::~CLodGeneratorAllDialog()
{
    delete ui;

	GetIEditor()->UnregisterNotifyListener(this);	
}

void CLodGeneratorAllDialog::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch( event )
	{
	case eNotify_OnIdleUpdate:
		{

		}

		break;

	}

}