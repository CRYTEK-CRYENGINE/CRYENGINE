#include "StdAfx.h"

#include "ABFPannelNew.h"
#include "../UIs/ui_cabfpannelnew.h"

using namespace LODGenerator;

cABFPannelNew::cABFPannelNew(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::cABFPannelNew)
{
    ui->setupUi(this);

	connect(ui->TestABFButton, SIGNAL(clicked()), this, SLOT(TestABF_Slot()));
	connect(ui->TestDecimator, SIGNAL(clicked()), this, SLOT(TestDecimator_Slot()));
	
	m_pUVPreview = new CMeshBakerUVPreview(ui->UVViwer);
	m_pUVPreview->SetRenderMesh(false);

	connect(ui->UVViwer, SIGNAL(SignalRender(const SRenderContext&)), this, SLOT(OnRenderUV(const SRenderContext&)));
	connect(ui->UVViwer, SIGNAL(SignalMouse(const SMouseEvent&)), this, SLOT(OnMouseEventUV(const SMouseEvent&)));
	connect(ui->UVViwer, SIGNAL(SignalKey(const SKeyEvent&)), this, SLOT(OnKeyEventUV(const SKeyEvent&)));

	
	m_pMeshPreview = new CMeshBakerUVPreview(ui->MeshViwer);

	connect(ui->MeshViwer, SIGNAL(SignalRender(const SRenderContext&)), this, SLOT(OnRender(const SRenderContext&)));
	connect(ui->MeshViwer, SIGNAL(SignalMouse(const SMouseEvent&)), this, SLOT(OnMouseEvent(const SMouseEvent&)));
	connect(ui->MeshViwer, SIGNAL(SignalKey(const SKeyEvent&)), this, SLOT(OnKeyEvent(const SKeyEvent&)));


	atlasgenerator_ = new LODGenerator::CAtlasGenerator();
	decimator = NULL;
}

void cABFPannelNew::OnIdle()
{
	ui->UVViwer->Update();
	ui->MeshViwer->Update();
}

cABFPannelNew::~cABFPannelNew()
{
    delete ui;

	delete atlasgenerator_;
}

void cABFPannelNew::OnRenderUV(const SRenderContext& rc)
{
	m_pUVPreview->OnRender(rc);
}

void cABFPannelNew::OnKeyEventUV(const SKeyEvent& sKeyEvent)
{

}

void cABFPannelNew::OnMouseEventUV(const SMouseEvent& sMouseEvent)
{
	m_pUVPreview->OnMouseEvent(sMouseEvent);
}

void cABFPannelNew::OnRender(const SRenderContext& rc)
{
	m_pMeshPreview->OnRender(rc);
}

void cABFPannelNew::OnKeyEvent(const SKeyEvent& sKeyEvent)
{

}

void cABFPannelNew::OnMouseEvent(const SMouseEvent& sMouseEvent)
{
	m_pMeshPreview->OnMouseEvent(sMouseEvent);
}

void cABFPannelNew::TestABF_Slot()
{
	atlasgenerator_->testGenerate_texture_atlas();
	//OGF::AutoSeam::prepare(textureAtlasGenerator->surface());
	m_pMeshPreview->UpdateMeshMap();
}

void cABFPannelNew::TestDecimator_Slot()
{
	if (decimator == NULL)
		return;

	decimator->set_proportion_to_remove(0.125);
	decimator->apply();
	m_pMeshPreview->UpdateMeshMap();
}

void cABFPannelNew::UpdateObj(IStatObj * pObj)
{
	atlasgenerator_->setSurface(pObj);
	decimator = new LODGenerator::CDecimator(atlasgenerator_->surface());
	m_pUVPreview->SetMeshMap(atlasgenerator_->surface());
	m_pMeshPreview->SetMeshMap(atlasgenerator_->surface());
}

