// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AsyncHelper.h"

class CMaterial;

namespace MeshImporter
{
class CSceneManager;
struct SDisplayScene;
}
class CTextureManager;

class CCreateMaterialTask : public CAsyncTaskBase
{
public:
	typedef std::function<void (CCreateMaterialTask*)> Callback;

	CCreateMaterialTask();
	~CCreateMaterialTask();

	bool                  Succeeded() const { return m_bSucceeded; }

	void                  SetCallback(const Callback& callback);
	void                  SetUserData(void* pUserData);
	void                  SetMaterialFilePath(const string& filePath);
	void                  SetDisplayScene(const std::shared_ptr<MeshImporter::SDisplayScene>& pDisplayScene);
	void                  SetTextureManager(const std::shared_ptr<CTextureManager>& pTextureManager);

	_smart_ptr<CMaterial> GetMaterial();
	void*                 GetUserData();

	// CAsyncTaskBase implementation.
	virtual bool InitializeTask() override;
	virtual bool PerformTask() override;
	virtual void FinishTask(bool bTaskSucceeded) override;
private:
	Callback                                     m_callback;
	void*                                        m_pUserData;
	std::shared_ptr<MeshImporter::SDisplayScene> m_pDisplayScene;
	std::shared_ptr<CTextureManager>             m_pTextureManager;
	_smart_ptr<CMaterial>                        m_pMaterial;
	string                                       m_materialFilePath;
	bool m_bSucceeded;
};

