// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "MaterialPanel.h"
#include "MaterialSettings.h"
#include "MaterialView.h"
#include "MaterialModel.h"
#include "MaterialElement.h"
#include "TextureManager.h"
#include "TextureModel.h"
#include "TextureView.h"
#include "MaterialHelpers.h"
#include "CreateMaterialTask.h"
#include "DialogCommon.h"  // CSceneManager, CreateTemporaryDirectory
#include "MaterialGenerator/MaterialGenerator.h"

#include <Cry3DEngine/I3DEngine.h>
#include <Material/Material.h>

#include <Serialization/QPropertyTree/QPropertyTree.h>

#include <QColorDialog>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QTemporaryDir>
#include <QVBoxLayout>

CMaterialPanel::CMaterialPanel(MeshImporter::CSceneManager* pSceneManager, ITaskHost* pTaskHost, QWidget* pParent /* = nullptr */)
	: QWidget(pParent)
	, m_pSceneManager(pSceneManager)
	, m_pTaskHost(pTaskHost)
	, m_pMaterialSettings(new CMaterialSettings())
	, m_pTextureManager(new CTextureManager())
	, m_bApplyingMaterials(false)
{
	m_pMaterialGenerator.reset(new CMaterialGenerator());
	m_pMaterialGenerator->SetSceneManager(m_pSceneManager);
	m_pMaterialGenerator->SetTextureManager(m_pTextureManager);
	m_pMaterialGenerator->SetTaskHost(m_pTaskHost);
	m_pMaterialGenerator->sigMaterialGenerated.Connect(this, &CMaterialPanel::OnMaterialCreated);

	SetupUi();
}

CMaterialPanel::~CMaterialPanel()
{
}

void CMaterialPanel::AssignScene(const MeshImporter::SImportScenePayload* pPayload)
{
	if (m_pMaterialList->model()->GetScene() == m_pSceneManager->GetScene())
	{
		return;  // Do not assign same scene twice.
	}

	m_pMaterialList->model()->SetScene(m_pSceneManager->GetScene());

	const string materialName = (pPayload && pPayload->pMetaData) ? pPayload->pMetaData->materialFilename : "";
	m_pMaterialSettings->SetMaterial(materialName);

	// Fill textures.
	m_pTextureManager->Init(*m_pSceneManager);
	m_pTextureModel->Reset();

	m_pMaterialList->resizeColumnToContents(CMaterialModel::eColumnType_Color);

	ApplyMaterial();
	m_pMaterialSettingsTree->revert();
	m_pMaterialSettingsTree->setEnabled(true);
}

void CMaterialPanel::UnloadScene()
{
	m_pMaterialList->model()->ClearScene();
	m_pMaterialSettingsTree->setEnabled(false);
}

CMaterialSettings* CMaterialPanel::GetMaterialSettings()
{
	return m_pMaterialSettings.get();
}

QPropertyTree* CMaterialPanel::GetMaterialSettingsTree()
{
	return m_pMaterialSettingsTree;
}

CSortedMaterialModel* CMaterialPanel::GetMaterialModel()
{
	return m_pMaterialList->model();
}

CMaterialView* CMaterialPanel::GetMaterialView()
{
	return m_pMaterialList;
}

void CMaterialPanel::ApplyMaterial()
{
	if (m_pMaterialList->model()->GetScene())
	{
		ApplyMaterial_Int();
		m_pMaterialList->model()->Reset();
	}
}

std::shared_ptr<CTextureManager> CMaterialPanel::GetTextureManager()
{
	return m_pTextureManager;
}

CTextureModel* CMaterialPanel::GetTextureModel()
{
	return m_pTextureModel.get();
}

void CMaterialPanel::ApplyMaterial_Int()
{
	if (!m_bApplyingMaterials)
	{
		m_bApplyingMaterials = true;

		const string materialName = m_pMaterialSettings->GetMaterial() ? m_pMaterialSettings->GetMaterial()->GetFilename() : "";
		auto loadedMaterialNames = GetSubMaterialList(materialName);
		m_pMaterialList->model()->ApplyMaterials(std::move(loadedMaterialNames), true);

		m_bApplyingMaterials = false;
	}
}

void CMaterialPanel::GenerateMaterial(void* pUserData)
{
	m_pMaterialGenerator->GenerateMaterial(pUserData);
}

void CMaterialPanel::GenerateMaterial(const QString& mtlFilePath, void* pUserData)
{
	m_pMaterialGenerator->GenerateMaterial(QtUtil::ToString(mtlFilePath), pUserData);
}

void CMaterialPanel::GenerateTemporaryMaterial(void* pUserData)
{
	m_pMaterialGenerator->GenerateTemporaryMaterial(pUserData);
}

void CMaterialPanel::OnMaterialCreated(CMaterial* pMtl, void* pUserData)
{
	if (!pMtl)
	{
		return;
	}

	m_pMaterialSettings->SetMaterial(string(pMtl->GetName()));
	m_pMaterialSettingsTree->revert();

	ApplyMaterial();

	SigGenerateMaterial(pUserData); // TODO: Memory leak!
}

_smart_ptr<CMaterial> CMaterialPanel::GetMaterial()
{
	return m_pMaterialSettings->GetMaterial();
}

IMaterial* CMaterialPanel::GetMatInfo()
{
	CMaterial* const pMat = m_pMaterialSettings->GetMaterial();
	return pMat ? pMat->GetMatInfo() : nullptr;
}

void CMaterialPanel::SetupUi()
{
	m_pMaterialSettingsTree = new QPropertyTree();
	m_pMaterialSettingsTree->setSizeToContent(true);
	m_pMaterialSettingsTree->setEnabled(false);
	m_pMaterialSettingsTree->attach(Serialization::SStruct(*m_pMaterialSettings));
	m_pMaterialSettingsTree->expandAll();

	// Materials.

	m_pMaterialModel.reset(new CSortedMaterialModel());

	m_pMaterialList = new CMaterialView(m_pMaterialModel.get());
	m_pMaterialList->setStatusTip(tr("MainDialog", "Displays material mapping"));

	CMaterialViewContainer* pMaterialViewContainer = new CMaterialViewContainer(m_pMaterialModel.get(), m_pMaterialList);

	// Create texture model and view.

	m_pTextureModel.reset(new CTextureModel(m_pTextureManager.get()));

	CTextureView* const pTextureView = new CTextureView();
	pTextureView->SetModel(m_pTextureModel.get());

	QSplitter* const pSplitter = new QSplitter(Qt::Vertical);
	{
		QWidget* pDummy = new QWidget();
		QVBoxLayout* pLayout = new QVBoxLayout();
		pLayout->setContentsMargins(0, 0, 0, 0);
		pLayout->setMargin(0);

		pLayout->addWidget(m_pMaterialSettingsTree);

		if (m_pMaterialList)
		{
			pLayout->addWidget(pMaterialViewContainer);
		}

		pDummy->setLayout(pLayout);
		pSplitter->addWidget(pDummy);
	}

	{
		QWidget* pDummy = new QWidget();
		QVBoxLayout* pLayout = new QVBoxLayout();
		pLayout->setContentsMargins(0, 0, 0, 0);
		pLayout->setMargin(0);

		m_pGenerateMaterialButton = new QPushButton(tr("Generate material"));
		connect(m_pGenerateMaterialButton, &QPushButton::clicked, [this]() { GenerateMaterial(); });

		pLayout->addWidget(m_pGenerateMaterialButton);

		if (pTextureView)
		{
			pLayout->addWidget(pTextureView);
		}

		pDummy->setLayout(pLayout);
		pSplitter->addWidget(pDummy);
	}

	QVBoxLayout* const pLayout = new QVBoxLayout();
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->addWidget(pSplitter);
	setLayout(pLayout);
}

