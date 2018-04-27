#include "StdAfx.h"
#include "LodGeneratorDialog.h"
#include "../UIs/ui_clodgeneratordialog.h"
#include "LODInterface.h"
#include "../Util/StringTool.h"
#include "Controls/QuestionDialog.h"
#include "QtViewPane.h"

using namespace LODGenerator;

REGISTER_VIEWPANE_FACTORY(CLodGeneratorDialog, "Lod Generator", "Tools", false);

CLodGeneratorDialog::CLodGeneratorDialog(QWidget *parent) :
    CDockableEditor(parent),
    ui(new Ui::CLodGeneratorDialog)
{
    ui->setupUi(this);

	connect(ui->cSourceFileWidget, SIGNAL(FileOpened_Signal()), this, SLOT(FileOpened()));
	connect(ui->cSourceFileWidget, &CLodGeneratorFilePanel::MaterialChanged_Signal, this, &CLodGeneratorDialog::OnMaterialChange);
	connect(ui->cGeometryGenerationPanelWidget,SIGNAL(OnMaterialGeneratePrepare_Signal()),this,SLOT(OnMaterialGeneratePrepare()));
	connect(this,SIGNAL(UpdateObj_Signal(IStatObj*)),ui->cGeometryGenerationPanelWidget,SLOT(UpdateObj(IStatObj*)));
	connect(this,SIGNAL(UpdateMaterial_Signal(IMaterial*)),ui->cGeometryGenerationPanelWidget,SLOT(UpdateMaterial(IMaterial*)));
	connect(ui->cMaterialTaskWidget,SIGNAL(OnGenerateMaterial()),this,SLOT(OnGenerateMaterial()));

	connect(ui->cGeometryTaskPanelWidget,SIGNAL(OnLodChainGenerationFinished()),ui->cGeometryGenerationPanelWidget,SLOT(OnLodChainGenerationFinished()));

	connect(ui->centralWidget,SIGNAL(OnGenerateMaterial()),this,SLOT(OnGenerateMaterial()));

	GetIEditor()->RegisterNotifyListener(this);
}

CLodGeneratorDialog::~CLodGeneratorDialog()
{
    delete ui;

	GetIEditor()->UnregisterNotifyListener(this);	
}

void CLodGeneratorDialog::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch( event )
	{
	case eNotify_OnIdleUpdate:
		{
			ui->cGeometryGenerationPanelWidget->OnIdle();
			for (int  i =0;i<ui->m_vLodPanels.size();i++)
			{
				CMaterialLODGeneratorLodItemOptionsPanel* pPanel = ui->m_vLodPanels[i];
				pPanel->OnIdle();
			}
		}

		break;

	}

}

void CLodGeneratorDialog::OnTextureSizeChanged(int nWidth, int nHeight)
{
	int curWidth = nWidth;
	int curHeight = nHeight;

	const int nNumLods = ui->m_vLodPanels.size();
	for (int idx = 1; idx < nNumLods; ++idx)
	{
		curWidth = curWidth/2;
		curHeight = curHeight/2;
		ui->m_vLodPanels[idx]->SetTextureSize(curWidth,curHeight);
	}
}

void CLodGeneratorDialog::Reset(bool value)
{
	//ClearLodPanels();

	ui->cSourceFileWidget->Reset();
	ui->cGeometryBakeOptonsWidge->Reset();
	ui->cMaterialBakeOptionsWidget->Reset();
	ui->cGeometryTaskPanelWidget->Reset();
	ui->cGeometryGenerationPanelWidget->Reset();

	CLodGeneratorInteractionManager::Instance()->ResetSettings();

	ui->cGeometryBakeOptonsWidge->Update();
	ui->cMaterialBakeOptionsWidget->Update();

	//if(m_pMatOptionsPanel)
	//	m_pMatOptionsPanel->Update();
}

bool CLodGeneratorDialog::FileOpened()
{
	Reset(true);

	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
	string filepath(CStringTool::QStringToCString(ui->cSourceFileWidget->LoadedFile()));
	if (!pInstance->LoadStatObj(filepath))
	{
		CQuestionDialog::SWarning("File Load Failed", "Failed to load file into the engine, please consult the editor.log");
		return false;
	}

	ui->cSourceFileWidget->RefreshMaterialFile();

	return LoadMaterialHelper(ui->cSourceFileWidget->MaterialFile());
}

bool CLodGeneratorDialog::OnMaterialChange(const QString& materialPath)
{
	if (LoadMaterialHelper(materialPath))
	{
		CLodGeneratorInteractionManager::Instance()->UpdateFilename(CStringTool::QStringToCString(materialPath));
		return true;
	}
	return false;
}

bool CLodGeneratorDialog::LoadMaterialHelper(const QString& materialPath)
{
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
	if(!pInstance->LoadMaterial(CStringTool::QStringToCString(materialPath)))
	{
		CQuestionDialog::SWarning("File Load Failed", "Failed to load material file into the engine, please consult the editor.log");
		return false;
	}

	ui->cGeometryGenerationPanelWidget->CreateExistingLodKeys();
	OnMaterialGeneratePrepare();
	ui->cGeometryBakeOptonsWidge->Update();
	ui->cMaterialBakeOptionsWidget->Update();

	UpdateObj_Signal(pInstance->GetLoadedModel());

	return true;
}

void CLodGeneratorDialog::ClearLodPanels()
{
	for each (CMaterialLODGeneratorLodItemOptionsPanel* pPanel in ui->m_vLodPanelParents)
	{
		int index = ui->LodGeneratorToolBox->indexOf(pPanel);
		if(index >= 0)
			ui->LodGeneratorToolBox->removeItem(index);
	}
	ui->m_vLodPanelsIdx.clear();
	ui->m_vLodPanels.clear();
	ui->m_vLodPanelParents.clear();
}


void CLodGeneratorDialog::GenerateLodPanels()
{
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
	if (!pInstance)
		return;

	int	nSourceLod = pInstance->GetGeometryOption<int>("nSourceLod");

	const int nLods = pInstance->NumberOfLods();
	for (int idx = nSourceLod+1; idx < nLods; ++idx)
	{
		int nLodIdx = idx;
		int nSubmatIdx = pInstance->GetSubMatId(nLodIdx);
		bool bAllowTextureSizeControl = idx == nSourceLod+1;
		CMaterialLODGeneratorLodItemOptionsPanel* pItemPanel = new CMaterialLODGeneratorLodItemOptionsPanel();
		pItemPanel->SetParams(nLodIdx,nSubmatIdx,bAllowTextureSizeControl);
		if(bAllowTextureSizeControl)
			connect(pItemPanel,SIGNAL(OnTextureSizeChanged(int,int)),this,SLOT(OnTextureSizeChanged(int,int)));
		if (!pItemPanel)
			continue;

		string panelName;
		panelName.Format(CMaterialLODGeneratorLodItemOptionsPanel::kPanelCaption,idx);

		//---------------------------------------------------------------------------------------------------

		QWidget* SourceFileWidget = new QWidget();
		//SourceFileWidget->setObjectName(QStringLiteral("SourceFileWidget"));
		SourceFileWidget->setGeometry(QRect(0, 0, 437, 411));
		QVBoxLayout* verticalLayout = new QVBoxLayout(SourceFileWidget);
		verticalLayout->setSpacing(6);
		verticalLayout->setContentsMargins(11, 11, 11, 11);
		verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
		verticalLayout->setContentsMargins(0, 0, 0, 0);
// 		cSourceFileWidget = new CLodGeneratorFilePanel(SourceFileWidget);
// 		cSourceFileWidget->setObjectName(QStringLiteral("cSourceFileWidget"));
		QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		sizePolicy.setHorizontalStretch(0);
		sizePolicy.setVerticalStretch(0);
		sizePolicy.setHeightForWidth(pItemPanel->sizePolicy().hasHeightForWidth());
		pItemPanel->setSizePolicy(sizePolicy);
		QHBoxLayout* horizontalLayout = new QHBoxLayout(pItemPanel);
		horizontalLayout->setSpacing(6);
		horizontalLayout->setContentsMargins(11, 11, 11, 11);
		horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
		horizontalLayout->setContentsMargins(0, 0, 0, 0);

		verticalLayout->addWidget(pItemPanel);

		QSpacerItem* verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

		verticalLayout->addItem(verticalSpacer);


		//---------------------------------------------------------------------------------------------------

		int index = ui->LodGeneratorToolBox->addItem(SourceFileWidget,CStringTool::CStringToQString(panelName));

		ui->m_vLodPanelsIdx.push_back(index);
		ui->m_vLodPanels.push_back(pItemPanel);
		ui->m_vLodPanelParents.push_back(SourceFileWidget);
	}

	for (int idx = 1; idx < nLods; idx++)
	{
		const int nSubMats = pInstance->GetSubMatCount(idx);
		if (nSubMats > 1)
		{
			CQuestionDialog::SWarning("LOD Sub-Material Warning", "Loaded lods have multiple sub-materials please ensure only a single material is used before attempting to bake texture materials");
			break;
		}
	}
}


void CLodGeneratorDialog::OnMaterialGeneratePrepare()
{
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
	if(!pInstance->GetLoadedModel())
		return;

	ClearLodPanels();
	GenerateLodPanels();
	ui->cMaterialBakeOptionsWidget->Update();

	XmlNodeRef settings = pInstance->GetSettings("MaterialLodSettings");
	const int individualLodChildSettings = settings->getChildCount();
	for (int idx = 0; idx < individualLodChildSettings; ++idx)
	{
		XmlNodeRef child = settings->getChild(idx);
		int nLodId = -1;
		if (!child->getAttr("nLodId",nLodId))
			break;

		const int nNumLods = ui->m_vLodPanels.size();
		for (int panelidx = 0; panelidx < nNumLods; ++panelidx)
		{
			if ( ui->m_vLodPanels[panelidx]->LodId() == nLodId)
			{
				 ui->m_vLodPanels[panelidx]->Serialize(child,true);
				break;
			}
		}
	}

	return;
}


bool CLodGeneratorDialog::OnGenerateMaterial()
{
	CWaitCursor wait;
	CLodGeneratorInteractionManager::Instance()->ClearResults();
	const int nNumLods = ui->m_vLodPanels.size();
	for (int idx = 0; idx < nNumLods; ++idx)
	{
		CMaterialLODGeneratorLodItemOptionsPanel* pPanel = ui->m_vLodPanels[idx];
		if (!pPanel->IsBakingEnabled())
			continue;

		int nLodId = pPanel->LodId();
		int nWidth = pPanel->Width();
		int nHeight = pPanel->Height();

		CLodGeneratorInteractionManager::Instance()->RunProcess(nLodId,nWidth,nHeight);
		pPanel->UpdateUITextures();
	}
	OnSave();
	CLodGeneratorInteractionManager::Instance()->ClearResults();

	return TRUE;
}


bool CLodGeneratorDialog::OnSave()
{
	CWaitCursor wait;
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();

	const int nNumLods = ui->m_vLodPanels.size();
	for (int idx = 0; idx < nNumLods; ++idx)
	{
		CMaterialLODGeneratorLodItemOptionsPanel* pPanel = ui->m_vLodPanels[idx];
		if (!pPanel->IsBakingEnabled())
			continue;

		int nLodId = pPanel->LodId();
		pInstance->SaveTextures(nLodId);
	}

	ui->cMaterialBakeOptionsWidget->Update();

	XmlNodeRef settings = pInstance->GetSettings("MaterialLodSettings");
	settings->removeAllChilds();
	for (int idx = 0; idx < nNumLods; ++idx)
	{
		CMaterialLODGeneratorLodItemOptionsPanel* pPanel = ui->m_vLodPanels[idx];
		pPanel->Serialize(settings,false);
	}

	// update panel material,cause of reloading
	for (int idx = 0; idx < nNumLods; ++idx)
	{
		CMaterialLODGeneratorLodItemOptionsPanel* pPanel = ui->m_vLodPanels[idx];
		pPanel->UpdateMaterial();
	}

	pInstance->SaveSettings();
	return true;
}

const char* CLodGeneratorDialog::GetEditorName() const
{
	return "LOD Generator";
}
