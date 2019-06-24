#include "StdAfx.h"

#include "ABFPannel.h"
#include "../UIs/ui_cabfpannel.h"

cABFPannel::cABFPannel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::cABFPannel)
{
    ui->setupUi(this);

//	connect(ui->TestABFButton, SIGNAL(clicked()), this, SLOT(TestABF_Slot()));

//	m_pUVPreview = new CMeshBakerUVPreview(ui->UVViwer);
//	m_pUVPreview->SetRenderMesh(false);

// 	connect(ui->UVViwer, SIGNAL(SignalRender(const SRenderContext&)), this, SLOT(OnRenderUV(const SRenderContext&)));
// 	connect(ui->UVViwer, SIGNAL(SignalMouse(const SMouseEvent&)), this, SLOT(OnMouseEventUV(const SMouseEvent&)));
// 	connect(ui->UVViwer, SIGNAL(SignalKey(const SKeyEvent&)), this, SLOT(OnKeyEventUV(const SKeyEvent&)));

	
//	m_pMeshPreview = new CMeshBakerUVPreview(ui->MeshViwer);

// 	connect(ui->MeshViwer, SIGNAL(SignalRender(const SRenderContext&)), this, SLOT(OnRender(const SRenderContext&)));
// 	connect(ui->MeshViwer, SIGNAL(SignalMouse(const SMouseEvent&)), this, SLOT(OnMouseEvent(const SMouseEvent&)));
// 	connect(ui->MeshViwer, SIGNAL(SignalKey(const SKeyEvent&)), this, SLOT(OnKeyEvent(const SKeyEvent&)));


//	textureAtlasGenerator = new LODGenerator::TextureAtlasGenerator();
}

void cABFPannel::OnIdle()
{
	ui->UVViwer->Update();
	ui->MeshViwer->Update();
}

cABFPannel::~cABFPannel()
{
    delete ui;

//	delete textureAtlasGenerator;
}

// void cABFPannel::OnRenderUV(const SRenderContext& rc)
// {
// //	m_pUVPreview->OnRender(rc);
// }

// void cABFPannel::OnKeyEventUV(const SKeyEvent& sKeyEvent)
// {
// //	m_pUVPreview->OnKeyEvent(sKeyEvent);
// }

// void cABFPannel::OnMouseEventUV(const SMouseEvent& sMouseEvent)
// {
// //	m_pUVPreview->OnMouseEvent(sMouseEvent);
// }

// void cABFPannel::OnRender(const SRenderContext& rc)
// {
// //	m_pMeshPreview->OnRender(rc);
// }

// void cABFPannel::OnKeyEvent(const SKeyEvent& sKeyEvent)
// {
// //	m_pMeshPreview->OnKeyEvent(sKeyEvent);
// }

// void cABFPannel::OnMouseEvent(const SMouseEvent& sMouseEvent)
// {
// //	m_pMeshPreview->OnMouseEvent(sMouseEvent);
// }

void cABFPannel::TestABF_Slot()
{
// 	textureAtlasGenerator->testGenerate_texture_atlas();
// 	//OGF::AutoSeam::prepare(textureAtlasGenerator->surface());
// 	m_pMeshPreview->UpdateMeshMap();
}

void cABFPannel::UpdateObj(IStatObj * pObj)
{
// 	textureAtlasGenerator->setSurface(pObj);
// 	//OGF::AutoSeam::prepare(textureAtlasGenerator->surface());	
// 	m_pUVPreview->SetMeshMap(textureAtlasGenerator->surface());
// 	m_pMeshPreview->SetMeshMap(textureAtlasGenerator->surface());
}

