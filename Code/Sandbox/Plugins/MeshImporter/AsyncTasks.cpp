// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AsyncTasks.h"

//--------------------------------------------------
// CAsyncImportFileTask
//--------------------------------------------------

CAsyncImportSceneTask::CAsyncImportSceneTask()
	: m_bSucceeded(false)
	, m_lastProgressMessage(QStringLiteral("Importing file..."))
	, m_lastProgressPercentage(0)
	, m_pUserData(nullptr)
{}

void CAsyncImportSceneTask::SetUserSettings(const FbxMetaData::SSceneUserSettings& userSettings)
{
	m_userSettings = userSettings;
}

void CAsyncImportSceneTask::SetFilePath(const QString& filePath)
{
	m_filePath = filePath;
}

void CAsyncImportSceneTask::SetCallback(const Callback& callback)
{
	m_callback = callback;
}

void CAsyncImportSceneTask::SetUserData(void* pUserData)
{
	m_pUserData = pUserData;
}

std::unique_ptr<FbxTool::CScene> CAsyncImportSceneTask::GetScene()
{
	return std::move(m_resultScene);
}

void* CAsyncImportSceneTask::GetUserData()
{
	return m_pUserData;
}

bool CAsyncImportSceneTask::Succeeded() const
{
	return m_bSucceeded;
}

const FbxMetaData::SSceneUserSettings& CAsyncImportSceneTask::GetUserSettings() const
{
	return m_userSettings;
}

const QString& CAsyncImportSceneTask::GetFilePath() const
{
	return m_filePath;
}

bool CAsyncImportSceneTask::PerformTask()
{
	FbxTool::SFileImportDescriptor desc;
	desc.bRemoveDegeneratePolygons = m_userSettings.bRemoveDegeneratePolygons;
	desc.bTriangulate = m_userSettings.bTriangulate;
	desc.generateNormals = (FbxTool::EGenerateNormals)m_userSettings.generateNormals;
	desc.filePath = QtUtil::ToString(m_filePath);
	desc.pCallbacks = this;

	m_resultScene = FbxTool::CScene::ImportFile(desc); // This is supposed to be the only place where CScene::Import() is called.

	return m_resultScene != nullptr;
}

void CAsyncImportSceneTask::FinishTask(bool bTaskSuceeded)
{
	m_bSucceeded = bTaskSuceeded;
	if (m_callback)
	{
		m_callback(this);
	}
	delete this;
}

void CAsyncImportSceneTask::OnProgressMessage(const char* szMessage)
{
	m_lastProgressMessage = QString::fromLatin1(szMessage);
	UpdateProgress(m_lastProgressMessage, m_lastProgressPercentage);
}

void CAsyncImportSceneTask::OnProgressPercentage(int percentage)
{
	m_lastProgressPercentage = percentage;
	UpdateProgress(m_lastProgressMessage, m_lastProgressPercentage);
}

void CAsyncImportSceneTask::OnWarning(const char* szMessage)
{
	gEnv->pLog->LogWarning("%s", szMessage);
}

void CAsyncImportSceneTask::OnError(const char* szMessage)
{
	gEnv->pLog->LogError("%s", szMessage);
}

