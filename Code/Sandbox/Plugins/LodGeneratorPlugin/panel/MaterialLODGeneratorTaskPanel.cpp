#include "StdAfx.h"
#include "MaterialLODGeneratorTaskPanel.h"
#include "../UIs/ui_cmateriallodgeneratortaskpanel.h"

CMaterialLODGeneratorTaskPanel::CMaterialLODGeneratorTaskPanel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CMaterialLODGeneratorTaskPanel)
{
    ui->setupUi(this);

	connect(ui->GenerateTextures_Button,SIGNAL(clicked(bool)),this,SLOT(OnButtonGenerate(bool)));
}

CMaterialLODGeneratorTaskPanel::~CMaterialLODGeneratorTaskPanel()
{
    delete ui;
}

void CMaterialLODGeneratorTaskPanel::OnButtonGenerate(bool checked)
{
	OnGenerateMaterial();
}