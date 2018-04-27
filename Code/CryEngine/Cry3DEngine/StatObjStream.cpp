// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   statobjconstr.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: loading
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "IndexedMesh.h"
#include "../RenderDll/Common/Shadow_Renderer.h"
#include <CryRenderer/IRenderer.h>
#include <CryMemory/CrySizer.h>

#include "CGF/CGFLoader.h"
#include "CGF/CGFSaver.h"
#include "CGF/ReadOnlyChunkFile.h"

#define GEOM_INFO_FILE_EXT "ginfo"
#define MESH_NAME_FOR_MAIN "main"

extern const char* stristr(const char* szString, const char* szSubstring);

extern void        TransformMesh(CMesh& mesh, Matrix34 tm);

#if !defined (_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
float CStatObj::s_fStreamingTime = 0.0f;
int CStatObj::s_nBandwidth = 0;
#endif

void CStatObj::StreamAsyncOnComplete(IReadStream* pStream, unsigned nError)
{
	FUNCTION_PROFILER_3DENGINE;

	if (nError == ERROR_CANT_OPEN_FILE && m_bHasStreamOnlyCGF)
	{
		// Deliberately ignore
	}
	else if (pStream->IsError())
	{
		// file was not loaded successfully
		m_eStreamingStatus = ecss_Ready;
		if (pStream->GetError() != ERROR_USER_ABORT && pStream->GetError() != ERROR_ABORTED_ON_SHUTDOWN)
			Error("CStatObj::StreamAsyncOnComplete: Error loading CGF: %s Error: %s", m_szFileName.c_str(), pStream->GetErrorName());
	}
	else
	{
#if !defined (_RELEASE)
		float timeinseconds = (gEnv->pTimer->GetCurrTime() - m_fStreamingStart);
		s_nBandwidth += pStream->GetBytesRead();
		s_fStreamingTime += timeinseconds;
		m_fStreamingStart = 0.0f;
#endif

		if (!LoadStreamRenderMeshes(0, pStream->GetBuffer(), pStream->GetBytesRead(), strstr(m_szFileName.c_str(), "_lod") != NULL))
		{
			Error("CStatObj::StreamOnComplete_LoadCGF_FromMemBlock, filename=%s", m_szFileName.c_str());
		}
	}

	pStream->FreeTemporaryMemory(); // We dont need internal stream loaded buffer anymore.
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	FUNCTION_PROFILER_3DENGINE;

	// Synchronous callback.

	if (nError == ERROR_CANT_OPEN_FILE && m_bHasStreamOnlyCGF)
	{
		m_bHasStreamOnlyCGF = 0;
		m_eStreamingStatus = ecss_NotLoaded;
	}
	else if (pStream->IsError())
	{
		// file was not loaded successfully
		if (pStream->GetError() != ERROR_USER_ABORT && pStream->GetError() != ERROR_ABORTED_ON_SHUTDOWN)
			Error("CStatObj::StreamOnComplete: Error loading CGF: %s Error: %s", m_szFileName.c_str(), pStream->GetErrorName());

		m_eStreamingStatus = ecss_Ready;
	}
	else
	{
		CommitStreamRenderMeshes();

		//////////////////////////////////////////////////////////////////////////
		// Forces vegetation sprites to be marked for regeneration from new mesh.
		//////////////////////////////////////////////////////////////////////////
		for (int nSID = 0; nSID < m_pObjManager->m_lstStaticTypes.Count(); nSID++)
		{
			PodArray<StatInstGroup>& rGroupTable = m_pObjManager->m_lstStaticTypes;
			for (int nGroupId = 0, nGroups = rGroupTable.Count(); nGroupId < nGroups; nGroupId++)
			{
				StatInstGroup& rGroup = rGroupTable[nGroupId];
				if (rGroup.pStatObj == this)
				{
					rGroup.Update(GetCVars(), Get3DEngine()->GetGeomDetailScreenRes());
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////

#ifdef OBJMAN_STREAM_STATS
		if (m_pStreamListener)
			m_pStreamListener->OnReceivedStreamedObject(static_cast<IStreamable*>(this));
#endif

		m_eStreamingStatus = ecss_Ready;
	}

	m_pReadStream = 0;
}

void CStatObj::IncrementModificationId()
{
	m_nModificationId++;

	if (m_pParentObject)
		m_pParentObject->IncrementModificationId();

	if (m_pLod0)
		m_pLod0->IncrementModificationId();
}

void CStatObj::GetStreamFilePath(stack_string& strOut)
{
	strOut = m_szFileName.c_str();
	strOut.append("m");
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::StartStreaming(bool bFinishNow, IReadStream_AutoPtr* ppStream)
{
	assert(!m_pParentObject);

	assert(m_eStreamingStatus == ecss_NotLoaded);

	if (m_eStreamingStatus != ecss_NotLoaded)
		return;

	// start streaming
	StreamReadParams params;
	params.dwUserData = 0;
	params.nSize = 0;
	params.pBuffer = NULL;
	params.nLoadTime = 10000;
	params.nMaxLoadTime = 10000;
	if (bFinishNow)
	{
		params.ePriority = estpUrgent;
	}

	if (m_szFileName.empty())
	{
		assert(!"CStatObj::StartStreaming: CGF name is empty");
		m_eStreamingStatus = ecss_Ready;
		if (ppStream) *ppStream = NULL;
		return;
	}

#if !defined (_RELEASE)
	m_fStreamingStart = gEnv->pTimer->GetCurrTime();
#endif //!defined (_RELEASE)
	const char* path = m_szFileName.c_str();

	stack_string streamPath;
	if (m_bHasStreamOnlyCGF)
	{
		GetStreamFilePath(streamPath);
		path = streamPath.c_str();
	}

	m_pReadStream = GetSystem()->GetStreamEngine()->StartRead(eStreamTaskTypeGeometry, path, this, &params);

	if (ppStream)
		(*ppStream) = m_pReadStream;
	if (!bFinishNow)
		m_eStreamingStatus = ecss_InProgress;
	else if (!ppStream)
		m_pReadStream->Wait();
}

void CStatObj::ReleaseStreamableContent()
{
	assert(!m_pParentObject);
	assert(!m_pClonedSourceObject);
	assert(!m_bSharesChildren);

	bool bLodsAreLoadedFromSeparateFile = (m_pLod0 != 0) ? m_pLod0->m_bLodsAreLoadedFromSeparateFile : m_bLodsAreLoadedFromSeparateFile;

	if (!bLodsAreLoadedFromSeparateFile)
	{
		for (int nLod = 0; nLod < MAX_STATOBJ_LODS_NUM; nLod++)
		{
			CStatObj* pLod = (CStatObj*)GetLodObject(nLod);

			if (!pLod)
				continue;

			pLod->SetRenderMesh(0);
			pLod->FreeFoliageData();
			pLod->m_eStreamingStatus = ecss_NotLoaded;

			if (pLod->m_pParentObject)
			{
				pLod->m_pParentObject->SetRenderMesh(0);
				pLod->m_pParentObject->FreeFoliageData();
				pLod->m_pParentObject->m_eStreamingStatus = ecss_NotLoaded;
			}
		}
	}

	for (int s = 0; s < GetSubObjectCount(); s++)
	{
		SSubObject& sub = *GetSubObject(s);
		if (!sub.pStatObj)
			continue;

		if (bLodsAreLoadedFromSeparateFile)
		{
			CStatObj* pSubLod = (CStatObj*)sub.pStatObj;

			if (!pSubLod)
				continue;

			pSubLod->SetRenderMesh(0);
			pSubLod->FreeFoliageData();
			pSubLod->m_eStreamingStatus = ecss_NotLoaded;

			if (pSubLod->m_pParentObject)
			{
				pSubLod->m_pParentObject->SetRenderMesh(0);
				pSubLod->m_pParentObject->FreeFoliageData();
				pSubLod->m_pParentObject->m_eStreamingStatus = ecss_NotLoaded;
			}
		}
		else
		{
			for (int nLod = 0; nLod < MAX_STATOBJ_LODS_NUM; nLod++)
			{
				CStatObj* pSubLod = (CStatObj*)sub.pStatObj->GetLodObject(nLod);

				if (!pSubLod)
					continue;

				pSubLod->SetRenderMesh(0);
				pSubLod->FreeFoliageData();
				pSubLod->m_eStreamingStatus = ecss_NotLoaded;

				if (pSubLod->m_pParentObject)
				{
					pSubLod->m_pParentObject->SetRenderMesh(0);
					pSubLod->m_pParentObject->FreeFoliageData();
					pSubLod->m_pParentObject->m_eStreamingStatus = ecss_NotLoaded;
				}
			}
		}
	}

	SetRenderMesh(0);
	FreeFoliageData();

	m_pMergedRenderMesh = 0;

	m_eStreamingStatus = ecss_NotLoaded;
}

int CStatObj::GetStreamableContentMemoryUsage(bool bJustForDebug)
{
	assert(!m_pParentObject || bJustForDebug); // only parents allowed to be registered for streaming

	bool bLodsAreLoadedFromSeparateFile = (m_pLod0 != 0) ? m_pLod0->m_bLodsAreLoadedFromSeparateFile : m_bLodsAreLoadedFromSeparateFile;
	bool bCountLods = !bLodsAreLoadedFromSeparateFile;

	if (m_arrRenderMeshesPotentialMemoryUsage[bCountLods] < 0)
	{
		int nCount = 0;

		if (bCountLods)
		{
			if (m_pLODs)
				for (int nLod = 1; nLod < MAX_STATOBJ_LODS_NUM; nLod++)
				{
					CStatObj* pLod = (CStatObj*)m_pLODs[nLod];

					if (!pLod)
						continue;

					nCount += pLod->m_nRenderMeshMemoryUsage;
				}
		}

		nCount += m_nRenderMeshMemoryUsage;

		for (int s = 0; s < GetSubObjectCount(); s++)
		{
			SSubObject& sub = *GetSubObject(s);

			if (!sub.pStatObj)
				continue;

			if (bCountLods)
			{
				CStatObj* pSubStatObj = (CStatObj*)sub.pStatObj;
				if (pSubStatObj->m_pLODs)
				{
					for (int nLod = 1; nLod < MAX_STATOBJ_LODS_NUM; nLod++)
					{
						CStatObj* pSubLod = pSubStatObj->m_pLODs[nLod];

						if (!pSubLod)
							continue;

						nCount += pSubLod->m_nRenderMeshMemoryUsage;
					}
				}
			}

			nCount += ((CStatObj*)sub.pStatObj)->m_nRenderMeshMemoryUsage;
		}

		m_arrRenderMeshesPotentialMemoryUsage[bCountLods] = nCount;
	}

	if (m_pMergedRenderMesh)
	{
		//TODO: Need to have a function to calculate memory usage.
		//m_nMergedMemoryUsage = m_pMergedRenderMesh->GetAllocatedBytes(true);
		m_nMergedMemoryUsage = m_pMergedRenderMesh->GetVerticesCount() * (sizeof(SPipTangents) + sizeof(SVF_P3S_C4B_T2S)) + m_pMergedRenderMesh->GetIndicesCount() * sizeof(uint16);
	}
	else if (!GetCVars()->e_StatObjMerge)
	{
		m_nMergedMemoryUsage = 0;
	}

	return m_nMergedMemoryUsage + m_arrRenderMeshesPotentialMemoryUsage[bCountLods];
}

void CStatObj::UpdateStreamingPrioriryInternal(const Matrix34A& objMatrix, float fImportance, bool bFullUpdate)
{
	int nRoundId = GetObjManager()->m_nUpdateStreamingPrioriryRoundId;

	if (m_pParentObject && m_bSubObject)
	{
		// stream parent for sub-objects
		m_pParentObject->UpdateStreamingPrioriryInternal(objMatrix, fImportance, bFullUpdate);
	}
	else if (!m_bSubObject)
	{
		// stream object itself
		if (m_bCanUnload)
		{
			if (UpdateStreamingPrioriryLowLevel(fImportance, nRoundId, bFullUpdate))
				GetObjManager()->RegisterForStreaming(this);
		}
	}
	else if (m_pLod0)
	{
		// sub-object lod without parent
		m_pLod0->UpdateStreamingPrioriryInternal(objMatrix, fImportance, bFullUpdate);
		assert(!m_pLod0->m_bLodsAreLoadedFromSeparateFile);
	}
	else if (m_bCanUnload)
		assert(!"Invalid CGF hierarchy");
}

bool CStatObj::UpdateStreamableComponents(float fImportance, const Matrix34A& objMatrix, bool bFullUpdate, int nNewLod)
{
	if (m_pLod0) // redirect to lod0, otherwise we fail to pre-cache neighbor LODs
		return m_pLod0->UpdateStreamableComponents(fImportance, objMatrix, bFullUpdate, nNewLod);

#ifndef _RELEASE
	if (GetCVars()->e_StreamCgfDebug && strlen(GetCVars()->e_StreamCgfDebugFilter->GetString()) && strstr(m_szFileName, GetCVars()->e_StreamCgfDebugFilter->GetString()))
		PrintMessage(__FUNCTION__);
#endif

	//  FUNCTION_PROFILER_3DENGINE;

	if (m_nFlags & STATIC_OBJECT_HIDDEN)
		return false;

	nNewLod = min(nNewLod, MAX_STATOBJ_LODS_NUM - 1);

	if ((m_nFlags & STATIC_OBJECT_COMPOUND) && SubObjectCount())
	{
		for (int s = 0, num = SubObjectCount(); s < num; s++)
		{
			IStatObj::SSubObject& subObj = SubObject(s);

			if (subObj.pStatObj && subObj.nType == STATIC_SUB_OBJECT_MESH && !subObj.bShadowProxy)
			{
				Matrix34 subObjMatrix = objMatrix * subObj.tm;

				CStatObj* pSubStatObj = ((CStatObj*)subObj.pStatObj);

				if (pSubStatObj->m_nLoadedTrisCount < 1)
					continue;

				for (int l = nNewLod; l <= (nNewLod + 1) && l < MAX_STATOBJ_LODS_NUM; l++)
				{
					if (CStatObj* pLod = (CStatObj*)pSubStatObj->GetLodObject(l, true))
					{
						pLod->UpdateStreamingPrioriryInternal(objMatrix, fImportance, bFullUpdate);

						if (!(CStatObj*)pSubStatObj->m_pLODs)
							break;
					}
				}
			}
		}
	}
	else if (m_nLoadedTrisCount >= 1)
	{
		for (int l = nNewLod; l <= (nNewLod + 1) && l < MAX_STATOBJ_LODS_NUM; l++)
		{
			if (CStatObj* pLod = (CStatObj*)GetLodObject(l, true))
			{
				pLod->UpdateStreamingPrioriryInternal(objMatrix, fImportance, bFullUpdate);

				if (!m_pLODs)
					break;
			}
		}
	}

	// update also next state CGF
	if (!m_szStreamingDependencyFilePath.empty())
		if (CStatObj* pNextState = (CStatObj*)GetObjManager()->FindStaticObjectByFilename(m_szStreamingDependencyFilePath))
			pNextState->UpdateStreamableComponents(fImportance, objMatrix, bFullUpdate, nNewLod);

	if (m_pParentObject && !m_pParentObject->m_szStreamingDependencyFilePath.empty())
		if (CStatObj* pNextState = (CStatObj*)GetObjManager()->FindStaticObjectByFilename(m_pParentObject->m_szStreamingDependencyFilePath))
			pNextState->UpdateStreamableComponents(fImportance, objMatrix, bFullUpdate, nNewLod);

	if (GetBillboardMaterial())
	{
		GetBillboardMaterial()->PrecacheMaterial(0, 0, false);
	}

	return true;
}

void CStatObj::DisableStreaming()
{
	for (int nLodLevel = 0; nLodLevel < MAX_STATOBJ_LODS_NUM; nLodLevel++)
	{
		if (CStatObj* pLodObj = (CStatObj*)GetLodObject(nLodLevel))
		{
			pLodObj->m_nLastDrawMainFrameId = gEnv->nMainFrameID + 1000;
			pLodObj->UpdateStreamingPrioriryLowLevel(1.f, GetObjManager()->m_nUpdateStreamingPrioriryRoundId, true);
			pLodObj->m_bCanUnload = false;

			CStatObj* pParentObject = pLodObj->m_pParentObject ? pLodObj->m_pParentObject : pLodObj;

			// only register the parent object for streaming, it will stream in all sub-objects + lods
			GetObjManager()->RegisterForStreaming(pParentObject);

			// force start streaming immediately, otherwise mesh may be loaded only when global streaming update reaches it in few frames
			if (pParentObject->m_eStreamingStatus == ecss_NotLoaded)
			{
				IReadStream_AutoPtr readStream;
				pParentObject->StartStreaming(true, &readStream);
				if (!(GetSystem()->GetStreamEngine()->GetPauseMask() & 1 << eStreamTaskTypeGeometry))
					readStream->Wait();
			}
		}
	}
}

bool CStatObj::CheckForStreamingDependencyLoop(const char* szFilenameDependancy) const
{
	CObjManager* pObjManager = GetObjManager();
	while (szFilenameDependancy)
	{
		const CStatObj* pStatObjDependency = static_cast<const CStatObj*>(pObjManager->FindStaticObjectByFilename(szFilenameDependancy));
		if (!pStatObjDependency)
			return false;

		if (this == pStatObjDependency)
			return true;

		szFilenameDependancy = pStatObjDependency->m_szStreamingDependencyFilePath.c_str();
	}

	return false;
}
