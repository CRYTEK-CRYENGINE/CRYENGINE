// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MaterialGenerator.h"
#include "CreateMaterialTask.h"
#include "DialogCommon.h"  // CSceneManager.

#include <Cry3DEngine/I3DEngine.h>
#include <Material/Material.h>

#include <QTemporaryDir>

CMaterialGenerator::CMaterialGenerator()
	: m_pTaskHost(nullptr)
	, m_pSceneManager(nullptr)
	, m_pTextureManager(nullptr)
{
}

void CMaterialGenerator::SetTaskHost(ITaskHost* pTaskHost)
{
	m_pTaskHost = pTaskHost;
}

void CMaterialGenerator::SetSceneManager(MeshImporter::CSceneManager* pSceneManager)
{
	m_pSceneManager = pSceneManager;
}

void CMaterialGenerator::SetTextureManager(const std::shared_ptr<CTextureManager>& pTextureManager)
{
	m_pTextureManager = pTextureManager;
}

void CMaterialGenerator::GenerateMaterialInternal(const string& mtlFilePath, void* pUserData)
{
	CCreateMaterialTask* pTask = new CCreateMaterialTask();
	pTask->SetMaterialFilePath(mtlFilePath);
	pTask->SetCallback(std::bind(&CMaterialGenerator::OnMaterialCreated, this, std::placeholders::_1));
	pTask->SetDisplayScene(m_pSceneManager->GetDisplayScene());
	pTask->SetTextureManager(m_pTextureManager);
	pTask->SetUserData(pUserData);

	if (m_pTaskHost)
	{
		m_pTaskHost->RunTask(pTask);
	}
	else
	{
		CAsyncTaskBase::CallBlocking(pTask);
	}
}

void CMaterialGenerator::GenerateMaterial(void* pUserData)
{
	GenerateMaterialInternal("", pUserData);
}

void CMaterialGenerator::GenerateMaterial(const string& mtlFilePath, void* pUserData)
{
	GenerateMaterialInternal(mtlFilePath, pUserData);
}

void CMaterialGenerator::GenerateTemporaryMaterial(void* pUserData)
{
	QTemporaryDir* pTempDir = CreateTemporaryDirectory().release();

	QString materialFilePath = pTempDir->path() + "/mi_caf_material.mtl";

	GenerateMaterialInternal(QtUtil::ToString(materialFilePath), pUserData);
}

void CMaterialGenerator::OnMaterialCreated(CCreateMaterialTask* pTask)
{
	if (!pTask->Succeeded())
	{
		return;
	}

	_smart_ptr<CMaterial> pMtl = pTask->GetMaterial();

	if (!pMtl)
	{
		return;
	}

	sigMaterialGenerated(pMtl, pTask->GetUserData()); // TODO: Memory leak!
}
