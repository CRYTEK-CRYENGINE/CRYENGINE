#include "StdAfx.h"
#include "LodGeneratorFilePanel.h"
#include "../UIs/ui_clodgeneratorfilepanel.h"
#include "qfiledialog.h"
#include "LODInterface.h"
#include "../Util/StringTool.h"
#include "Controls/QuestionDialog.h"

using namespace LODGenerator;

CLodGeneratorFilePanel::CLodGeneratorFilePanel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CLodGeneratorFilePanel)
{   
    ui->setupUi(this);

    connect(ui->SelectMeshButton, SIGNAL(clicked()), this, SLOT(SelectMeshFile_Slot()));
	connect(ui->SelectObjButton, SIGNAL(clicked()), this, SLOT(SelectObjFile_Slot()));
	connect(ui->SelectMaterialButton, SIGNAL(clicked()), this, SLOT(SelectMaterial_Slot()));
	connect(ui->ChangeMaterialButton, SIGNAL(clicked()), this, SLOT(ChangeMaterial_Slot()));

    connect(this,SIGNAL(SelectMeshFile_Signal(const QString)),ui->SelectMeshPathEdit,SLOT(setText(const QString)));
	connect(this,SIGNAL(SelectMateralFile_Signal(const QString)),ui->SelectMaterialPathEdit,SLOT(setText(const QString)));

}

CLodGeneratorFilePanel::~CLodGeneratorFilePanel()
{
    delete ui;
}

void CLodGeneratorFilePanel::SelectMaterial_Slot()
{
	setCursor(QCursor(Qt::WaitCursor));  
	CLodGeneratorInteractionManager::Instance()->OpenMaterialEditor();
	setCursor(QCursor(Qt::ArrowCursor));  
}

void CLodGeneratorFilePanel::ChangeMaterial_Slot()
{
	const QString material(QFileDialog::getOpenFileName(this, tr("Select Material"), "", tr("Material (*.mtl)")));
	if (!material.isNull())
	{
		MaterialChanged_Signal(material);
		SelectMateralFile_Signal(material);
	}
}

void CLodGeneratorFilePanel::SelectMeshFile_Slot()
{
    m_MeshPath = QFileDialog::getOpenFileName(this,  tr("Select File"),  "",  tr("Mesh (*.cgf)"));
	string material(CLodGeneratorInteractionManager::Instance()->GetDefaultBrushMaterial(CStringTool::QStringToCString(m_MeshPath)));

    SelectMeshFile_Signal(m_MeshPath);
	SelectMateralFile_Signal(CStringTool::CStringToQString(material));

	FileOpened_Signal();
}

void CLodGeneratorFilePanel::OnOpenWithPathParameter(const QString& objectPath )
{
	if(!objectPath.isEmpty())
	{
		CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
		m_MaterialPath = CStringTool::CStringToQString(pInstance->GetDefaultBrushMaterial(CStringTool::QStringToCString(objectPath)));

		m_MeshPath = objectPath;

		SelectMeshFile_Signal(m_MeshPath);
		SelectMateralFile_Signal(m_MaterialPath);

		pInstance->SetParameterFilePath("");
	}
}

void CLodGeneratorFilePanel::SelectObjFile_Slot()
{
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
	string filepath(pInstance->GetParameterFilePath());

	if( filepath.IsEmpty() )
		filepath = pInstance->GetSelectedBrushFilepath();
	else
	{
		OnOpenWithPathParameter(CStringTool::CStringToQString(pInstance->GetParameterFilePath()));
		return;
	}

	if ( filepath.IsEmpty() )
		return;

	setCursor(QCursor(Qt::WaitCursor)); 

	m_MeshPath = CStringTool::CStringToQString(filepath);
	m_MaterialPath = CStringTool::CStringToQString(pInstance->GetSelectedBrushMaterial());

	SelectMeshFile_Signal(m_MeshPath);
	SelectMateralFile_Signal(m_MaterialPath);

	FileOpened_Signal();

	setCursor(QCursor(Qt::ArrowCursor));  

}

const QString CLodGeneratorFilePanel::LoadedFile()
{
	return m_MeshPath;
}

const QString CLodGeneratorFilePanel::MaterialFile()
{
	return m_MaterialPath;
}

const void CLodGeneratorFilePanel::RefreshMaterialFile()
{
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();

	setCursor(QCursor(Qt::WaitCursor));  
	m_MaterialPath = CStringTool::CStringToQString(CLodGeneratorInteractionManager::Instance()->GetDefaultBrushMaterial(CStringTool::QStringToCString(m_MeshPath)));

	SelectMateralFile_Signal(m_MaterialPath);
	setCursor(QCursor(Qt::ArrowCursor)); 
}

void CLodGeneratorFilePanel::Reset()
{

}