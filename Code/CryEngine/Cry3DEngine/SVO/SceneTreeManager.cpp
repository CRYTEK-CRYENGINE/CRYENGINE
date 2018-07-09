// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
		float mapSize = (float)Cry3DEngineBase::Get3DEngine()->GetTerrainSize();
		AABB areaBox(Vec3(0, 0, 0), Vec3(mapSize, mapSize, mapSize));
		gSvoEnv = new CSvoEnv(areaBox);
	}
}

char* CSvoManager::GetStatusString(int lineId)
{
	static char szText[256] = "";

	int slotId = 0;

	if (lineId == (slotId++) && (CVoxelSegment::m_addPolygonToSceneCounter || GetCVars()->e_svoEnabled))
	{
		int allSlotsNum = SVO_ATLAS_DIM_BRICKS_XY * SVO_ATLAS_DIM_BRICKS_XY * SVO_ATLAS_DIM_BRICKS_Z;
		cry_sprintf(szText, "SVO pool: %2d of %dMB x %d, %3d/%3d of %4d = %.1f Async: %2d, Post: %2d, Loaded: %4d",
		            CVoxelSegment::m_poolUsageBytes / 1024 / 1024,
		            int(CVoxelSegment::m_voxTexPoolDimXY * CVoxelSegment::m_voxTexPoolDimXY * CVoxelSegment::m_voxTexPoolDimZ / 1024 / 1024) * (gSvoEnv->m_voxTexFormat == eTF_BC3 ? 1 : 4),
		            CVoxelSegment::m_svoDataPoolsCounter,
		            CVoxelSegment::m_addPolygonToSceneCounter, CVoxelSegment::m_poolUsageItems, allSlotsNum,
		            (float)CVoxelSegment::m_poolUsageItems / allSlotsNum,
		            CVoxelSegment::m_streamingTasksInProgress, CVoxelSegment::m_postponedCounter, CVoxelSegment::m_arrLoadedSegments.Count());
		return szText;
	}

	static ICVar* pDisplayInfo = GetConsole()->GetCVar("r_DisplayInfo");
	if (pDisplayInfo->GetIVal() < 2)
	{
		return nullptr;
	}

	if (lineId == (slotId++))
	{
		static int s_texUpdatesInProgressLast = 0;
		s_texUpdatesInProgressLast = max(s_texUpdatesInProgressLast, CVoxelSegment::m_updatesInProgressTex);

		static int s_briUpdatesInProgressLast = 0;
		s_briUpdatesInProgressLast = max(s_briUpdatesInProgressLast, CVoxelSegment::m_updatesInProgressBri);

		cry_sprintf(szText, "Brick updates: %2d / %3d, bs: %d  GPU nodes: %2d / %2d",
		            s_briUpdatesInProgressLast, s_texUpdatesInProgressLast, SVO_VOX_BRICK_MAX_SIZE,
		            gSvoEnv->m_dynNodeCounter, gSvoEnv->m_dynNodeCounter_DYNL);

		float curTime = Cry3DEngineBase::Get3DEngine()->GetCurTimeSec();
		static float s_lastTime = Cry3DEngineBase::Get3DEngine()->GetCurTimeSec();
		if (curTime > s_lastTime + 0.1f)
		{
			if (s_texUpdatesInProgressLast)
				s_texUpdatesInProgressLast -= 4;

			if (s_briUpdatesInProgressLast)
				s_briUpdatesInProgressLast--;

			s_lastTime = curTime;
		}

		return szText;
	}

	if (lineId == (slotId++))
	{
		cry_sprintf(szText, "cpu bricks pool: el = %d of %d, %d MB",
		            gSvoEnv->m_brickSubSetAllocator.GetCount(),
		            gSvoEnv->m_brickSubSetAllocator.GetCapacity(),
		            gSvoEnv->m_brickSubSetAllocator.GetCapacityBytes() / 1024 / 1024);
		return szText;
	}

	if (lineId == (slotId++))
	{
		cry_sprintf(szText, "cpu node pool: el = %d of %d, %d KB",
		            gSvoEnv->m_nodeAllocator.GetCount(),
		            gSvoEnv->m_nodeAllocator.GetCapacity(),
		            gSvoEnv->m_nodeAllocator.GetCapacityBytes() / 1024);
		return szText;
	}

	if (lineId == (slotId++))
	{
		cry_sprintf(szText, "bouncing lights: dynamic: %d, static: %d", gSvoEnv->m_lightsTI_D.Count(), gSvoEnv->m_lightsTI_S.Count());
		return szText;
	}

	if (gSvoEnv->m_pSvoRoot && lineId == (slotId++))
	{
		int trisCount = 0, vertCount = 0, trisBytes = 0, vertBytes = 0, maxVertPerArea = 0, matsCount = 0;
		gSvoEnv->m_pSvoRoot->GetTrisInAreaStats(trisCount, vertCount, trisBytes, vertBytes, maxVertPerArea, matsCount);

		int voxSegAllocatedBytes = 0;
		gSvoEnv->m_pSvoRoot->GetVoxSegMemUsage(voxSegAllocatedBytes);

		cry_sprintf(szText, "VoxSeg: %d MB, Tris/Verts %d / %d K, %d / %d MB, avmax %d K, Mats %d",
		            voxSegAllocatedBytes / 1024 / 1024,
		            trisCount / 1000, vertCount / 1000, trisBytes / 1024 / 1024, vertBytes / 1024 / 1024, maxVertPerArea / 1000, matsCount);
		return szText;
	}

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	if (lineId == (slotId++) && gSvoEnv->m_arrRTPoolInds.Count())
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
	CVoxelSegment::m_currPassMainFrameID = passInfo.GetMainFrameID();
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

void CSvoManager::Render(bool bSyncUpdate)
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

		float mapSize = (float)Cry3DEngineBase::Get3DEngine()->GetTerrainSize();
		AABB areaBox(Vec3(0, 0, 0), Vec3(mapSize, mapSize, mapSize));
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
			if (bSyncUpdate)
				gSvoEnv->m_svoFreezeTime = gEnv->pTimer->GetAsyncCurTime();

			if (gSvoEnv->m_bFirst_SvoFreezeTime)
				gSvoEnv->m_svoFreezeTime = gEnv->pTimer->GetAsyncCurTime();
			gSvoEnv->m_bFirst_SvoFreezeTime = false;

			if (gSvoEnv->m_svoFreezeTime > 0)
			{
				// perform synchronous SVO update, usually happens in first frames after level loading
				while (gSvoEnv->m_svoFreezeTime > 0)
				{
					gSvoEnv->Render();
					gEnv->pSystem->GetStreamEngine()->Update();

					if ((gEnv->pTimer->GetAsyncCurTime() - gSvoEnv->m_svoFreezeTime) > GetCVars()->e_svoTI_MaxSyncUpdateTime)
					{
						// prevent possible freeze in case of SVO pool overflow
						break;
					}

					CrySleep(5);
				}

				gSvoEnv->m_svoFreezeTime = -1;
			}
			else
			{
				gSvoEnv->Render();
			}
		}
	}
}

void CSvoManager::OnDisplayInfo(float& textPosX, float& textPosY, float& textStepY, float textScale)
{
	ICVar* pr_HDRRendering = GetConsole()->GetCVar("r_HDRRendering");

	if (GetCVars()->e_svoEnabled && pr_HDRRendering->GetIVal())
	{
		int lineId = 0;
		while (char* szStatus = CSvoManager::GetStatusString(lineId))
		{
			Get3DEngine()->DrawTextRightAligned(textPosX, textPosY += textStepY, textScale, ColorF(0.5f, 1.0f, 1.0f), szStatus);
			lineId++;
		}
	}
}

bool CSvoManager::GetSvoStaticTextures(I3DEngine::SSvoStaticTexInfo& svoInfo, PodArray<I3DEngine::SLightTI>* pLightsTI_S, PodArray<I3DEngine::SLightTI>* pLightsTI_D)
{
	return gSvoEnv ? gSvoEnv->GetSvoStaticTextures(svoInfo, pLightsTI_S, pLightsTI_D) : false;
}

void CSvoManager::GetSvoBricksForUpdate(PodArray<I3DEngine::SSvoNodeInfo>& arrNodeInfo, float nodeSize, PodArray<SVF_P3F_C4B_T2F>* pVertsOut)
{
	if (gSvoEnv)
		gSvoEnv->GetSvoBricksForUpdate(arrNodeInfo, nodeSize, pVertsOut);
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
	CVoxelSegment::m_exportVisitedAreasCounter = 0;
	CVoxelSegment::m_exportVisitedNodesCounter = 0;
	CVoxelSegment::m_bExportAbortRequested = false;

	if (!gSvoEnv)
	{
		CSvoManager::Render(false);
	}

	int result = 0;

	if (gSvoEnv)
	{
		result = gSvoEnv->ExportSvo(pArchive);
	}

	CVoxelSegment::m_bExportMode = false;

	return result;
}

void CSvoManager::RegisterMovement(const AABB& objBox)
{
	if (gSvoEnv && gSvoEnv->m_pSvoRoot)
		gSvoEnv->m_pSvoRoot->RegisterMovement(objBox);
}

#endif
