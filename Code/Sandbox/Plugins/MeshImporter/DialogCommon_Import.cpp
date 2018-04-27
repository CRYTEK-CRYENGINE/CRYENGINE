// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DialogCommon.h"
#include "FbxScene.h"
#include "SandboxPlugin.h"
#include "AsyncTasks.h"
#include "ImporterUtil.h"

// EditorCommon
#include "Controls/QuestionDialog.h"

// Qt
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QTemporaryFile>

namespace MeshImporter
{

namespace Private_DialogCommon
{

static const QString s_initialFilePropertyName = QStringLiteral("initialFile");

QString CopyToDir(QString filePath, const QString& dirPath)
{
	const QString newFilePath = QDir(dirPath).absoluteFilePath(QFileInfo(filePath).fileName());
	QFile::copy(filePath, newFilePath);
	return newFilePath;
}

struct SImportScenePayloadPrivate
{
	uint64                                     timestamp;
	std::unique_ptr<IImportFile>               pImportFile;
	std::unique_ptr<const SImportScenePayload> pUserData;

	SImportScenePayloadPrivate()
		: timestamp(0)
	{}
};

uint64 GetTimestamp()
{
	static uint64 clock = 0;
	return ++clock;
}

} // namespace Private_DialogCommon

// ==================================================
// CImportFileManager
// ==================================================

CImportFileManager::CImportFile::~CImportFile()
{
	if (m_filePath != m_originalFilePath)
	{
		QDir dir(QFileInfo(m_filePath).path());
		dir.removeRecursively();
	}
}

QString CImportFileManager::CImportFile::GetFilePath() const
{
	return m_filePath;
}

QString CImportFileManager::CImportFile::GetOriginalFilePath() const
{
	return m_originalFilePath;
}

QString CImportFileManager::CImportFile::GetFilename() const
{
	return QFileInfo(m_filePath).fileName();
}

std::unique_ptr<IImportFile> CImportFileManager::ImportFile(const QString& originalFilePath)
{
	using namespace Private_DialogCommon;

	QString filePath = originalFilePath;
	if (filePath.isEmpty())
	{
		return std::unique_ptr<IImportFile>();
	}

	std::unique_ptr<CImportFile> pImportFile(new CImportFile());

	// Is file outside game directory?
	if (!IsAssetPath(QtUtil::ToString(filePath)))
	{
		// Create temporary file inside game directory.
		std::unique_ptr<QTemporaryDir> pTmpDir(CreateTemporaryDirectory());
		filePath = CopyToDir(filePath, pTmpDir->path());
		pImportFile->m_pTemporaryDir = std::move(pTmpDir);

		if (filePath.isEmpty())
		{
			return std::unique_ptr<IImportFile>();
		}
	}

	pImportFile->m_filePath = filePath;
	pImportFile->m_originalFilePath = originalFilePath;
	return std::move(pImportFile);
}

// ==================================================
// CSceneManager
// ==================================================

CSceneManager::~CSceneManager()
{
	UnloadScene();
}

void CSceneManager::ImportFile(const QString& originalFilePath, const SImportScenePayload* pUserData, ITaskHost* pTaskHost)
{
	using namespace Private_DialogCommon;

	CFbxToolPlugin::GetInstance()->SetPersonalizationProperty(s_initialFilePropertyName, originalFilePath);

	std::unique_ptr<IImportFile> pImportFile = std::move(CImportFileManager::ImportFile(originalFilePath));
	if (!pImportFile)
	{
		return;
	}

	const QString filePath = pImportFile->GetFilePath();

	std::unique_ptr<SImportScenePayloadPrivate> pPayload(new SImportScenePayloadPrivate());
	pPayload->timestamp = GetTimestamp();
	pPayload->pImportFile = std::move(pImportFile);
	pPayload->pUserData.reset(pUserData);

	CAsyncImportSceneTask* pTask = new CAsyncImportSceneTask();
	pTask->SetUserSettings(GetUserSettings());
	pTask->SetFilePath(filePath);
	pTask->SetCallback(std::bind(&CSceneManager::OnSceneImported, this, std::placeholders::_1));
	pTask->SetUserData(pPayload.release());
	pTaskHost->RunTask(pTask);
}

FbxMetaData::SSceneUserSettings CSceneManager::GetUserSettings() const
{
	FbxMetaData::SSceneUserSettings settings;
	settings.bRemoveDegeneratePolygons = true;
	settings.bTriangulate = false;
	settings.generateNormals = FbxMetaData::EGenerateNormals::Never;
	return settings;
}

void CSceneManager::UnloadScene()
{
	if (m_pDisplayScene)
	{
		for (auto& callback : m_unloadSceneCallbacks)
		{
			if (callback)
			{
				callback();
			}
		}

		m_pDisplayScene.reset();
	}
}

void CSceneManager::OnSceneImported(CAsyncImportSceneTask* pTask)
{
	using namespace Private_DialogCommon;

	std::unique_ptr<SImportScenePayloadPrivate> pPayload((SImportScenePayloadPrivate*)pTask->GetUserData());
	CRY_ASSERT(pPayload && pPayload->pImportFile);
	if (pTask->Succeeded())
	{
		if (m_pDisplayScene && m_pDisplayScene->m_timestamp > pPayload->timestamp)
		{
			return;
		}

		UnloadScene();

		m_pDisplayScene = std::make_shared<SDisplayScene>();
		m_pDisplayScene->m_timestamp = pPayload->timestamp;
		m_pDisplayScene->m_pImportFile = std::move(pPayload->pImportFile);
		m_pDisplayScene->m_pScene = std::move(pTask->GetScene());

		for (auto& callback : m_assignSceneCallbacks)
		{
			if (callback)
			{
				callback(pPayload->pUserData.get());
			}
		}
	}
}

const FbxTool::CScene* CSceneManager::GetScene() const
{
	return m_pDisplayScene ? m_pDisplayScene->m_pScene.get() : nullptr;
}

FbxTool::CScene* CSceneManager::GetScene()
{
	return m_pDisplayScene ? m_pDisplayScene->m_pScene.get() : nullptr;
}

const IImportFile* CSceneManager::GetImportFile() const
{
	return m_pDisplayScene ? m_pDisplayScene->m_pImportFile.get() : nullptr;
}

} // namespace MeshImporter

