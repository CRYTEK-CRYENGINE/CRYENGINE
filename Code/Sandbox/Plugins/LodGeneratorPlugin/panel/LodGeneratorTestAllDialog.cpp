#include "StdAfx.h"

#include "LodGeneratorTestAllDialog.h"
#include "../UIs/ui_CLodGeneratorTestAllDialog.h"

CLodGeneratorTestAllDialog::CLodGeneratorTestAllDialog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CLodGeneratorTestAllDialog)
{
    ui->setupUi(this);

	GetIEditor()->RegisterNotifyListener(this);
}

CLodGeneratorTestAllDialog::~CLodGeneratorTestAllDialog()
{
    delete ui;

	GetIEditor()->UnregisterNotifyListener(this);	
}

void CLodGeneratorTestAllDialog::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch( event )
	{
	case eNotify_OnIdleUpdate:
		{

		}

		break;

	}

}