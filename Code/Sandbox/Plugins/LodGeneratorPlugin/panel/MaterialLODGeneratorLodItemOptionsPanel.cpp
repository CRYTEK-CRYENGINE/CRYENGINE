#include "StdAfx.h"
#include "MaterialLODGeneratorLodItemOptionsPanel.h"
#include "../UIs/ui_cmateriallodgeneratorloditemoptionspanel.h"
#include "LODInterface.h"
#include "../Util/TopologyTransform.h"
#include "Material/Material.h"

using namespace LODGenerator;

const char * CMaterialLODGeneratorLodItemOptionsPanel::kPanelCaption = "LOD (%d) Options Panel";

CMaterialLODGeneratorLodItemOptionsPanel::CMaterialLODGeneratorLodItemOptionsPanel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CMaterialLODGeneratorLodItemOptionsPanel)
{
	ui->setupUi(this);

	m_nLodId = -1;

	m_pMeshMap = NULL;


	connect(ui->Bake_CheckBox, SIGNAL(stateChanged(int)), this, SLOT(SetIsBakingEnabled(int)));

	connect(ui->TextureSize_Slider,SIGNAL(valueChanged(int)),this,SLOT(UpdateSizeControl(int)));

	ui->TextureSize_Slider->setMinimum(10);
	ui->TextureSize_Slider->setMaximum(22);
	ui->TextureSize_Slider->setValue(256); //start at 512x512 and drop down each lod id

	m_bEnabled = true;
	ui->Bake_CheckBox->setChecked(m_bEnabled);

	m_pUVPreview = new CMeshBakerUVPreview(ui->UVView);
	m_pUVPreview->SetRenderMesh(false);

	connect(ui->UVView, SIGNAL(SignalRender(const SRenderContext&)), this, SLOT(OnRenderUV(const SRenderContext&)));
	connect(ui->UVView, SIGNAL(SignalMouse(const SMouseEvent&)), this, SLOT(OnMouseEventUV(const SMouseEvent&)));
	connect(ui->UVView, SIGNAL(SignalKey(const SKeyEvent&)), this, SLOT(OnKeyEventUV(const SKeyEvent&)));

	m_pMeshPreview = new CMeshBakerUVPreview(ui->CageView);

	connect(ui->CageView, SIGNAL(SignalRender(const SRenderContext&)), this, SLOT(OnRender(const SRenderContext&)));
	connect(ui->CageView, SIGNAL(SignalMouse(const SMouseEvent&)), this, SLOT(OnMouseEvent(const SMouseEvent&)));
	connect(ui->CageView, SIGNAL(SignalKey(const SKeyEvent&)), this, SLOT(OnKeyEvent(const SKeyEvent&)));
}

CMaterialLODGeneratorLodItemOptionsPanel::~CMaterialLODGeneratorLodItemOptionsPanel()
{
	delete ui;

	if (m_pMeshMap != NULL)
		delete m_pMeshMap;
}

void CMaterialLODGeneratorLodItemOptionsPanel::UpdateLOD()
{
	UpdateMesh();
	UpdateMaterial();
	UpdateUI();
}

void CMaterialLODGeneratorLodItemOptionsPanel::UpdateUI()
{
	ui->TextureSize_Slider->setValue(18-((m_nLodId-1)*2)); //start at 512x512 and drop down each lod id
}

void CMaterialLODGeneratorLodItemOptionsPanel::UpdateMesh()
{
	if (m_pMeshMap != NULL)
	{
		delete m_pMeshMap;
		m_pMeshMap = NULL;
	}
	m_pMeshMap = LODGenerator::CTopologyTransform::TransformToTopology(CLodGeneratorInteractionManager::Instance()->GetLoadedModel(m_nLodId));

	if (m_pMeshMap != NULL)
	{
		m_pUVPreview->SetMeshMap(m_pMeshMap);
		m_pMeshPreview->SetMeshMap(m_pMeshMap);
	}
}

void CMaterialLODGeneratorLodItemOptionsPanel::UpdateMaterial()
{
	CMaterial* pMtl = CLodGeneratorInteractionManager::Instance()->GetLODMaterial();
	if (pMtl != NULL)
	{
		int subMaterialCount=pMtl->GetSubMaterialCount();
		CMaterial *pSubMat=pMtl->GetSubMaterial(m_nSubMaterialId);

		if(pSubMat != NULL)
		{
			m_pMeshPreview->SetMaterial(pSubMat->GetMatInfo(true));
		}
	}
}

void CMaterialLODGeneratorLodItemOptionsPanel::OnRenderUV(const SRenderContext& rc)
{
	m_pUVPreview->OnRender(rc);
}

void CMaterialLODGeneratorLodItemOptionsPanel::OnKeyEventUV(const SKeyEvent& sKeyEvent)
{
	//m_pUVPreview->OnKeyEvent(sKeyEvent);
}

void CMaterialLODGeneratorLodItemOptionsPanel::OnMouseEventUV(const SMouseEvent& sMouseEvent)
{
	m_pUVPreview->OnMouseEvent(sMouseEvent);
}

void CMaterialLODGeneratorLodItemOptionsPanel::SetParams(int nLodId, int nSubmatId, bool bAllowControl)
{	
	m_nLodId = nLodId;
	m_nSubMaterialId = nSubmatId;
	m_bAllowControl = bAllowControl;


	ui->TextureSize_Slider->setEnabled(m_bAllowControl);
	if (m_bAllowControl)
		ui->TextureSize_Slider->show();
	else
		ui->TextureSize_Slider->hide();

	UpdateLOD();

	FitToScreen();
}

void CMaterialLODGeneratorLodItemOptionsPanel::FitToScreen()
{
	SetCameraLookAt( 2.0f,Vec3(1,1,-0.5) );
}

void CMaterialLODGeneratorLodItemOptionsPanel::SetCameraLookAt( float fRadiusScale,const Vec3 &fromDir )
{
// 	if(m_pMeshMap == NULL)
// 		return;
// 
// 	SViewportSettings sViewportSettings = ui->UVView->GetSettings();
// 	SViewportCameraSettings& sViewportCameraSettings = sViewportSettings.camera;
// 
// 	OGF::vec3 vmin,vmax,vcenter;
// 	float vradius;
// 	m_pMeshMap->compue_vetex_minmax(vmin,vmax,vcenter,vradius);
// 
// 	
// 	Vec3 cameraTarget = Vec3(vcenter.x,vcenter.y,vcenter.z);
// 
// 	Vec3 dir = fromDir.GetNormalized();
// 	Matrix34 tm = Matrix33::CreateRotationVDir(dir,0);
// 	tm.SetTranslation( cameraTarget - dir*vradius );
// 	sViewportCameraSettings.
// 	m_camera.SetMatrix(tm);
// 	if (m_cameraChangeCallback)
// 		m_cameraChangeCallback(m_pCameraChangeUserData, this);
// 		
// 
// 	ui->UVView->SetSettings(sViewportSettings);
}

int CMaterialLODGeneratorLodItemOptionsPanel::LodId()
{
	return m_nLodId;
}

int CMaterialLODGeneratorLodItemOptionsPanel::SubMatId()
{
	return m_nSubMaterialId;
}

bool CMaterialLODGeneratorLodItemOptionsPanel::IsBakingEnabled()
{
	return (bool)m_bEnabled;
}

void CMaterialLODGeneratorLodItemOptionsPanel::SetIsBakingEnabled(int bEnable)
{
	m_bEnabled = bEnable;
}

int CMaterialLODGeneratorLodItemOptionsPanel::Width()
{
	int outputHeight = 1<<(ui->TextureSize_Slider->value()/2);
	int outputWidth = outputHeight;
	if (ui->TextureSize_Slider->value()&1)
		outputWidth <<= 1;
	return outputWidth;
}

int CMaterialLODGeneratorLodItemOptionsPanel::Height()
{
	int outputHeight = 1<<(ui->TextureSize_Slider->value()/2);
	return outputHeight;
}


void CMaterialLODGeneratorLodItemOptionsPanel::UpdateSizeControl(int value)
{
	QString labelText;
	int width = Width();
	int height = Height();
	labelText = QString("%1x%2").arg(width).arg(height);
	ui->TextureSize_Label->setText(labelText);

	OnTextureSizeChanged(Width(),Height());
}


void CMaterialLODGeneratorLodItemOptionsPanel::SetTextureSize(int nWidth, int nHeight)
{
	int pos = (logf((float)nWidth) / logf((float)2))*2;
	ui->TextureSize_Slider->setValue(pos);
	UpdateSizeControl(0);
}

void CMaterialLODGeneratorLodItemOptionsPanel::Serialize(XmlNodeRef xml,bool load)
{
	if ( !load )
	{
		XmlNodeRef ref = xml->createNode("IndividualLodSettings");
		ref->setAttr("nLodId",m_nLodId);
		ref->setAttr("nWidth",Width());
		ref->setAttr("nHeight",Height());
		ref->setAttr("bEnabled",IsBakingEnabled());
		xml->addChild(ref);
	}
	else
	{
		int nWidth = 0;
		int nHeight = 0;
		xml->getAttr("nLodId", m_nLodId);
		xml->getAttr("bEnabled", m_bEnabled);
		xml->getAttr("nWidth",nWidth);
		xml->getAttr("nHeight",nHeight);
		SetTextureSize(nWidth,nHeight);
	}
}

void CMaterialLODGeneratorLodItemOptionsPanel::OnRender(const SRenderContext& rc)
{
	m_pMeshPreview->OnRender(rc);
}

void CMaterialLODGeneratorLodItemOptionsPanel::OnKeyEvent(const SKeyEvent& sKeyEvent)
{
	//m_pMeshPreview->OnKeyEvent(sKeyEvent);
}

void CMaterialLODGeneratorLodItemOptionsPanel::OnMouseEvent(const SMouseEvent& sMouseEvent)
{
	m_pMeshPreview->OnMouseEvent(sMouseEvent);
}

void CMaterialLODGeneratorLodItemOptionsPanel::OnIdle()
{
	ui->CageView->Update();
	ui->UVView->Update();
}

void CMaterialLODGeneratorLodItemOptionsPanel::UpdateUITextures()
{
	const SMeshBakingOutput* pResults = CLodGeneratorInteractionManager::Instance()->GetResults(m_nLodId);
	if (!pResults)
		return;

	for(int texid = CLodGeneratorInteractionManager::eTextureType_Diffuse; texid < CLodGeneratorInteractionManager::eTextureType_Max; ++texid)
	{
		UpdateUITexture(pResults->ppOuputTexture[texid],(CLodGeneratorInteractionManager::eTextureType)texid);
	}

}

void CMaterialLODGeneratorLodItemOptionsPanel::UpdateUITexture(ITexture* pTex, int type)
{
	switch (type)
	{
	case CLodGeneratorInteractionManager::eTextureType_Diffuse: ui->DiffuseView->SetTexture(pTex, false);
		break;
	case CLodGeneratorInteractionManager::eTextureType_Normal: ui->NormalView->SetTexture(pTex, false);
		break;
	case CLodGeneratorInteractionManager::eTextureType_Spec: ui->SpecularView->SetTexture(pTex, false);
		break;
	}
}
