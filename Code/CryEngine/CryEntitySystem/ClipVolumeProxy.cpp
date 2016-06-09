// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "ClipVolumeProxy.h"
#include <Cry3DEngine/CGF/CGFContent.h>

//////////////////////////////////////////////////////////////////////////
CClipVolumeProxy::CClipVolumeProxy()
	: m_pEntity(NULL)
	, m_pClipVolume(NULL)
	, m_pBspTree(NULL)
	, m_nFlags(IClipVolume::eClipVolumeAffectedBySun)
{
}

void CClipVolumeProxy::ProcessEvent(SEntityEvent& event)
{
	if (m_pEntity != NULL)
	{
		if (event.event == ENTITY_EVENT_XFORM ||
		    event.event == ENTITY_EVENT_HIDE ||
		    event.event == ENTITY_EVENT_UNHIDE)
		{
			gEnv->p3DEngine->UpdateClipVolume(m_pClipVolume, m_pRenderMesh, m_pBspTree, m_pEntity->GetWorldTM(), !m_pEntity->IsHidden(), m_nFlags, m_pEntity->GetName());
		}
	}
}

void CClipVolumeProxy::Release()
{
	if (m_pClipVolume)
	{
		gEnv->p3DEngine->DeleteClipVolume(m_pClipVolume);
		m_pClipVolume = NULL;
	}

	m_pRenderMesh = NULL;
	gEnv->pEntitySystem->ReleaseBSPTree3D(m_pBspTree);

	m_GeometryFileName = "";
}

void CClipVolumeProxy::UpdateRenderMesh(IRenderMesh* pRenderMesh, const DynArray<Vec3>& meshFaces)
{
	m_pRenderMesh = pRenderMesh;
	gEnv->pEntitySystem->ReleaseBSPTree3D(m_pBspTree);

	const size_t nFaceCount = meshFaces.size() / 3;
	if (nFaceCount > 0)
	{
		IBSPTree3D::FaceList faceList;
		faceList.reserve(nFaceCount);

		for (int i = 0; i < meshFaces.size(); i += 3)
		{
			IBSPTree3D::CFace face;
			face.push_back(meshFaces[i + 0]);
			face.push_back(meshFaces[i + 1]);
			face.push_back(meshFaces[i + 2]);

			faceList.push_back(face);
		}

		m_pBspTree = gEnv->pEntitySystem->CreateBSPTree3D(faceList);
	}

	if (m_pEntity && m_pClipVolume)
		gEnv->p3DEngine->UpdateClipVolume(m_pClipVolume, m_pRenderMesh, m_pBspTree, m_pEntity->GetWorldTM(), !m_pEntity->IsHidden(), m_nFlags, m_pEntity->GetName());
}

void CClipVolumeProxy::SetProperties(bool bIgnoresOutdoorAO)
{
	if (m_pEntity && m_pClipVolume)
	{
		m_nFlags &= ~IClipVolume::eClipVolumeIgnoreOutdoorAO;
		m_nFlags |= bIgnoresOutdoorAO ? IClipVolume::eClipVolumeIgnoreOutdoorAO : 0;

		gEnv->p3DEngine->UpdateClipVolume(m_pClipVolume, m_pRenderMesh, m_pBspTree, m_pEntity->GetWorldTM(), !m_pEntity->IsHidden(), m_nFlags, m_pEntity->GetName());
	}
}

bool CClipVolumeProxy::Init(IEntity* pEntity, SEntitySpawnParams& params)
{
	m_pEntity = pEntity;
	m_pClipVolume = gEnv->p3DEngine->CreateClipVolume();

	if (params.pUserData)
		m_GeometryFileName = static_cast<const char*>(params.pUserData);

	return true;
}

void CClipVolumeProxy::Reload(IEntity* pEntity, SEntitySpawnParams& params)
{
	Release();
	m_pEntity = pEntity;
}

void CClipVolumeProxy::SerializeXML(XmlNodeRef& entityNodeXML, bool loading)
{
	if (loading)
	{
		LOADING_TIME_PROFILE_SECTION;

		if (XmlNodeRef pVolumeNode = entityNodeXML->findChild("ClipVolume"))
		{
			const char* szFileName = NULL;
			if (pVolumeNode->getAttr("GeometryFileName", &szFileName))
			{
				// replace %level% by level path
				char szFilePath[_MAX_PATH];
				const int nAliasNameLen = sizeof("%level%") - 1;

				cry_strcpy(szFilePath, gEnv->p3DEngine->GetLevelFilePath(szFileName + nAliasNameLen));

				if (m_pEntity && LoadFromFile(szFilePath))
					gEnv->p3DEngine->UpdateClipVolume(m_pClipVolume, m_pRenderMesh, m_pBspTree, m_pEntity->GetWorldTM(), !m_pEntity->IsHidden(), m_nFlags, m_pEntity->GetName());
			}
		}
	}
	else
	{
		XmlNodeRef volumeNode = entityNodeXML->newChild("ClipVolume");
		volumeNode->setAttr("GeometryFileName", m_GeometryFileName);
	}
}

bool CClipVolumeProxy::LoadFromFile(const char* szFilePath)
{
	assert(!m_pRenderMesh && !m_pBspTree);

	if (FILE* pCgfFile = gEnv->pCryPak->FOpen(szFilePath, "rb"))
	{
		const size_t nFileSize = gEnv->pCryPak->FGetSize(pCgfFile);
		uint8* pBuffer = new uint8[nFileSize];

		gEnv->pCryPak->FReadRawAll(pBuffer, nFileSize, pCgfFile);
		gEnv->pCryPak->FClose(pCgfFile);

		_smart_ptr<IChunkFile> pChunkFile = gEnv->p3DEngine->CreateChunkFile(true);
		if (pChunkFile->ReadFromMemory(pBuffer, nFileSize))
		{
			if (IChunkFile::ChunkDesc* pBspTreeDataChunk = pChunkFile->FindChunkByType(ChunkType_BspTreeData))
			{
				m_pBspTree = gEnv->pEntitySystem->CreateBSPTree3D(IBSPTree3D::FaceList());
				m_pBspTree->ReadFromBuffer(static_cast<uint8*>(pBspTreeDataChunk->data));
			}
			else
			{
				CryLog("ClipVolume '%s' runtime collision data not found. Please reexport the level.", m_pEntity->GetName());
			}
		}

		if (gEnv->pRenderer)
		{
			CContentCGF* pCgfContent = gEnv->p3DEngine->CreateChunkfileContent(szFilePath);
			if (gEnv->p3DEngine->LoadChunkFileContentFromMem(pCgfContent, pBuffer, nFileSize, 0, false))
			{
				for (int i = 0; i < pCgfContent->GetNodeCount(); ++i)
				{
					CNodeCGF* pNode = pCgfContent->GetNode(i);
					if (pNode->type == CNodeCGF::NODE_MESH && pNode->pMesh)
					{
						m_pRenderMesh = gEnv->pRenderer->CreateRenderMesh("ClipVolume", szFilePath, NULL, eRMT_Static);
						m_pRenderMesh->SetMesh(*pNode->pMesh, 0, FSM_CREATE_DEVICE_MESH, NULL, false);

						break;
					}
				}
			}
			gEnv->p3DEngine->ReleaseChunkfileContent(pCgfContent);
		}

		delete[] pBuffer;
	}

	return m_pRenderMesh && m_pBspTree;
}

bool CClipVolumeProxy::GetSignature(TSerialize signature)
{
	signature.BeginGroup("ClipVolume");
	signature.EndGroup();
	return true;
}

void CClipVolumeProxy::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(m_GeometryFileName);
	pSizer->AddObject(m_pRenderMesh);
	pSizer->AddObject(m_pBspTree);
}
