// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TempRcObject.h"
#include "FbxMetaData.h"
#include "DialogCommon.h"
#include "RcLoader.h"
#include "RcCaller.h"

#include <QtUtil.h>

#include <QTemporaryDir>
#include <QTemporaryFile>

namespace Private_TempRcObject
{

struct SRcPayload
{
	std::unique_ptr<QTemporaryFile> m_pMetaDataFile;
	QString                         m_compiledFilename;
	int                             m_sceneTimestamp;

	SRcPayload()
		: m_sceneTimestamp(-1)
	{
	}
};

} // namespace Private_TempRcObject

CTempRcObject::CTempRcObject(ITaskHost* pTaskHost, MeshImporter::CSceneManager* pSceneManager)
	: m_pTaskHost(pTaskHost)
	, m_pSceneManager(pSceneManager)
{
	using namespace Private_TempRcObject;

	m_pTempDir = CreateTemporaryDirectory();

	m_pRcCaller.reset(new CRcInOrderCaller());
	m_pRcCaller->SetAssignCallback(std::bind(&CTempRcObject::RcCaller_OnAssign, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	m_pRcCaller->SetDiscardCallback(&DeletePayload<SRcPayload>);
}

CTempRcObject::~CTempRcObject()
{
}

void CTempRcObject::RcCaller_OnAssign(CRcInOrderCaller* pCaller, const QString& filePath, void* pUserData)
{
	using namespace Private_TempRcObject;
	std::unique_ptr<SRcPayload> pPayload((SRcPayload*)pUserData);

	CRY_ASSERT(m_pSceneManager);

	if (pPayload->m_sceneTimestamp != m_pSceneManager->GetTimestamp())
	{
		return;
	}

	m_filePath = QtUtil::ToString(filePath);

	if (m_finalize)
	{
		m_finalize(this);
	}
}

void CTempRcObject::CreateAsync()
{
	using namespace Private_TempRcObject;

	CRY_ASSERT(m_pSceneManager);
	CRY_ASSERT(m_pTaskHost);

	auto pMetaDataFile = WriteTemporaryFile(m_pTempDir->path(), m_metaData.ToJson());
	const QString metaDataFilename = pMetaDataFile->fileName();

	std::unique_ptr<SRcPayload> pPayload(new SRcPayload());
	pPayload->m_pMetaDataFile = std::move(pMetaDataFile);
	pPayload->m_sceneTimestamp = m_pSceneManager->GetTimestamp();

	// Do not delete meta-data files to prevent Qt from reusing names.
	pPayload->m_pMetaDataFile->setAutoRemove(false);

	const string rcOptions = string().Format("%s %s", 
		CRcCaller::OptionOverwriteExtension("fbx"),
		CRcCaller::OptionVertexPositionFormat(m_metaData.bVertexPositionFormatF32));

	m_pRcCaller->CallRc(metaDataFilename, pPayload.release(), m_pTaskHost, QtUtil::ToQString(rcOptions), QtUtil::ToQString(m_message));
}

string CTempRcObject::GetFilePath() const
{
	return m_filePath;
}

void CTempRcObject::SetMessage(const string& message)
{
	m_message = message;
}

void CTempRcObject::ClearMessage()
{
	m_message.clear();
}

void CTempRcObject::SetMetaData(const FbxMetaData::SMetaData& metaData)
{
	m_metaData = metaData;
}

void CTempRcObject::SetFinalize(const Finalize& finalize)
{
	m_finalize = finalize;
}

