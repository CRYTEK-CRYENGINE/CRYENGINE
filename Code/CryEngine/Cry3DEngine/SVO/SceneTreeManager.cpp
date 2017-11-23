// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   3dengine.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Implementation of I3DEngine interface methods
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if defined(FEATURE_SVO_GI)

	#include "VoxelSegment.h"
	#include "SceneTree.h"
	#include "SceneTreeManager.h"

extern CSvoEnv* gSvoEnv;

void CSvoManager::CheckAllocateGlobalCloud()
{
	LOADING_TIME_PROFILE_SECTION;
	if (!gSvoEnv && Cry3DEngineBase::GetCVars()->e_svoEnabled)
	{
		float fMapSize = (float)Cry3DEngineBase::Get3DEngine()->GetTerrainSize();
		AABB areaBox(Vec3(0, 0, 0), Vec3(fMapSize, fMapSize, fMapSize));
		gSvoEnv = new CSvoEnv(areaBox);
	}
}

char* CSvoManager::GetStatusString(int nLine)
{
	static char szText[256] = "";

	int nSlot = 0;

	if (nLine == (nSlot++) && (CVoxelSegment::m_nAddPolygonToSceneCounter || GetCVars()->e_svoEnabled))
	{
		int nAll = nAtlasDimBriXY * nAtlasDimBriXY * nAtlasDimBriZ;
		cry_sprintf(szText, "SVO pool: %2d of %dMB x %d, %3d/%3d of %4d = %.1f Async: %2d, Post: %2d, Loaded: %4d",
		            CVoxelSegment::m_nPoolUsageBytes / 1024 / 1024,
		            int(CVoxelSegment::nVoxTexPoolDimXY * CVoxelSegment::nVoxTexPoolDimXY * CVoxelSegment::nVoxTexPoolDimZ / 1024 / 1024) * (gSvoEnv->m_nVoxTexFormat == eTF_BC3 ? 1 : 4),
		            CVoxelSegment::m_nSvoDataPoolsCounter,
		            CVoxelSegment::m_nAddPolygonToSceneCounter, CVoxelSegment::m_nPoolUsageItems, nAll,
		            (float)CVoxelSegment::m_nPoolUsageItems / nAll,
		            CVoxelSegment::m_nStreamingTasksInProgress, CVoxelSegment::m_nPostponedCounter, CVoxelSegment::m_arrLoadedSegments.Count());
		return szText;
	}

	static ICVar* pDisplayInfo = GetConsole()->GetCVar("r_DisplayInfo");
	if (pDisplayInfo->GetIVal() < 2)
	{
		return nullptr;
	}

	if (nLine == (nSlot++))
	{
		static int nTexUpdatesInProgressLast = 0;
		nTexUpdatesInProgressLast = max(nTexUpdatesInProgressLast, CVoxelSegment::m_nUpdatesInProgressTex);

		static int nBriUpdatesInProgressLast = 0;
		nBriUpdatesInProgressLast = max(nBriUpdatesInProgressLast, CVoxelSegment::m_nUpdatesInProgressBri);

		cry_sprintf(szText, "Brick updates: %2d / %3d, bs: %d  GPU nodes: %2d / %2d",
		            nBriUpdatesInProgressLast, nTexUpdatesInProgressLast, nVoxTexMaxDim,
		            gSvoEnv->m_nDynNodeCounter, gSvoEnv->m_nDynNodeCounter_DYNL);

		float fCurTime = Cry3DEngineBase::Get3DEngine()->GetCurTimeSec();
		static float fLastTime = Cry3DEngineBase::Get3DEngine()->GetCurTimeSec();
		if (fCurTime > fLastTime + 0.1f)
		{
			if (nTexUpdatesInProgressLast)
				nTexUpdatesInProgressLast -= 4;

			if (nBriUpdatesInProgressLast)
				nBriUpdatesInProgressLast--;

			fLastTime = fCurTime;
		}

		return szText;
	}

	if (nLine == (nSlot++))
	{
		cry_sprintf(szText, "cpu bricks pool: el = %d of %d, %d MB",
		            gSvoEnv->m_cpuBricksAllocator.GetCount(),
		            gSvoEnv->m_cpuBricksAllocator.GetCapacity(),
		            gSvoEnv->m_cpuBricksAllocator.GetCapacityBytes() / 1024 / 1024);
		return szText;
	}

	if (nLine == (nSlot++))
	{
		cry_sprintf(szText, "cpu node pool: el = %d of %d, %d KB",
		            gSvoEnv->m_nodeAllocator.GetCount(),
		            gSvoEnv->m_nodeAllocator.GetCapacity(),
		            gSvoEnv->m_nodeAllocator.GetCapacityBytes() / 1024);
		return szText;
	}

	if (gSvoEnv->m_pSvoRoot && nLine == (nSlot++))
	{
		int nTrisCount = 0, nVertCount = 0, nTrisBytes = 0, nVertBytes = 0, nMaxVertPerArea = 0, nMatsCount = 0;
		gSvoEnv->m_pSvoRoot->GetTrisInAreaStats(nTrisCount, nVertCount, nTrisBytes, nVertBytes, nMaxVertPerArea, nMatsCount);

		int nVoxSegAllocatedBytes = 0;
		gSvoEnv->m_pSvoRoot->GetVoxSegMemUsage(nVoxSegAllocatedBytes);

		cry_sprintf(szText, "VoxSeg: %d MB, Tris/Verts %d / %d K, %d / %d MB, avmax %d K, Mats %d",
		            nVoxSegAllocatedBytes / 1024 / 1024,
		            nTrisCount / 1000, nVertCount / 1000, nTrisBytes / 1024 / 1024, nVertBytes / 1024 / 1024, nMaxVertPerArea / 1000, nMatsCount);
		return szText;
	}

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	if (nLine == (nSlot++) && gSvoEnv->m_arrRTPoolInds.Count())
	{
		cry_sprintf(szText, "RT pools: tex %.2f, verts %.2f, inds %.2f",
		            (float)gSvoEnv->m_arrRTPoolTexs.Count() / max((float)gSvoEnv->m_arrRTPoolTexs.capacity(), 1.f),
		            (float)gSvoEnv->m_arrRTPoolTris.Count() / max((float)gSvoEnv->m_arrRTPoolTris.capacity(), 1.f),
		            (float)gSvoEnv->m_arrRTPoolInds.Count() / max((float)gSvoEnv->m_arrRTPoolInds.capacity(), 1.f));
		return szText;
	}
	#endif

	return nullptr;
}

void CSvoManager::Update(const SRenderingPassInfo& passInfo, CCamera& newCam)
{
	if (/*Get3DEngine()->GetRenderFramesSinceLevelStart()>30 && */ passInfo.IsGeneralPass())
	{
		CSvoManager::UpdateSubSystems(passInfo.GetCamera(), newCam);
	}
}

void CSvoManager::UpdateSubSystems(const CCamera& _newCam, CCamera& newCam)
{

}

void CSvoManager::OnFrameStart(const SRenderingPassInfo& passInfo)
{
	CVoxelSegment::m_nCurrPassMainFrameID = passInfo.GetMainFrameID();
	//	if(GetCVars()->e_rsMode != RS_RENDERFARM)
	CVoxelSegment::SetVoxCamera(passInfo.GetCamera());

	CVoxelSegment::m_pCurrPassInfo = (SRenderingPassInfo*)&passInfo;
}

void CSvoManager::OnFrameComplete()
{
	//	if(gSSRSystem)
	//	gSSRSystem->OnFrameComplete();
	CVoxelSegment::m_pCurrPassInfo = 0;
}

void CSvoManager::Release()
{
	SAFE_DELETE(gSvoEnv);
}

void CSvoManager::Render()
{
	LOADING_TIME_PROFILE_SECTION;
	if (GetCVars()->e_svoTI_Apply && (!m_bLevelLoadingInProgress || gEnv->IsEditor()) && !GetCVars()->e_svoTI_Active)
	{
		GetCVars()->e_svoTI_Active = 1;
		GetCVars()->e_svoLoadTree = 1;
		GetCVars()->e_svoRender = 1;

		if (GetCVars()->e_svoTI_Troposphere_Active)
		{
			GetCVars()->e_Clouds = 0;
			if (ICVar* pV = gEnv->pConsole->GetCVar("r_UseAlphaBlend"))
				pV->Set(0);
		}
	}

	if (GetCVars()->e_svoLoadTree)
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("SVO Load Tree");
		SAFE_DELETE(gSvoEnv);

		GetCVars()->e_svoEnabled = 1;

		float fMapSize = (float)Cry3DEngineBase::Get3DEngine()->GetTerrainSize();
		AABB areaBox(Vec3(0, 0, 0), Vec3(fMapSize, fMapSize, fMapSize));
		gSvoEnv = new CSvoEnv(areaBox);
		gSvoEnv->ReconstructTree(0 /*m_pGamePlayArea && (m_pGamePlayArea->GetPoints().Count()>0)*/);

		GetCVars()->e_svoLoadTree = 0;
	}

	if (GetCVars()->e_svoEnabled && GetCVars()->e_svoRender)
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("SVO Render");

		CheckAllocateGlobalCloud();

		if (gSvoEnv && !CVoxelSegment::m_bExportMode)
		{
			if (gSvoEnv->m_bFirst_SvoFreezeTime)
				gSvoEnv->m_fSvoFreezeTime = gEnv->pTimer->GetAsyncCurTime();
			gSvoEnv->m_bFirst_SvoFreezeTime = false;

			if (gSvoEnv->m_fSvoFreezeTime > 0)
			{
				while (gSvoEnv->m_fSvoFreezeTime > 0)
				{
					gSvoEnv->Render();
					gEnv->pSystem->GetStreamEngine()->Update();
					CrySleep(5);
				}

				gSvoEnv->m_fSvoFreezeTime = -1;
			}
			else
			{
				gSvoEnv->Render();
			}
		}
	}
}

void CSvoManager::OnDisplayInfo(float& fTextPosX, float& fTextPosY, float& fTextStepY, float fTextScale)
{
	ICVar* pr_HDRRendering = GetConsole()->GetCVar("r_HDRRendering");

	if (GetCVars()->e_svoEnabled && pr_HDRRendering->GetIVal())
	{
		int nLine = 0;
		while (char* szStatus = CSvoManager::GetStatusString(nLine))
		{
			Get3DEngine()->DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, fTextScale, ColorF(0.5f, 1.0f, 1.0f), szStatus);
			nLine++;
		}
	}
}

bool CSvoManager::GetSvoStaticTextures(I3DEngine::SSvoStaticTexInfo& svoInfo, PodArray<I3DEngine::SLightTI>* pLightsTI_S, PodArray<I3DEngine::SLightTI>* pLightsTI_D)
{
	return gSvoEnv ? gSvoEnv->GetSvoStaticTextures(svoInfo, pLightsTI_S, pLightsTI_D) : false;
}

void CSvoManager::GetSvoBricksForUpdate(PodArray<I3DEngine::SSvoNodeInfo>& arrNodeInfo, float fNodeSize, PodArray<SVF_P3F_C4B_T2F>* pVertsOut)
{
	if (gSvoEnv)
		gSvoEnv->GetSvoBricksForUpdate(arrNodeInfo, fNodeSize, pVertsOut);
}

int CSvoManager::ExportSvo(ICryArchive* pArchive)
{
	if (GetCVars()->e_StreamCgf)
	{
		assert(!"e_StreamCgf must be 0 for successful SVO export");
		PrintMessage("SVO export error: e_StreamCgf must be 0 for successful SVO export");
		return 0;
	}

	CVoxelSegment::m_bExportMode = true;
	CVoxelSegment::m_nExportVisitedAreasCounter = 0;

	if (!gSvoEnv)
	{
		CSvoManager::Render();
	}

	int nResult = 0;

	if (gSvoEnv)
	{
		nResult = gSvoEnv->ExportSvo(pArchive);
	}

	CVoxelSegment::m_bExportMode = false;

	return nResult;
}

void CSvoManager::RegisterMovement(const AABB& objBox)
{
	if (gSvoEnv && gSvoEnv->m_pSvoRoot)
		gSvoEnv->m_pSvoRoot->RegisterMovement(objBox);
}

#endif
