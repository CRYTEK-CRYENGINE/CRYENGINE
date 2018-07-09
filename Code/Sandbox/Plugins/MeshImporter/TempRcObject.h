// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>
#include <CrySandbox/CrySignal.h>

#include <functional>
#include <memory>

class CRcInOrderCaller;
struct ITaskHost;

class QTemporaryDir;

namespace MeshImporter
{

class CSceneManager;

} // namespace MeshImporter

namespace FbxMetaData
{

struct SMetaData;

} // namespace FbxMetaData

//! CTempRcObject is used to create temporary asset files from FBX, such as CGF, SKIN, and CHR files.
class CTempRcObject
{
public:
	typedef std::function<void(const CTempRcObject*)> Finalize;

public:
	CTempRcObject(ITaskHost* pTaskHost, MeshImporter::CSceneManager* pSceneManager);
	~CTempRcObject();

	void SetMessage(const string& message);
	void ClearMessage();

	void SetMetaData(const FbxMetaData::SMetaData& metaData);
	void SetFinalize(const Finalize& finalize);

	void CreateAsync();

	string GetFilePath() const;
private:
	void RcCaller_OnAssign(CRcInOrderCaller* pCaller, const QString& filePath, void* pUserData);

	std::unique_ptr<CRcInOrderCaller> m_pRcCaller;
	std::unique_ptr<QTemporaryDir> m_pTempDir;
	MeshImporter::CSceneManager* m_pSceneManager;
	ITaskHost* m_pTaskHost;
	string m_filePath;

	string m_message;
	FbxMetaData::SMetaData m_metaData;
	Finalize m_finalize;
};

