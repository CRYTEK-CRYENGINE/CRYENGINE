// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Async tasks used by FbxTool.
#pragma once
#include "AsyncHelper.h"
#include "FbxScene.h"
#include "FbxMetaData.h"
#include "MainDialog.h"

#include <CrySystem/ISystem.h>
#include <CrySystem/ILog.h>

// Async FBX scene import task
class CAsyncImportSceneTask : public CAsyncTaskBase, FbxTool::ICallbacks
{
public:
	typedef std::function<void (CAsyncImportSceneTask*)> Callback;

	CAsyncImportSceneTask();

	void                                   SetUserSettings(const FbxMetaData::SSceneUserSettings& userSettings);
	void                                   SetFilePath(const QString& filePath);
	void                                   SetCallback(const Callback& callback);
	void                                   SetUserData(void* pUserData);

	std::unique_ptr<FbxTool::CScene>       GetScene();
	void*                                  GetUserData();

	bool                                   Succeeded() const;
	const FbxMetaData::SSceneUserSettings& GetUserSettings() const;
	const QString&                         GetFilePath() const;

	// CAsyncTaskBase
	virtual bool PerformTask() override;
	virtual void FinishTask(bool bResult) override;
	// ~CAsyncTaskBase

	// ICallbacks
	virtual void OnProgressMessage(const char* szMessage) override;
	virtual void OnProgressPercentage(int percentage) override;
	virtual void OnWarning(const char* szMessage) override;
	virtual void OnError(const char* szMessage) override;
	// ~ICallbacks
private:
	static CryCriticalSection m_fbxSdkMutex;

	FbxMetaData::SSceneUserSettings  m_userSettings;
	QString                          m_filePath;
	bool                             m_bSucceeded;
	std::unique_ptr<FbxTool::CScene> m_resultScene;
	QString                          m_lastProgressMessage;
	int                              m_lastProgressPercentage;
	Callback                         m_callback;
	void*                            m_pUserData;
};

