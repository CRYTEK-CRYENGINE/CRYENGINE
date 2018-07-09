// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySandbox/CrySignal.h>
#include <CryString/CryString.h>

#include <memory>

namespace MeshImporter
{

class CSceneManager;

} // namespace MeshImporter

class CCreateMaterialTask;
class CTextureManager;
struct ITaskHost;

class CMaterial;

class CMaterialGenerator
{
public:
	CMaterialGenerator();

	void SetTaskHost(ITaskHost* pTaskHost);
	void SetSceneManager(MeshImporter::CSceneManager* pSceneManager);
	void SetTextureManager(const std::shared_ptr<CTextureManager>& pTextureManager);

	void GenerateMaterial(void* pUserData = nullptr);
	void GenerateMaterial(const string& mtlFilePath, void* pUserData = nullptr);
	void GenerateTemporaryMaterial(void* pUserData = nullptr);

	CCrySignal<void(CMaterial* pMaterial, void* pUserData)> sigMaterialGenerated;
private:
	void GenerateMaterialInternal(const string& mtlFilePath, void* pUserData);
	void OnMaterialCreated(CCreateMaterialTask* pTask);

private:
	ITaskHost* m_pTaskHost;
	MeshImporter::CSceneManager* m_pSceneManager;
	std::shared_ptr<CTextureManager> m_pTextureManager;
};
