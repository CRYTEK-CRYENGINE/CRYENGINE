#include "StdAfx.h"
#include "GeometryLodGeneratorPreviewPanel.h"
#include "../UIs/ui_cgeometrylodgeneratorpreviewpanel.h"
#include "LODInterface.h"
#include "../control/RampControl.h"
#include "Controls/QuestionDialog.h"

using namespace LODGenerator;

CGeometryLodGeneratorPreviewPanel::CGeometryLodGeneratorPreviewPanel(QWidget *parent) :
    QWidget(parent),ui(new Ui::CGeometryLodGeneratorPreviewPanel)
{
    ui->setupUi(this);
	m_pPreview = new CCMeshBakerPopupPreview(ui->LodGenPreviewWidget);
	m_pPreview->SetWireframe(true);


	connect(ui->LodGenUVMapAndSaveButton,SIGNAL(clicked(bool)), this, SLOT(OnGenerateLods(bool)));

	connect(ui->LodGenPreviewWidget, SIGNAL(SignalRender(const SRenderContext&)), this, SLOT(OnRender(const SRenderContext&)));
	connect(ui->LodGenPreviewWidget, SIGNAL(SignalMouse(const SMouseEvent&)), this, SLOT(OnMouseEvent(const SMouseEvent&)));
	connect(ui->LodGenPreviewWidget, SIGNAL(SignalKey(const SKeyEvent&)), this, SLOT(OnKeyEvent(const SKeyEvent&)));

	connect(ui->LodChainRampWidget, SIGNAL(OnLodValueChanged()), this, SLOT(OnLodValueChanged()));
	connect(ui->LodChainRampWidget, SIGNAL(OnLodDeleted()), this, SLOT(OnLodDeleted()));
	connect(ui->LodChainRampWidget, SIGNAL(OnLodAdded(float)), this, SLOT(OnLodAdded(float)));
	connect(ui->LodChainRampWidget, SIGNAL(OnLodSelected()), this, SLOT(OnLodSelected()));
	connect(ui->LodChainRampWidget, SIGNAL(OnSelectionCleared()), this, SLOT(OnSelectionCleared()));
}



CGeometryLodGeneratorPreviewPanel::~CGeometryLodGeneratorPreviewPanel()
{
	delete m_pPreview;
    delete ui;
}

void CGeometryLodGeneratorPreviewPanel::UpdateObj(IStatObj * pObj)
{
	m_pPreview->SetObject(pObj);
}

void CGeometryLodGeneratorPreviewPanel::UpdateMaterial(IMaterial* pMaterial)
{
	m_pPreview->SetMaterial(pMaterial);
}

void CGeometryLodGeneratorPreviewPanel::OnRender(const SRenderContext& rc)
{
	m_pPreview->OnRender(rc);
}

void CGeometryLodGeneratorPreviewPanel::OnKeyEvent(const SKeyEvent& sKeyEvent)
{
	m_pPreview->OnKeyEvent(sKeyEvent);
}

void CGeometryLodGeneratorPreviewPanel::OnMouseEvent(const SMouseEvent& sMouseEvent)
{
	m_pPreview->OnMouseEvent(sMouseEvent);
}

void CGeometryLodGeneratorPreviewPanel::Reset(bool release)
{

}

void CGeometryLodGeneratorPreviewPanel::OnIdle()
{
	ui->LodGenPreviewWidget->Update();

	/*
	if( m_PrevPivotType != GetPivotType() )
	{
		if( GetPivotType() == EPivotTypeT_Cursor )
			GetGizmo()->SetPos(GetUVCursor()->GetPos());
		else	
			GetGizmo()->SetPos(GetElementSet()->GetCenterPos());
	}

	m_PrevPivotType = GetPivotType();

	GetGizmo()->Hide(GetElementSet()->IsEmpty());
	*/
}


void CGeometryLodGeneratorPreviewPanel::CreateExistingLodKeys()
{
	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();

	if (!pInstance)
		return;

	if (!ui->LodChainRampWidget)
		return;

	ui->LodChainRampWidget->SelectAllHandles();
	ui->LodChainRampWidget->RemoveSelectedHandles();

	const int nSourceLod = pInstance->GetGeometryOption<int>("nSourceLod");
	const int nLods = pInstance->NumberOfLods();
	for ( int nLodIdx = nSourceLod+1; nLodIdx < nLods; ++nLodIdx )
	{
		ui->LodChainRampWidget->AddHandle(pInstance->GetLodsPercentage(nLodIdx));
	}
}

void CGeometryLodGeneratorPreviewPanel::OnGenerateLods(bool checked)
{
	setCursor(QCursor(Qt::WaitCursor));

	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
	int highestLod = pInstance->GetHighestLod();

	if ( highestLod > 0 )
	{
		// TODO
	/*	if ( CryMessageBox("Lods already exist for this model, generating will overwrite any existing lods.","Overwrite existing lods?",MB_OKCANCEL) == IDCANCEL )
		{
			
		}*/
	}

	const int lods = ui->LodChainRampWidget->GetHandleCount();

	pInstance->ClearUnusedLods(lods);

	int nSourceLod = pInstance->GetGeometryOption<int>("nSourceLod");
	for ( int idx = lods-1, count = 1; idx >= 0; --idx, ++count)
	{
		pInstance->GenerateLod(nSourceLod+count,ui->LodChainRampWidget->GetHandleData(idx).percentage);
	}
	pInstance->SaveSettings();
	pInstance->ReloadModel();

	OnMaterialGeneratePrepare_Signal();

	setCursor(QCursor(Qt::ArrowCursor));
}

void CGeometryLodGeneratorPreviewPanel::OnLodChainGenerationFinished()
{

	CLodGeneratorInteractionManager* pInstance = CLodGeneratorInteractionManager::Instance();
	const int nSourceLod = pInstance->GetGeometryOption<int>("nSourceLod");
	const int nLods = pInstance->NumberOfLods() - nSourceLod;
	if ( nLods == 1 )
	{
		CreateLod(50.0f);
	}
	else
	{
		SelectFirst();
	}

	EnableExport();

}

void CGeometryLodGeneratorPreviewPanel::CreateLod(float fPercentage)
{
	if (!ui->LodChainRampWidget)
		return;

	ui->LodChainRampWidget->AddHandle(fPercentage);
	ui->LodChainRampWidget->SelectHandle(fPercentage);
}


void CGeometryLodGeneratorPreviewPanel::SelectFirst()
{
	if (!ui->LodChainRampWidget)
		return;

	ui->LodChainRampWidget->SelectNextHandle();
}

void CGeometryLodGeneratorPreviewPanel::EnableExport()
{
	ui->LodGenUVMapAndSaveButton->setEnabled(true);
}


void CGeometryLodGeneratorPreviewPanel::OnLodValueChanged()
{
	CCRampControl::HandleData data;
	if ( ui->LodChainRampWidget && ui->LodChainRampWidget->GetSelectedData(data) )
	{
		CLodGeneratorInteractionManager::Instance()->GenerateTemporaryLod(data.percentage,m_pPreview,ui->LodChainRampWidget);
	}
}

void CGeometryLodGeneratorPreviewPanel::OnLodDeleted()
{
	if (ui->LodChainRampWidget)
	{
		int nLodCount = ui->LodChainRampWidget->GetHandleCount();
		OnLodRemoved(nLodCount);
	}
}

void CGeometryLodGeneratorPreviewPanel::OnLodRemoved(int iLod)
{
	int nSourceLod = CLodGeneratorInteractionManager::Instance()->GetGeometryOption<int>("nSourceLod");
	int genLodCount = iLod;
	int highestLod = CLodGeneratorInteractionManager::Instance()->GetHighestLod();

	if ((highestLod - nSourceLod) > genLodCount)
		CQuestionDialog::SWarning("LOD Tool Message", "Deleting existing lods requires a full export of the lod chain and material baking pass.");
}

void CGeometryLodGeneratorPreviewPanel::OnLodAdded(float fPercentage)
{
	CLodGeneratorInteractionManager::Instance()->GenerateTemporaryLod(fPercentage,m_pPreview,ui->LodChainRampWidget);
}

void CGeometryLodGeneratorPreviewPanel::OnLodSelected()
{
	CCRampControl::HandleData data;
	if (ui->LodChainRampWidget && ui->LodChainRampWidget->GetSelectedData(data) )
	{
		CLodGeneratorInteractionManager::Instance()->GenerateTemporaryLod(data.percentage,m_pPreview,ui->LodChainRampWidget);
	}
}

void CGeometryLodGeneratorPreviewPanel::OnSelectionCleared()
{

}
