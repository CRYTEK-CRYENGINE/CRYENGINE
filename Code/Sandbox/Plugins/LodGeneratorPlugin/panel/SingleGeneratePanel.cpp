#include "StdAfx.h"

#include "SingleGeneratePanel.h"
#include "Util/PakFile.h"
#include "../UIs/ui_singlegeneratepanel.h"
#include "AutoGeneratorDataBase.h"
#include "Material/Material.h"
#include "Objects/BrushObject.h"
#include "FilePathUtil.h"

CSingleGeneratePanel::CSingleGeneratePanel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SingleGeneratePanel)
{
    ui->setupUi(this);

	connect(ui->SelectObj, SIGNAL(clicked()), this, SLOT(SelectObj_Slot()));
	connect(ui->Prepare, SIGNAL(clicked()), this, SLOT(Prepare_Slot()));
	connect(ui->AddLod, SIGNAL(clicked()), this, SLOT(AddLod_Slot()));
	connect(ui->GenerateMesh, SIGNAL(clicked()), this, SLOT(GenerateMesh_Slot()));
	connect(ui->GenerateTexture, SIGNAL(clicked()), this, SLOT(GenerateTexture_Slot()));
	connect(ui->SingleAutoGenerate, SIGNAL(clicked()), this, SLOT(SingleAutoGenerate_Slot()));
	connect(ui->LodIndex,SIGNAL(textChanged(const QString)),this,SLOT(LodIndex_Slot(const QString)));

	connect(this,SIGNAL(UpdateProgress_Signal(int)),ui->GenerateProgress,SLOT(setValue(int)));

	OriginWidth = 1024;
	OriginHeight = 1024;
	OriginPercent = 0.5;
	index = 0;
	m_lod = -1;

	m_PrepareDone = true;

	UpdateProgress_Signal(0);
}

CSingleGeneratePanel::~CSingleGeneratePanel()
{
    delete ui;
}

void CSingleGeneratePanel::Prepare_Slot()
{
	m_nTimerId = startTimer(1000);
	m_AutoGenerator.Prepare();
	m_PrepareDone = false;
}

void CSingleGeneratePanel::AddLod_Slot()
{
	index++;

	m_AutoGenerator.CreateLod(pow(OriginPercent,index),OriginWidth/pow(2,index - 1),OriginHeight/pow(2,index - 1));
}

void CSingleGeneratePanel::GenerateMesh_Slot()
{
	if (m_lod <= 0 && m_lod > index)
		return;

	m_AutoGenerator.GenerateLodMesh(m_lod);
}

void CSingleGeneratePanel::GenerateTexture_Slot()
{
	if (m_lod <= 0 && m_lod > index)
		return;

	m_AutoGenerator.GenerateLodTexture(m_lod);
}

void CSingleGeneratePanel::SingleAutoGenerate_Slot()
{
	if (m_lod <= 0 && m_lod > index)
		return;

	m_AutoGenerator.GenerateLodMesh(m_lod);
	m_AutoGenerator.GenerateLodTexture(m_lod);
}

void CSingleGeneratePanel::LodIndex_Slot(const QString str)
{
	bool success = false;
	int lod = str.toInt(&success);
	if(success)
		m_lod = lod;
}

void CSingleGeneratePanel::ShowMessage(std::string message)
{
	CryLog("Lod Generator Auto: %s",message.c_str());
	//ui->MessageShow->append(message);
}


void CSingleGeneratePanel::timerEvent( QTimerEvent *event )
{
	if (!m_AutoGenerator.IsPrepareDone())
	{
		float fProgress = 0.0f;
		fProgress = m_AutoGenerator.Tick();
		UpdateProgress_Signal(100*fProgress);
	}			
	else
	{
		UpdateProgress_Signal(100);
		if ( m_nTimerId != 0 )
			killTimer(m_nTimerId);
		m_nTimerId = 0;
	}
}

const string GetSelectedBrushFilepath()
{
	string objectName("");
	CBaseObject * pObject = GetIEditor()->GetSelectedObject();
	if (!pObject)
		return objectName;

	if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		CBrushObject *pBrushObject = (CBrushObject*)pObject;
		return string(pBrushObject->GetIStatObj()->GetFilePath());
	}

	return objectName;
}

const string GetSelectedBrushMaterial()
{
	string materialName("");
	CBaseObject * pObject = GetIEditor()->GetSelectedObject();
	if (!pObject)
		return materialName;

	IEditorMaterial* pMaterial = NULL;
	if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		CBrushObject* pBrushObject = (CBrushObject*)pObject;
		pMaterial = pBrushObject->GetRenderMaterial();
	}

	if (!pMaterial)
		return materialName;

	string convertedPath(PathUtil::AbsolutePathToGamePath(pMaterial->GetFullName()));
	return convertedPath;
}

void CSingleGeneratePanel::SelectObj_Slot()
{
	string meshPath = GetSelectedBrushFilepath();
	string materialPath = GetSelectedBrushMaterial(); 

	if ( meshPath.IsEmpty() || materialPath.IsEmpty())
		return;

	m_AutoGenerator.Init(meshPath,materialPath);
}