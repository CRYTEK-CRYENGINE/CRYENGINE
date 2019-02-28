// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   SceneTree.cpp
//  Created:     2012 by Vladimir Kajalin.
//  Description: CPU side SVO
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if defined(FEATURE_SVO_GI)

	#include <Cry3DEngine/ITimeOfDay.h>
	#include "VoxelSegment.h"
	#include "SceneTree.h"

	#include "BlockPacker.h"
	#include <CryAnimation/ICryAnimation.h>
	#include "Brush.h"
	#include "SceneTreeManager.h"
	#include <CryRenderer/IRenderAuxGeom.h>
	#include "visareas.h"

CSvoEnv* gSvoEnv = 0;

void CSvoEnv::ReconstructTree(bool bMultiPoint)
{
	if (GetCVars()->e_svoTI_Active)
	{
		PrintMessage("Constructing voxel tree ...");

		m_voxTexFormat = eTF_R8G8B8A8;
		PrintMessage("Voxel texture format: %s", GetRenderer()->GetTextureFormatName(m_voxTexFormat));

		SAFE_DELETE(m_pSvoRoot);

		AllocateRootNode();

		m_bReady = true;

		CVoxelSegment::CheckAllocateTexturePool();
	}
}

void CSvoEnv::AllocateRootNode()
{
	float rootSize = GetCVars()->e_svoRootless ? SVO_ROOTLESS_PARENT_SIZE : max((float)Get3DEngine()->GetTerrainSize(), SVO_ROOTLESS_PARENT_SIZE);

	m_pSvoRoot = new CSvoNode(AABB(Vec3(0, 0, 0), Vec3(rootSize)), nullptr);

	m_pSvoRoot->AllocateSegment(CVoxelSegment::m_nextSegmentId++, 0, ecss_NotLoaded, ecss_NotLoaded, true);
}

int CSvoEnv::ExportSvo(ICryArchive* pArchive)
{
	float timeStart = GetCurAsyncTimeSec();

	PrintMessage("======= Compiling SVO for entire level (press and keep ESC key to abort) =======");

	SAFE_DELETE(m_pSvoRoot);

	AllocateRootNode();

	if (m_pSvoRoot)
	{
		uint32 dummyCounter = 0;
		uint32 totalSizeCounter = 0;
		PodArray<byte> dummyArray;
		m_pSvoRoot->SaveNode(dummyArray, dummyCounter, pArchive, totalSizeCounter);

		PrintMessage("======= Finished SVO data export: %.1f MB in %1.1f sec =======", (float)totalSizeCounter / 1024.f / 1024.f, GetCurAsyncTimeSec() - timeStart);
	}

	return m_pSvoRoot ? 1 : 0;
}

Vec3* GeneratePointsOnHalfSphere(int n);
static Vec3* pKernelPoints = 0;
const int kRaysNum = 512;

bool CSvoEnv::Render()
{
	FUNCTION_PROFILER_3DENGINE;

	AUTO_LOCK(m_csLockTree);

	CVoxelSegment::CheckAllocateTexturePool();

	CVoxelSegment::UpdateObjectLayersInfo();

	CollectLights();

	CollectAnalyticalOccluders();

	if (Get3DEngine()->m_pObjectsTree)
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "CSvoEnv::Render_FindProbe");

		AUTO_LOCK(m_csLockGlobalEnvProbe);

		m_pGlobalEnvProbe = nullptr;

		PodArray<IRenderNode*> arrObjects;
		Get3DEngine()->m_pObjectsTree->GetObjectsByType(arrObjects, eERType_Light, &m_worldBox, 0, (uint64) ~0, false);

		float maxRadius = 999;

		for (int n = 0; n < arrObjects.Count(); n++)
		{
			ILightSource* pRN = (ILightSource*)arrObjects[n];
			SRenderLight& rLight = pRN->GetLightProperties();

			if (rLight.m_fRadius > maxRadius && rLight.m_Flags & DLF_DEFERRED_CUBEMAPS)
			{
				maxRadius = rLight.m_fRadius;
				m_pGlobalEnvProbe = &rLight;
			}
		}
	}

	// Teleport SVO root when camera comes close to the border of current SVO bounds
	if (GetCVars()->e_svoRootless && m_pSvoRoot && CVoxelSegment::m_streamingTasksInProgress == 0 && GetCVars()->e_svoTI_Apply && SVO_ROOTLESS_PARENT_SIZE < Get3DEngine()->GetTerrainSize())
	{
		ProcessSvoRootTeleport();
	}

	static int s_updateLightingPrev = GetCVars()->e_svoTI_UpdateLighting;
	if ((!s_updateLightingPrev || gEnv->IsEditing()) && GetCVars()->e_svoTI_UpdateLighting && GetCVars()->e_svoTI_IntegrationMode && m_bReady && m_pSvoRoot)
		DetectMovement_StatLights();
	s_updateLightingPrev = GetCVars()->e_svoTI_UpdateLighting;

	static int s_updateGeometryPrev = GetCVars()->e_svoTI_UpdateGeometry;
	if (!s_updateGeometryPrev && GetCVars()->e_svoTI_UpdateGeometry && m_bReady && m_pSvoRoot)
	{
		DetectMovement_StaticGeom();
		CVoxelSegment::m_voxTrisCounter = 0;
	}
	s_updateGeometryPrev = GetCVars()->e_svoTI_UpdateGeometry;

	bool bMultiUserMode = false;
	int userId = 0;

	#ifdef FEATURE_SVO_GI_DVR // direct volume rendering
	CRenderObject* pObjVox = 0;
	{
		pObjVox = Cry3DEngineBase::GetIdentityCRenderObject((*CVoxelSegment::m_pCurrPassInfo));
		pObjVox->SetAmbientColor(Cry3DEngineBase::Get3DEngine()->GetSkyColor(), (*CVoxelSegment::m_pCurrPassInfo));
		pObjVox->SetMatrix(Matrix34::CreateIdentity(), (*CVoxelSegment::m_pCurrPassInfo));
		pObjVox->m_nSort = 0;
		pObjVox->m_ObjFlags |= (/*FOB_NO_Z_PASS | */ FOB_NO_FOG);

		SRenderObjData* pData = pObjVox->GetObjData();
		pData->m_fTempVars[0] = (float)SVO_ATLAS_DIM_MAX_XY;
		pData->m_fTempVars[1] = (float)SVO_ATLAS_DIM_MAX_XY;
		pData->m_fTempVars[2] = (float)SVO_ATLAS_DIM_MAX_Z;
		pData->m_fTempVars[3] = (float)(SVO_VOX_BRICK_MAX_SIZE / voxBloMaxDim);
		pData->m_fTempVars[4] = 0;//GetCVars()->e_svoDVR!=1 ? 0 : 11.111f;
	}
	#endif

	if (m_bReady && m_pSvoRoot)
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "CSvoEnv::Traverse SVO");

		//		UpdatePVS();

		static Vec3 arrLastCamPos[16];
		static Vec3 arrLastCamDir[16];
		static float arrLastUpdateTime[16];
		int poolId = max(0, userId);
		PodArray<SVF_P3F_C4B_T2F>& arrVertsOut = m_arrSvoProxyVertices;//arrVertsOutPool[nPoolId];

		if (fabs(arrLastUpdateTime[poolId] - GetCurTimeSec()) > (/*bMultiUserMode ? 0.125f : 0.125f/64*/ 0.125f) || GetCVars()->e_svoEnabled >= 2 || (m_streamingStartTime >= 0) || (GetCVars()->e_svoDVR != 10))
		{
			arrLastUpdateTime[poolId] = GetCurTimeSec();
			arrLastCamPos[poolId] = CVoxelSegment::m_voxCam.GetPosition();
			arrLastCamDir[poolId] = CVoxelSegment::m_voxCam.GetViewdir();

			CVoxelSegment::m_postponedCounter = CVoxelSegment::m_checkReadyCounter = CVoxelSegment::m_addPolygonToSceneCounter = 0;

			arrVertsOut.Clear();

			m_pSvoRoot->CheckReadyForRendering(0, m_arrForStreaming);

			m_pSvoRoot->Render(0, 1, 0, arrVertsOut, m_arrForStreaming);

			CheckUpdateMeshPools();
		}

	#ifdef FEATURE_SVO_GI_DVR // direct volume rendering
		if (arrVertsOut.Count() && GetCVars()->e_svoDVR == 10)
		{
			uint16 dummyIndex = 0;

			static SInputShaderResources res;
			static _smart_ptr<IMaterial> pMat = Cry3DEngineBase::MakeSystemMaterialFromShader("Total_Illumination.RenderDvrProxiesPass_GS", &res);
			pObjVox->m_pCurrMaterial = pMat;

			Cry3DEngineBase::GetRenderer()->EF_AddPolygonToScene(pMat->GetShaderItem(),
			                                                     arrVertsOut.Count(), &arrVertsOut[0], nullptr, pObjVox, (*CVoxelSegment::m_pCurrPassInfo), &dummyIndex, 1, false);
		}
	#endif
	}

	int maxLoadedNodes = Cry3DEngineBase::GetCVars()->e_svoMaxBricksOnCPU;

	if (m_voxTexFormat != eTF_R8G8B8A8)
		maxLoadedNodes *= 4;

	//	if(GetCVars()->e_rsMode != RS_FAT_CLIENT)
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "CSvoEnv::Render_StartStreaming");

		for (int treeLevel = 0; (treeLevel < SVO_STREAM_QUEUE_MAX_SIZE); treeLevel++)
		{
			for (int distId = 0; (distId < SVO_STREAM_QUEUE_MAX_SIZE); distId++)
			{
				if (!m_bFirst_StartStreaming)
					for (int i = 0; (i < m_arrForStreaming[treeLevel][distId].Count()) && (CVoxelSegment::m_streamingTasksInProgress < Cry3DEngineBase::GetCVars()->e_svoMaxStreamRequests); i++)
					{
						if (CVoxelSegment::m_arrLoadedSegments.Count() > maxLoadedNodes)
							break;

						if (/*m_arrForStreaming[treeLevel][nDistId][i]->m_nFileStreamSize>0 && */ !m_arrForStreaming[treeLevel][distId][i]->StartStreaming(m_pStreamEngine))
							break;
					}

				m_arrForStreaming[treeLevel][distId].Clear();
			}
		}

		m_bFirst_StartStreaming = false;
	}

	if (CVoxelSegment::m_arrLoadedSegments.Count() > (maxLoadedNodes - Cry3DEngineBase::GetCVars()->e_svoMaxStreamRequests))
	{
		{
			CRY_PROFILE_REGION(PROFILE_3DENGINE, "CSvoEnv::Render_UnloadStreamable_Sort");

			qsort(CVoxelSegment::m_arrLoadedSegments.GetElements(), CVoxelSegment::m_arrLoadedSegments.Count(), sizeof(CVoxelSegment::m_arrLoadedSegments[0]), CVoxelSegment::ComparemLastVisFrameID);
		}

		{
			CRY_PROFILE_REGION(PROFILE_3DENGINE, "CSvoEnv::Render_UnloadStreamable_FreeRenderData");

			int numNodesToDelete = 4 + Cry3DEngineBase::GetCVars()->e_svoMaxStreamRequests;//CVoxelSegment::m_arrLoadedSegments.Count()/1000;

			int nodesUnloaded = 0;

			while ((nodesUnloaded < numNodesToDelete) &&
			       (CVoxelSegment::m_arrLoadedSegments[0]->m_eStreamingStatus != ecss_InProgress) &&
			       (CVoxelSegment::m_arrLoadedSegments[0]->m_lastRendFrameId < (GetCurrPassMainFrameID() - 32)))
			{
				CVoxelSegment* pSeg = CVoxelSegment::m_arrLoadedSegments[0];

				pSeg->FreeRenderData();
				pSeg->m_eStreamingStatus = ecss_NotLoaded;

				CSvoNode** ppChilds = pSeg->m_pParentCloud->m_pNode->m_ppChilds;

				for (int childId = 0; childId < 8; childId++)
				{
					if (ppChilds[childId] == pSeg->m_pNode)
					{
						SAFE_DELETE(ppChilds[childId]); // this also deletes pSeg and unregister it from CVoxelSegment::m_arrLoadedSegments
					}
				}

				assert(CVoxelSegment::m_arrLoadedSegments.Find(pSeg) < 0);

				nodesUnloaded++;
			}
		}
	}

	m_pStreamEngine->ProcessSyncCallBacks();

	CVoxelSegment::m_maxBrickUpdates = max(1, bMultiUserMode ? Cry3DEngineBase::GetCVars()->e_svoMaxBrickUpdates * 16 : (Cry3DEngineBase::GetCVars()->e_svoMaxBrickUpdates / max(1, Cry3DEngineBase::GetCVars()->e_svoTI_LowSpecMode / (int)2 + 1)));
	if (!(m_streamingStartTime < 0)) // if not bSvoReady yet
		CVoxelSegment::m_maxBrickUpdates *= 100;

	//if(nUserId == 0 || !bMultiUserMode)
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "CSvoEnv::Render_BrickUpdate");

		CVoxelSegment::m_updatesInProgressTex = 0;
		CVoxelSegment::m_updatesInProgressBri = 0;

		for (int treeLevel = 0; (treeLevel < 16); treeLevel++)
		{
			for (int i = 0; (i < gSvoEnv->m_arrForBrickUpdate[treeLevel].Count()) && (CVoxelSegment::m_updatesInProgressBri < CVoxelSegment::m_maxBrickUpdates); i++)
			{
				CVoxelSegment* pSeg = gSvoEnv->m_arrForBrickUpdate[treeLevel][i];

				if (pSeg->m_eStreamingStatus == ecss_Ready)
				{
					CVoxelSegment::m_updatesInProgressBri++;

					bool bWas = pSeg->CheckUpdateBrickRenderData(true);

					if (!pSeg->CheckUpdateBrickRenderData(false))
					{
						break;
					}

					if (!bWas)
						pSeg->PropagateDirtyFlag();
				}
			}

			gSvoEnv->m_arrForBrickUpdate[treeLevel].Clear();
		}

		if (m_texNodePoolId)
		{
			CRY_PROFILE_REGION(PROFILE_3DENGINE, "CSvoEnv::Render_UpdateNodeRenderDataPtrs");

			m_pSvoRoot->UpdateNodeRenderDataPtrs();
		}
	}

	#ifdef FEATURE_SVO_GI_DVR // direct volume rendering
	// render DVR quad
	if (GetCVars()->e_svoDVR == 10)
	{
		/*
		   SVF_P3F_C4B_T2F arrVerts[4];
		   ZeroStruct(arrVerts);
		   uint16 arrIndices[] = { 0,2,1,2,3,1 };

		   for(int x=0; x<2; x++)
		   {
		   for(int y=0; y<2; y++)
		   {
		    int i = x*2+y;
		    float X = (float)(GetRenderer()->GetWidth()*x);
		    float Y = (float)(GetRenderer()->GetHeight()*y);
		    GetRenderer()->UnProjectFromScreen(X, Y, 0.05f, &arrVerts[i].xyz.x, &arrVerts[i].xyz.y, &arrVerts[i].xyz.z);
		    arrVerts[i].st.x = (float)x;
		    arrVerts[i].st.y = (float)y;
		   }
		   }

		   static SInputShaderResources res;
		   static _smart_ptr< IMaterial > pMat = Cry3DEngineBase::MakeSystemMaterialFromShader("Total_Illumination.BlendDvrIntoScene", &res);

		   int nRGBD=0, nNORM=0, nOPAC=0;
		   GetRenderer()->GetISvoRenderer()->GetDynamicTextures(nRGBD, nNORM, nOPAC, nullptr, nullptr);
		   pObjVox->m_nTextureID = nRGBD;

		   pObjVox->m_pCurrMaterial = pMat;

		   Cry3DEngineBase::GetRenderer()->EF_AddPolygonToScene( pMat->GetShaderItem(),
		   4, &arrVerts[0], nullptr, pObjVox, (*CVoxelSegment::m_pCurrPassInfo), &arrIndices[0], 6, false );
		 */
	}
	else if (GetCVars()->e_svoDVR > 1)
	{
		const CCamera& camera = GetISystem()->GetViewCamera();

		SVF_P3F_C4B_T2F arrVerts[4];
		ZeroStruct(arrVerts);
		uint16 arrIndices[] = { 0, 2, 1, 2, 3, 1 };

		for (int x = 0; x < 2; x++)
		{
			for (int y = 0; y < 2; y++)
			{
				int i = x * 2 + y;
				float X = (float)(camera.GetViewSurfaceX() * x);
				float Y = (float)(camera.GetViewSurfaceZ() * y);

				GetRenderer()->UnProjectFromScreen(X, Y, 0.05f, &arrVerts[i].xyz.x, &arrVerts[i].xyz.y, &arrVerts[i].xyz.z);
			}
		}

		SInputShaderResourcesPtr pInputResource = GetRenderer()->EF_CreateInputShaderResource();
		SInputShaderResources& res = *pInputResource;

		static _smart_ptr<IMaterial> pMat = Cry3DEngineBase::MakeSystemMaterialFromShader("Total_Illumination.SvoDebugDraw", &res);
		pObjVox->m_pCurrMaterial = pMat;

		SRenderPolygonDescription poly(
		  pObjVox,
		  pMat->GetShaderItem(),
		  4, &arrVerts[0], nullptr,
		  &arrIndices[0], 6,
		  EFSLIST_DECAL, false);

		CVoxelSegment::m_pCurrPassInfo->GetIRenderView()->AddPolygon(poly, (*CVoxelSegment::m_pCurrPassInfo));

		if (ICVar* pV = gEnv->pConsole->GetCVar("r_UseAlphaBlend"))
			pV->Set(1);
	}
	#endif

	if (Cry3DEngineBase::GetCVars()->e_svoDebug == 4 && 0)
	{
		if (!pKernelPoints)
		{
			pKernelPoints = GeneratePointsOnHalfSphere(kRaysNum);
			//memcpy(pKernelPoints, &arrAO_Kernel[0], sizeof(Vec3)*32);

			Cry3DEngineBase::PrintMessage("Organizing subsets . . .");

			// try to organize point into 2 subsets, each subset should also present nice hemisphere
			for (int i = 0; i < 1000; i++)
			{
				// find worst points in each of subsets and swap them
				int m0 = GetWorstPointInSubSet(0, kRaysNum / 2);
				int m1 = GetWorstPointInSubSet(kRaysNum / 2, kRaysNum);
				std::swap(pKernelPoints[m0], pKernelPoints[m1]);
			}

			Cry3DEngineBase::PrintMessage("Writing kernel.txt . . .");

			FILE* f = 0;
			fopen_s(&f, "kernel.txt", "wt");
			if (f)
			{
				//        fprintf(f, "#define nSampleNum %d\n", nRaysNum);
				//        fprintf(f, "static float3 kernel[nSampleNum] = \n");
				fprintf(f, "static float3 kernel_HS_%d[%d] = \n", kRaysNum, kRaysNum);
				fprintf(f, "{\n");
				for (int p = 0; p < kRaysNum; p++)
					fprintf(f, "  float3( %.6f , %.6f, %.6f ),\n", pKernelPoints[p].x, pKernelPoints[p].y, pKernelPoints[p].z);
				fprintf(f, "};\n");
				fclose(f);
			}

			Cry3DEngineBase::PrintMessage("Done");
		}

		static Vec3 vPos = CVoxelSegment::m_voxCam.GetPosition();
		Cry3DEngineBase::DrawSphere(vPos, 4.f);
		for (int i = 0; i < kRaysNum; i++)
			Cry3DEngineBase::DrawSphere(vPos + pKernelPoints[i] * 4.f, .1f, (i < kRaysNum / 2) ? Col_Yellow : Col_Cyan);
	}

	StartupStreamingTimeTest(CVoxelSegment::m_streamingTasksInProgress == 0 && CVoxelSegment::m_updatesInProgressBri == 0 && CVoxelSegment::m_updatesInProgressTex == 0 && CVoxelSegment::m_bUpdateBrickRenderDataPostponed == 0);

	// show GI probe object in front of the camera
	if (GetCVars()->e_svoDebug == 1)
	{
		IEntity* pEnt = gEnv->pEntitySystem->FindEntityByName("svoti_debug_probe");
		if (pEnt)
		{
			CCamera& cam = CVoxelSegment::m_voxCam;
			CCamera camDef;
			pEnt->SetPos(cam.GetPosition() + cam.GetViewdir() * 1.f * camDef.GetFov() / cam.GetFov());
		}
	}

	return true;
}

void CSvoEnv::ProcessSvoRootTeleport()
{
	FUNCTION_PROFILER_3DENGINE;

	if (GetCVars()->e_svoDebug == 3 || GetCVars()->e_svoDebug == 6)
		Cry3DEngineBase::Get3DEngine()->DrawBBox(m_pSvoRoot->m_nodeBox, Col_Yellow);

	AABB rootBox = m_pSvoRoot->m_nodeBox;
	Vec3 rootCenter = rootBox.GetCenter();
	float rootSize = rootBox.GetSize().x;
	Vec3 vCamPos = gEnv->pSystem->GetViewCamera().GetPosition();

	for (int axisId = 0; axisId < 3; axisId++)
	{
		float scrollDir = 0;
		float threshold = 2.f;

		if (vCamPos[axisId] > (rootBox.max[axisId] - rootSize / 4.f + threshold))
			scrollDir = 1.f;
		else if (vCamPos[axisId] < (rootBox.min[axisId] + rootSize / 4.f - threshold))
			scrollDir = -1.f;

		if (scrollDir)
		{
			m_bRootTeleportSkipFrame = true; // workaround for one frame flicker on root teleport

			// scroll root node bounding box
			m_pSvoRoot->m_nodeBox.min[axisId] += rootSize / 2 * scrollDir;
			m_pSvoRoot->m_nodeBox.max[axisId] += rootSize / 2 * scrollDir;

			if (m_pSvoRoot->m_pSeg)
			{
				m_pSvoRoot->m_pSeg->m_bChildOffsetsDirty = 2;
				m_pSvoRoot->m_pSeg->m_vSegOrigin[axisId] = (m_pSvoRoot->m_nodeBox.max[axisId] + m_pSvoRoot->m_nodeBox.min[axisId]) / 2;
			}

			// scroll pointers to childs, delete unnecessary childs
			if (m_pSvoRoot->m_ppChilds)
			{
				for (int i = 0; i < 2; i++)
				{
					for (int j = 0; j < 2; j++)
					{
						int childIdOld = 0, childIdNew = 0;

						bool o = vCamPos[axisId] > rootCenter[axisId];
						bool n = !o;

						if (axisId == 0) // X
						{
							childIdOld = (o ? 4 : 0) | (i ? 2 : 0) | (j ? 1 : 0);
							childIdNew = (n ? 4 : 0) | (i ? 2 : 0) | (j ? 1 : 0);
						}
						else if (axisId == 1) // Y
						{
							childIdOld = (i ? 4 : 0) | (o ? 2 : 0) | (j ? 1 : 0);
							childIdNew = (i ? 4 : 0) | (n ? 2 : 0) | (j ? 1 : 0);
						}
						else if (axisId == 2) // Z
						{
							childIdOld = (j ? 4 : 0) | (i ? 2 : 0) | (o ? 1 : 0);
							childIdNew = (j ? 4 : 0) | (i ? 2 : 0) | (n ? 1 : 0);
						}

						SAFE_DELETE(m_pSvoRoot->m_ppChilds[childIdNew]);

						m_pSvoRoot->m_ppChilds[childIdNew] = m_pSvoRoot->m_ppChilds[childIdOld];
						m_pSvoRoot->m_ppChilds[childIdOld] = nullptr;

						m_pSvoRoot->m_arrChildNotNeeded[childIdNew] = m_pSvoRoot->m_arrChildNotNeeded[childIdOld];
						m_pSvoRoot->m_arrChildNotNeeded[childIdOld] = false;
					}
				}
			}
		}
	}

	gSvoEnv->m_vSvoOriginAndSize = Vec4(m_pSvoRoot->m_nodeBox.min, m_pSvoRoot->m_nodeBox.max.x - m_pSvoRoot->m_nodeBox.min.x);
}

CSvoNode* CSvoNode::FindNodeByPosition(const Vec3& vPosWS, int treeLevelToFind, int treeLevelCur)
{
	if (treeLevelToFind == treeLevelCur)
		return this;

	Vec3 vNodeCenter = m_nodeBox.GetCenter();

	int childId =
	  ((vPosWS.x > vNodeCenter.x) ? 4 : 0) |
	  ((vPosWS.y > vNodeCenter.y) ? 2 : 0) |
	  ((vPosWS.z > vNodeCenter.z) ? 1 : 0);

	if (m_ppChilds && m_ppChilds[childId])
		return m_ppChilds[childId]->FindNodeByPosition(vPosWS, treeLevelToFind, treeLevelCur + 1);

	return nullptr;
}

void CSvoNode::OnStatLightsChanged(const AABB& objBox)
{
	if (Overlap::AABB_AABB(objBox, m_nodeBox))
	{
		m_pSeg->m_bStatLightsChanged = 1;

		if (m_ppChilds)
		{
			for (int childId = 0; childId < 8; childId++)
			{
				if (m_ppChilds[childId])
				{
					m_ppChilds[childId]->OnStatLightsChanged(objBox);
				}
			}
		}
	}
}

AABB CSvoNode::GetChildBBox(int childId)
{
	int x = (childId / 4);
	int y = (childId - x * 4) / 2;
	int z = (childId - x * 4 - y * 2);
	Vec3 vSize = m_nodeBox.GetSize() * 0.5f;
	Vec3 vOffset = vSize;
	vOffset.x *= x;
	vOffset.y *= y;
	vOffset.z *= z;
	AABB childBox;
	childBox.min = m_nodeBox.min + vOffset;
	childBox.max = childBox.min + vSize;
	return childBox;
}

bool CSvoNode::CheckReadyForRendering(int treeLevel, PodArray<CVoxelSegment*> arrForStreaming[SVO_STREAM_QUEUE_MAX_SIZE][SVO_STREAM_QUEUE_MAX_SIZE])
{
	bool bAllReady = true;

	if (m_pSeg)
	{
		CVoxelSegment* pCloud = m_pSeg;

		//		if(CVoxelSegment::voxCamera.IsAABBVisible_E(m_nodeBox))
		if (GetCurrPassMainFrameID() > 1)
			pCloud->m_lastRendFrameId = max(pCloud->m_lastRendFrameId, GetCurrPassMainFrameID() - 1);

		CVoxelSegment::m_checkReadyCounter++;

		if (pCloud->m_eStreamingStatus == ecss_NotLoaded)
		{
			float boxSize = m_nodeBox.GetSize().x;

			int distanceId = min(SVO_STREAM_QUEUE_MAX_SIZE - 1, (int)(m_nodeBox.GetDistance(CVoxelSegment::m_voxCam.GetPosition()) / boxSize));
			int treeLevelId = min(SVO_STREAM_QUEUE_MAX_SIZE - 1, treeLevel);
			if (arrForStreaming[treeLevelId][distanceId].Count() < Cry3DEngineBase::GetCVars()->e_svoMaxStreamRequests)
				arrForStreaming[treeLevelId][distanceId].Add(pCloud);
		}

		if (pCloud->m_eStreamingStatus != ecss_Ready)
		{
			bAllReady = false;
		}

		if (pCloud->m_eStreamingStatus == ecss_Ready)
		{
			if (!pCloud->CheckUpdateBrickRenderData(true))
			{
				if (gSvoEnv->m_arrForBrickUpdate[treeLevel].Count() < CVoxelSegment::m_maxBrickUpdates)
					gSvoEnv->m_arrForBrickUpdate[treeLevel].Add(pCloud);
				bAllReady = false;
			}
		}
	}

	return bAllReady;
}

CSvoNode* CSvoNode::FindNodeByPoolAffset(int allocatedAtlasOffset)
{
	if (allocatedAtlasOffset == m_pSeg->m_allocatedAtlasOffset)
		return this;

	if (m_ppChilds)
	{
		for (int childId = 0; childId < 8; childId++)
		{
			if (m_ppChilds[childId])
			{
				if (CSvoNode* pRes = m_ppChilds[childId]->FindNodeByPoolAffset(allocatedAtlasOffset))
					return pRes;
			}
		}
	}

	return 0;
}

void CSvoNode::RegisterMovement(const AABB& objBox)
{
	if (Overlap::AABB_AABB(objBox, m_nodeBox))
	{
		//		m_nRequestBrickUpdateFrametId = GetCurrPassMainFrameID();

		//m_bLightingChangeDetected = true;

		if (m_pSeg &&
		    m_pSeg->m_eStreamingStatus != ecss_NotLoaded &&
		    m_pSeg->GetBoxSize() <= Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize)
		{
			//			m_pCloud->m_bRefreshRequeste d = true;
		}

		if (m_ppChilds)
		{
			for (int childId = 0; childId < 8; childId++)
			{
				if (m_ppChilds[childId])
				{
					m_ppChilds[childId]->RegisterMovement(objBox);
				}
			}
		}
	}
}

Vec3i ComputeDataCoord(int atlasOffset)
{
	static const Vec3i vAtlasDimInt(32, 32, 32);

	//	nAtlasOffset = clamp(nAtlasOffset, 0, vAtlasDimInt*vAtlasDimInt*vAtlasDimInt);

	Vec3i vOffset3D;
	vOffset3D.z = atlasOffset / vAtlasDimInt.x / vAtlasDimInt.y;
	vOffset3D.y = (atlasOffset / vAtlasDimInt.x - vOffset3D.z * vAtlasDimInt.y);
	vOffset3D.x = atlasOffset - vOffset3D.z * vAtlasDimInt.y * vAtlasDimInt.y - vOffset3D.y * vAtlasDimInt.x;

	return vOffset3D;
}

void CSvoNode::Render(PodArray<struct SPvsItem>* pSortedPVS, uint64 nodeKey, int treeLevel, PodArray<SVF_P3F_C4B_T2F>& arrVertsOut, PodArray<CVoxelSegment*> arrForStreaming[SVO_STREAM_QUEUE_MAX_SIZE][SVO_STREAM_QUEUE_MAX_SIZE])
{
	float boxSize = m_nodeBox.GetSize().x;

	Vec3 vCamPos = CVoxelSegment::m_voxCam.GetPosition();

	const float nodeToCamDist = m_nodeBox.GetDistance(vCamPos);

	const float boxSizeRated = boxSize * Cry3DEngineBase::GetCVars()->e_svoTI_VoxelizationLODRatio;

	bool bDrawThisNode = false;

	if (m_ppChilds)
	{
		for (int childId = 0; childId < 8; childId++)
		{
			if (m_ppChilds[childId] && m_ppChilds[childId]->m_bForceRecreate && !m_ppChilds[childId]->IsStreamingInProgress())
			{
				SAFE_DELETE(m_ppChilds[childId]);
				gSvoEnv->m_svoFreezeTime = gEnv->pTimer->GetAsyncCurTime();
				ZeroStruct(gSvoEnv->m_arrVoxelizeMeshesCounter);
			}
		}
	}

	// auto allocate new childs
	if (Cry3DEngineBase::GetCVars()->e_svoTI_Active && m_pSeg && m_pSeg->m_eStreamingStatus == ecss_Ready && m_pSeg->m_pBlockInfo)
		if (m_pSeg->m_dwChildTrisTest || (m_pSeg->GetBoxSize() > Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize) || (Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Subdivide))
		{
			if (nodeToCamDist < boxSizeRated && !bDrawThisNode)
			{
				CheckAllocateChilds();
			}
		}

	if ((!m_ppChilds || (nodeToCamDist > boxSizeRated) || (boxSize <= Cry3DEngineBase::GetCVars()->e_svoMinNodeSize))
	    || (!CVoxelSegment::m_voxCam.IsAABBVisible_E(m_nodeBox) && (nodeToCamDist * 1.5 > boxSizeRated)))
	{
		bDrawThisNode = true;
	}

	treeLevel++;

	if (!bDrawThisNode && m_ppChilds)
	{
		if (Cry3DEngineBase::GetCVars()->e_svoDVR)
		{
			// check one child per frame
			int childId = GetCurrPassMainFrameID() % 8;

			{
				CSvoNode* pChild = m_ppChilds[childId];
				if (pChild && pChild->m_pSeg)
					if (!pChild->CheckReadyForRendering(treeLevel, arrForStreaming))
						//					if(CVoxelSegment::voxCamera.IsAABBVisible_E(pChild->m_nodeBox))
						bDrawThisNode = true;
			}
		}
		else
		{
			for (int childId = 0; childId < 8; childId++)
			{
				CSvoNode* pChild = m_ppChilds[childId];
				if (pChild && pChild->m_pSeg)
					if (!pChild->CheckReadyForRendering(treeLevel, arrForStreaming))
						//					if(CVoxelSegment::voxCamera.IsAABBVisible_E(pChild->m_nodeBox))
						bDrawThisNode = true;
			}
		}
	}

	if (bDrawThisNode)
	{
		if (m_pSeg)
		{
			//			if(m_pCloud->m_voxVolumeBox.IsReset() || CVoxelSegment::voxCamera.IsAABBVisible_E(m_pCloud->m_voxVolumeBox))
			{
				if (CVoxelSegment::m_voxCam.IsAABBVisible_E(m_nodeBox))
					m_pSeg->RenderMesh(arrVertsOut);
			}
		}

		if (m_pSeg && Cry3DEngineBase::GetCVars()->e_svoDebug == 2)
			if (CVoxelSegment::m_voxCam.IsAABBVisible_E(m_nodeBox) && m_nodeBox.GetSize().z <= Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize)
			{
				//        if(fBoxSize <= Cry3DEngineBase::GetCVars()->e_svoMinNodeSize)
				//          Cry3DEngineBase::Get3DEngine()->DrawBBox(m_nodeBox);

				Cry3DEngineBase::Get3DEngine()->DrawBBox(m_nodeBox,
				                                         ColorF(
				                                           (m_nodeBox.GetSize().z == Cry3DEngineBase::GetCVars()->e_svoMinNodeSize),
				                                           ((m_nodeBox.GetSize().z - Cry3DEngineBase::GetCVars()->e_svoMinNodeSize) / (Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize - Cry3DEngineBase::GetCVars()->e_svoMinNodeSize)),
				                                           (m_nodeBox.GetSize().z == Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize),
				                                           1));
			}
	}
	else if (m_ppChilds)
	{
		Vec3 vSrcCenter = m_nodeBox.GetCenter();

		int firstId =
		  ((vCamPos.x > vSrcCenter.x) ? 4 : 0) |
		  ((vCamPos.y > vSrcCenter.y) ? 2 : 0) |
		  ((vCamPos.z > vSrcCenter.z) ? 1 : 0);

		byte arrChildId[8];
		arrChildId[0] = firstId;
		arrChildId[1] = firstId ^ 1;
		arrChildId[2] = firstId ^ 2;
		arrChildId[3] = firstId ^ 4;
		arrChildId[4] = firstId ^ 3;
		arrChildId[5] = firstId ^ 5;
		arrChildId[6] = firstId ^ 6;
		arrChildId[7] = firstId ^ 7;

		for (int c = 0; c < 8; c++)
		{
			int childId = arrChildId[c];
			CSvoNode* pChild = m_ppChilds[childId];
			if (pChild)
				pChild->Render(nullptr, (nodeKey << 3) + (childId), treeLevel, arrVertsOut, arrForStreaming);
		}
	}
}

Vec3* GeneratePointsOnHalfSphere(int n)
{
	((C3DEngine*)gEnv->p3DEngine)->PrintMessage("Generating kernel of %d points . . .", n);

	int i, j;
	int counter = 0, countmax = n * 1024 * 4;
	int minp1 = 0, minp2 = 0;
	float d = 0, mind = 0;
	static Vec3 p[1000];
	Vec3 p1(0, 0, 0), p2(0, 0, 0);

	// Create the initial random set of points

	srand(0);

	float minZ = 0.5f;

	for (i = 0; i < n; i++)
	{
		p[i].x = float(cry_random(-500, 500));
		p[i].y = float(cry_random(-500, 500));
		p[i].z = fabs(float(cry_random(-500, 500)));

		if (p[i].GetLength() < 100)
		{
			i--;
			continue;
		}

		p[i].Normalize();

		p[i].z += minZ;

		p[i].Normalize();
	}

	while (counter < countmax)
	{
		if (counter && (counter % 10000) == 0)
			((C3DEngine*)gEnv->p3DEngine)->PrintMessage("Adjusting points, pass %d K of %d K . . .", counter / 1000, countmax / 1000);

		// Find the closest two points

		minp1 = 0;
		minp2 = 1;
		mind = sqrt(p[minp1].GetSquaredDistance2D(p[minp2]));

		for (i = 0; i < n - 1; i++)
		{
			for (j = i + 1; j < n; j++)
			{
				if ((d = sqrt(p[i].GetSquaredDistance2D(p[j]))) < mind)
				{
					mind = d;
					minp1 = i;
					minp2 = j;
				}
			}
		}

		// Move the two minimal points apart, in this case by 1%, but should really vary this for refinement

		if (d == 0)
			p[minp2].z += 0.001f;

		p1 = p[minp1];
		p2 = p[minp2];

		p[minp2].x = p1.x + 1.01f * (p2.x - p1.x);
		p[minp2].y = p1.y + 1.01f * (p2.y - p1.y);
		p[minp2].z = p1.z + 1.01f * (p2.z - p1.z);

		p[minp1].x = p1.x - 0.01f * (p2.x - p1.x);
		p[minp1].y = p1.y - 0.01f * (p2.y - p1.y);
		p[minp1].z = p1.z - 0.01f * (p2.z - p1.z);

		p[minp2].z = max(p[minp2].z, minZ);
		p[minp1].z = max(p[minp1].z, minZ);

		p[minp1].Normalize();
		p[minp2].Normalize();

		counter++;
	}

	((C3DEngine*)gEnv->p3DEngine)->PrintMessage("  Finished generating kernel");

	return p;
}

CSvoEnv::CSvoEnv(const AABB& worldBox)
{
	gSvoEnv = this;

	m_vSvoOriginAndSize = Vec4(worldBox.min, worldBox.max.x - worldBox.min.x);
	m_debugDrawVoxelsCounter = 0;
	m_voxTexFormat = eTF_R8G8B8A8; // eTF_BC3

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	m_texTrisPoolId = 0;
	#endif
	m_texRgb0PoolId = 0;
	m_texRgb1PoolId = 0;
	m_texDynlPoolId = 0;
	m_texRgb2PoolId = 0;
	m_texRgb3PoolId = 0;
	m_texRgb4PoolId = 0;
	m_texNormPoolId = 0;
	m_texAldiPoolId = 0;
	m_texOpasPoolId = 0;
	m_texNodePoolId = 0;

	m_prevCheckVal = -1000000;
	m_streamingStartTime = 0;
	m_nodeCounter = 0;
	m_dynNodeCounter_DYNL = m_dynNodeCounter = 0;

	if (GetCVars()->e_svoTI_Active)
		GetCVars()->e_svoVoxelPoolResolution = min((int)256, GetCVars()->e_svoVoxelPoolResolution);

	CVoxelSegment::m_voxTexPoolDimXY = min(GetCVars()->e_svoVoxelPoolResolution, (int)512);
	CVoxelSegment::m_voxTexPoolDimZ = min(GetCVars()->e_svoVoxelPoolResolution, (int)1024);
	CVoxelSegment::m_voxTexPoolDimZ = max(CVoxelSegment::m_voxTexPoolDimZ, (int)256);

	m_worldBox = worldBox;
	m_bReady = false;

	AllocateRootNode();

	m_pGlobalEnvProbe = nullptr;
	m_svoFreezeTime = -1;
	ZeroStruct(gSvoEnv->m_arrVoxelizeMeshesCounter);
	GetRenderer()->GetISvoRenderer(); // allocate SVO sub-system in renderer
	m_bFirst_SvoFreezeTime = m_bFirst_StartStreaming = true;
	m_bStreamingDonePrev = false;

	m_pStreamEngine = new CVoxStreamEngine();
}

CSvoEnv::~CSvoEnv()
{
	SAFE_DELETE(m_pStreamEngine);
	assert(CVoxelSegment::m_streamingTasksInProgress == 0);

	SAFE_DELETE(m_pSvoRoot);

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	GetRenderer()->RemoveTexture(m_texTrisPoolId);
	#endif
	GetRenderer()->RemoveTexture(m_texRgb0PoolId);
	GetRenderer()->RemoveTexture(m_texRgb1PoolId);
	GetRenderer()->RemoveTexture(m_texDynlPoolId);
	GetRenderer()->RemoveTexture(m_texRgb2PoolId);
	GetRenderer()->RemoveTexture(m_texRgb3PoolId);
	GetRenderer()->RemoveTexture(m_texRgb4PoolId);
	GetRenderer()->RemoveTexture(m_texNormPoolId);
	GetRenderer()->RemoveTexture(m_texAldiPoolId);
	GetRenderer()->RemoveTexture(m_texOpasPoolId);
	GetRenderer()->RemoveTexture(m_texNodePoolId);
	CVoxelSegment::m_svoDataPoolsCounter = 0;

	GetCVars()->e_svoTI_Active = 0;
	GetCVars()->e_svoTI_Apply = 0;
	GetCVars()->e_svoLoadTree = 0;
	GetCVars()->e_svoEnabled = 0;
	gSvoEnv = nullptr;

	{
		AUTO_MODIFYLOCK(CVoxelSegment::m_arrLockedTextures.m_Lock);
		CVoxelSegment::m_arrLockedTextures.clear();
	}

	{
		AUTO_MODIFYLOCK(CVoxelSegment::m_arrLockedMaterials.m_Lock);
		CVoxelSegment::m_arrLockedMaterials.clear();
	}
}

void CSvoNode::CheckAllocateSegment(int lodId)
{
	if (!m_pSeg)
	{
		int segmentId = CVoxelSegment::m_nextSegmentId;
		CVoxelSegment::m_nextSegmentId++;

		AllocateSegment(segmentId, -1, lodId, ecss_Ready, false);
	}
}

CSvoNode::CSvoNode(const AABB& box, CSvoNode* pParent)
{
	ZeroStruct(*this);

	m_pParent = pParent;
	m_nodeBox = box;

	if (!pParent)
		gSvoEnv->m_vSvoOriginAndSize = Vec4(box.min, box.max.x - box.min.x);

	gSvoEnv->m_nodeCounter++;
}

CSvoNode::~CSvoNode()
{
	DeleteChilds();

	if (m_pSeg)
	{
		CVoxelSegment* pCloud = m_pSeg;
		delete pCloud;
	}
	m_pSeg = 0;

	SAFE_DELETE_ARRAY(m_pChildFileOffsets);

	gSvoEnv->m_nodeCounter--;
}

uint32 CSvoNode::SaveNode(PodArray<byte>& rS, uint32& nodesCounterRec, ICryArchive* pArchive, uint32& totalSizeCounter)
{
	assert(m_pSeg);

	if (m_pSeg->m_eStreamingStatus != ecss_Ready)
	{
		assert(m_pSeg->m_eStreamingStatus != ecss_InProgress);

		// start streaming
		m_pSeg->m_eStreamingStatus = ecss_InProgress;
		m_pSeg->m_streamingTasksInProgress++;

		// async callback
		m_pSeg->VoxelizeMeshes(0, true);

		// sync callback
		m_pSeg->StreamOnComplete(0, 0);
	}

	assert(m_pSeg->m_eStreamingStatus == ecss_Ready);

	// auto allocate new childs same way it happens during rendering
	if (Cry3DEngineBase::GetCVars()->e_svoTI_Active && m_pSeg && m_pSeg->m_eStreamingStatus == ecss_Ready /*&& m_pSeg->m_pBlockInfo*/)
	{
		if (m_pSeg->m_dwChildTrisTest || (m_pSeg->GetBoxSize() > Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize) || (Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Subdivide))
		{
			CheckAllocateChilds();
		}
	}

	if (m_nodeBox.GetSize().x > SVO_ROOTLESS_PARENT_SIZE)
	{
		// just continue recursion

		for (int childId = 0; childId < 8; childId++)
		{
			if (m_ppChilds && m_ppChilds[childId])
			{
				m_ppChilds[childId]->SaveNode(rS, nodesCounterRec, pArchive, totalSizeCounter);
			}
		}

		return 0;
	}
	else if (m_nodeBox.GetSize().x == SVO_ROOTLESS_PARENT_SIZE)
	{
		// create new export file for every child and continue recursion

		for (int childId = 0; childId < 8; childId++)
		{
			if (m_ppChilds && m_ppChilds[childId])
			{
				char szFileName[256];
				m_ppChilds[childId]->MakeNodeFilePath(szFileName);

				if (pArchive->FindFile(szFileName) == nullptr)
				{
					PodArray<byte> arrSvoData;
					arrSvoData.PreAllocate(32 * 1024 * 1024);

					uint32 nodesCounter = 0;
					float startTime = Cry3DEngineBase::GetTimer()->GetAsyncCurTime();

					AABB terrainBox = AABB(Vec3(0, 0, 0), Vec3((float)gEnv->p3DEngine->GetTerrainSize(), (float)gEnv->p3DEngine->GetTerrainSize(), (float)gEnv->p3DEngine->GetTerrainSize()));
					terrainBox.Expand(-Vec3(Cry3DEngineBase::GetCVars()->e_svoTI_VoxelizationMapBorder + 1.f, Cry3DEngineBase::GetCVars()->e_svoTI_VoxelizationMapBorder + 1.f, 0));
					bool isInPlayableArea = terrainBox.IsIntersectBox(m_ppChilds[childId]->m_nodeBox);

					if (isInPlayableArea && !CVoxelSegment::m_bExportAbortRequested)
					{
						m_ppChilds[childId]->SaveNode(arrSvoData, nodesCounter, pArchive, totalSizeCounter);
					}

					if (nodesCounter)
					{
						assert(pArchive);

						// We do not use PAK compression because we will stream from those files (each SVO node is compressed independently)
						pArchive->UpdateFile(szFileName, arrSvoData.GetElements(), arrSvoData.GetDataSize(), ICryArchive::METHOD_STORE);
						totalSizeCounter += arrSvoData.GetDataSize();

						int areaGridDim = int(Cry3DEngineBase::Get3DEngine()->GetTerrainSize() / (SVO_ROOTLESS_PARENT_SIZE / 2));
						Cry3DEngineBase::PrintMessage("Exported segment %s, Progress = %d %%", szFileName, int(100.f * float(CVoxelSegment::m_exportVisitedAreasCounter) / float(areaGridDim * areaGridDim * areaGridDim)));

						Cry3DEngineBase::PrintMessage("%d KB stored in %.1f sec (%d nodes, total size = %.1f MB)",
						                              arrSvoData.Count() / 1024, Cry3DEngineBase::GetTimer()->GetAsyncCurTime() - startTime,
						                              nodesCounter, (float)totalSizeCounter / 1024.f / 1024.f);
					}
					else
					{
						pArchive->RemoveFile(szFileName);
					}
				}

				SAFE_DELETE(m_ppChilds[childId]);

				CVoxelSegment::m_exportVisitedAreasCounter++;
			}
		}

		return 0;
	}

	assert(m_pSeg);

	assert(m_pParent); // levels smaller than SVO_ROOTLESS_PARENT_SIZE not supported for now

	#ifdef SVO_DEBUG
	Vec3i vKey = (Vec3i)m_nodeBox.GetCenter();
	int boxSize = (int)m_nodeBox.GetSize().z;
	if (boxSize == Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize)
	{
		Cry3DEngineBase::PrintMessage("Voxelizing SVO area %04d_%04d_%04d_%04d", vKey.x, vKey.y, vKey.z, boxSize);
	}
	#endif

	uint32 nNodeDataSize = rS.Count();

	m_pSeg->SaveVoxels(rS);

	if (m_pSeg->m_voxData.pData[SVoxBrick::OPA3D] ? (m_pSeg->m_vCropTexSize.x * m_pSeg->m_vCropTexSize.y * m_pSeg->m_vCropTexSize.z * sizeof(ColorB)) : 0)
	{
		nodesCounterRec++;
	}

	if (!(++CVoxelSegment::m_exportVisitedNodesCounter % 2000))
	{
		Cry3DEngineBase::PrintMessage("SVO nodes processed: %d K", CVoxelSegment::m_exportVisitedNodesCounter / 1000);

		if (Cry3DEngineBase::IsEscapePressed())
		{
			if (!CVoxelSegment::m_bExportAbortRequested)
			{
				Cry3DEngineBase::PrintMessage("Aborting SVO export process with the ESC Key");
			}

			CVoxelSegment::m_bExportAbortRequested = true;
		}
	}

	// calculate node data size
	nNodeDataSize = (rS.Count() - nNodeDataSize);

	// information about childs is appended only if childs exist; at loading time presence of this chunk will be detected by size of remaining data
	if (m_ppChilds && !CVoxelSegment::m_bExportAbortRequested)
	{
		int childOffsetsPos = rS.Count();

		const uint32 arrZeros[8 * 2] = { 0 };

		// preallocate space for child offsets
		rS.AddList((byte*)arrZeros, sizeof(arrZeros)); // file offset

		for (int childId = 0; childId < 8; childId++)
		{
			if (m_ppChilds && m_ppChilds[childId])
			{
				((uint32*)&rS[childOffsetsPos])[childId * 2 + 0] = rS.Count();  // file offset

				int childNodeDataSize = m_ppChilds[childId]->SaveNode(rS, nodesCounterRec, pArchive, totalSizeCounter);

				((uint32*)&rS[childOffsetsPos])[childId * 2 + 1] = childNodeDataSize;  // file data size
			}
		}

		nNodeDataSize += 8 * sizeof(uint32) * 2;
	}

	#ifdef SVO_DEBUG
	if (boxSize == Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize)
	{
		Cry3DEngineBase::PrintMessage("%d tris voxelized", m_pSeg->m_pTrisInArea ? m_pSeg->m_pTrisInArea->Count() : 0);
	}
	#endif

	return nNodeDataSize;
}

void CSvoNode::MakeNodeFilePath(char* szFileName)
{
	Vec3_tpl<uint16> vPos = (Vec3_tpl<uint16> )m_nodeBox.GetCenter();

	uint16 nBoxSize = (uint16)m_nodeBox.GetSize().z;

	const char* pLevelPath = Cry3DEngineBase::Get3DEngine()->GetLevelFilePath(COMPILED_SVO_FOLDER_NAME);

	sprintf(szFileName, "%s%04d_%04d_%04d_%04d.crysvo", pLevelPath, vPos.x, vPos.y, vPos.z, nBoxSize);
}

CVoxelSegment* CSvoNode::AllocateSegment(int segmentId, int stationId, int lodId, EFileStreamingStatus eStreamingStatus, bool bDroppedOnDisk)
{
	CVoxelSegment* pCloud = new CVoxelSegment(this, true, eStreamingStatus, bDroppedOnDisk);

	AABB cloudBox = m_nodeBox;
	Vec3 vCenter = m_nodeBox.GetCenter();
	cloudBox.min -= vCenter;
	cloudBox.max -= vCenter;
	pCloud->SetBoxOS(cloudBox);
	//	pCloud->SetRenderBBox(m_nodeBox);

	pCloud->SetID(segmentId);
	//pCloud->m_nStationId = nStationId;
	pCloud->m_vSegOrigin = vCenter;

	m_pSeg = pCloud;

	if (m_pParent)
		m_pSeg->m_pParentCloud = m_pParent->m_pSeg;

	pCloud->m_vStaticGeomCheckSumm = GetStatGeomCheckSumm();

	return pCloud;
}

Vec3i CSvoNode::GetStatGeomCheckSumm()
{
	Vec3i vCheckSumm(0, 0, 0);

	float nodeSize = m_nodeBox.GetSize().x;

	if (nodeSize > Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize)
		return vCheckSumm;

	PodArray<IRenderNode*> lstObjects;
	AABB boxEx = m_nodeBox;
	float border = nodeSize / SVO_VOX_BRICK_MAX_SIZE;
	boxEx.Expand(Vec3(border, border, border));
	Cry3DEngineBase::Get3DEngine()->GetObjectsByTypeGlobal(lstObjects, eERType_Brush, &boxEx);
	Cry3DEngineBase::Get3DEngine()->GetVisAreaManager()->GetObjectsByType(lstObjects, eERType_Brush, &boxEx);

	float precisioin = 1000;

	for (int i = 0; i < lstObjects.Count(); i++)
	{
		IRenderNode* pNode = lstObjects[i];

		if (pNode->GetRndFlags() & (ERF_HIDDEN | ERF_COLLISION_PROXY | ERF_RAYCAST_PROXY))
			continue;

		//				if(!(pNode->GetRndFlags() & (ERF_CASTSHADOWMAPS|ERF_CASTSHADOWINTORAMMAP)))
		//				continue;

		AABB box = pNode->GetBBox();

		IStatObj* pStatObj = pNode->GetEntityStatObj();

		if (pStatObj && pStatObj->m_eStreamingStatus == ecss_Ready)
		{
			vCheckSumm += Vec3i(box.min * precisioin);
			vCheckSumm += Vec3i(box.max * precisioin * 2);
			vCheckSumm.x += uint16(((uint64)pNode->GetMaterial()) / 64);
		}
	}

	// add visarea shapes
	for (int v = 0;; v++)
	{
		CVisArea* pVisArea = (CVisArea*)Cry3DEngineBase::GetVisAreaManager()->GetVisAreaById(v);
		if (!pVisArea)
			break;

		if (!Overlap::AABB_AABB(*pVisArea->GetAABBox(), m_nodeBox))
			continue;

		vCheckSumm += Vec3i(pVisArea->GetAABBox()->min * precisioin);
		vCheckSumm += Vec3i(pVisArea->GetAABBox()->max * precisioin * 2);
	}

	return vCheckSumm;
}

void CSvoNode::UpdateNodeRenderDataPtrs()
{
	if (m_pSeg && m_ppChilds && m_pSeg->m_pBlockInfo && m_pSeg->m_bChildOffsetsDirty)
	{
		ZeroStruct(m_pSeg->m_arrChildOffset);

		for (int childId = 0; childId < 8; childId++)
		{
			CSvoNode* pChildNode = m_ppChilds[childId];

			if (pChildNode && pChildNode->m_pSeg)
			{
				pChildNode->UpdateNodeRenderDataPtrs();

				if (pChildNode->m_pSeg->CheckUpdateBrickRenderData(true))
				{
					m_pSeg->m_arrChildOffset[childId] = pChildNode->m_pSeg->m_allocatedAtlasOffset;

					int allChildsNum = 0;
					int readyChildsNum = 0;

					if (pChildNode->m_ppChilds)
					{
						for (int subChildId = 0; subChildId < 8; subChildId++)
						{
							if (CSvoNode* pSubChildNode = pChildNode->m_ppChilds[subChildId])
							{
								if (pSubChildNode->m_pSeg)
								{
									allChildsNum++;
									if (pSubChildNode->m_pSeg->CheckUpdateBrickRenderData(true))
										readyChildsNum++;
								}
							}
						}
					}

					if (allChildsNum != readyChildsNum || !allChildsNum)
						m_pSeg->m_arrChildOffset[childId] = -m_pSeg->m_arrChildOffset[childId];
				}
			}
			/*	else if(m_arrChildNotNeeded[childId])
			   {
			    m_pCloud->m_arrChildOffset[childId] = 0;//-3000;
			   }*/
		}

		if (m_pSeg->m_bChildOffsetsDirty == 2)
			m_pSeg->UpdateNodeRenderData();

		m_pSeg->m_bChildOffsetsDirty = 0;
	}
}

bool IntersectRayAABB(const Ray& r, const Vec3& m1, const Vec3& m2, float& tmin, float& tmax)
{
	float tymin, tymax, tzmin, tzmax;
	float flag = 1.0;

	if (r.direction.x >= 0)
	{
		tmin = (m1.x - r.origin.x) / r.direction.x;
		tmax = (m2.x - r.origin.x) / r.direction.x;
	}
	else
	{
		tmin = (m2.x - r.origin.x) / r.direction.x;
		tmax = (m1.x - r.origin.x) / r.direction.x;
	}
	if (r.direction.y >= 0)
	{
		tymin = (m1.y - r.origin.y) / r.direction.y;
		tymax = (m2.y - r.origin.y) / r.direction.y;
	}
	else
	{
		tymin = (m2.y - r.origin.y) / r.direction.y;
		tymax = (m1.y - r.origin.y) / r.direction.y;
	}

	if ((tmin > tymax) || (tymin > tmax)) flag = -1.0;
	if (tymin > tmin) tmin = tymin;
	if (tymax < tmax) tmax = tymax;

	if (r.direction.z >= 0)
	{
		tzmin = (m1.z - r.origin.z) / r.direction.z;
		tzmax = (m2.z - r.origin.z) / r.direction.z;
	}
	else
	{
		tzmin = (m2.z - r.origin.z) / r.direction.z;
		tzmax = (m1.z - r.origin.z) / r.direction.z;
	}
	if ((tmin > tzmax) || (tzmin > tmax)) flag = -1.0;
	if (tzmin > tmin) tmin = tzmin;
	if (tzmax < tmax) tmax = tzmax;

	return (flag > 0);
}

bool CSvoNode::IsStreamingInProgress()
{
	if (m_pSeg && m_pSeg->m_eStreamingStatus == ecss_InProgress)
		return true;

	if (m_ppChilds)
		for (int childId = 0; childId < 8; childId++)
			if (m_ppChilds[childId] && m_ppChilds[childId]->IsStreamingInProgress())
				return true;

	return false;
}

void CSvoNode::GetTrisInAreaStats(int& trisCount, int& vertCount, int& trisBytes, int& vertBytes, int& maxVertPerArea, int& matsCount)
{

	if (m_pSeg && m_pSeg->m_pTrisInArea)
	{
		if (m_pSeg && m_pSeg->m_pTrisInArea)
		{
			AUTO_READLOCK(m_pSeg->m_superMeshLock);

			trisBytes += m_pSeg->m_pTrisInArea->ComputeSizeInMemory();
			vertBytes += m_pSeg->m_pVertInArea->ComputeSizeInMemory();

			trisCount += m_pSeg->m_pTrisInArea->Count();
			vertCount += m_pSeg->m_pVertInArea->Count();

			maxVertPerArea = max(maxVertPerArea, m_pSeg->m_pVertInArea->Count());

			matsCount += m_pSeg->m_pMatsInArea->Count();
		}
	}
	else if (m_ppChilds)
	{
		for (int childId = 0; childId < 8; childId++)
			if (m_ppChilds[childId])
				m_ppChilds[childId]->GetTrisInAreaStats(trisCount, vertCount, trisBytes, vertBytes, maxVertPerArea, matsCount);
	}
}

void CSvoNode::GetVoxSegMemUsage(int& allocated)
{
	if (m_pSeg)
	{
		allocated += m_pSeg->m_nodeTrisAllMerged.capacity() * sizeof(int);
		allocated += sizeof(*m_pSeg);
	}

	if (m_ppChilds)
	{
		for (int childId = 0; childId < 8; childId++)
			if (m_ppChilds[childId])
				m_ppChilds[childId]->GetVoxSegMemUsage(allocated);
	}
}

void CSvoNode::DeleteChilds()
{
	if (m_ppChilds)
	{
		for (int childId = 0; childId < 8; childId++)
			SAFE_DELETE(m_ppChilds[childId]);
		SAFE_DELETE_ARRAY(m_ppChilds);
	}
}

bool CPointTreeNode::GetAllPointsInTheBox(const AABB& testBox, const AABB& nodeBox, PodArray<int>& arrIds)
{
	if (m_pPoints)
	{
		// leaf
		for (int s = 0; s < m_pPoints->Count(); s++)
		{
			Vec3& vPos = (*m_pPoints)[s].vPos;

			if (testBox.IsContainPoint(vPos))
			{
				arrIds.Add((*m_pPoints)[s].id);
			}
		}
	}

	if (m_ppChilds)
	{
		for (int childId = 0; childId < 8; childId++)
		{
			if (m_ppChilds[childId])
			{
				AABB childBox = GetChildBBox(childId, nodeBox);
				if (Overlap::AABB_AABB(testBox, childBox))
					if (m_ppChilds[childId]->GetAllPointsInTheBox(testBox, childBox, arrIds))
						return true;
			}
		}
	}

	return false;
}

bool CPointTreeNode::IsThereAnyPointInTheBox(const AABB& testBox, const AABB& nodeBox)
{
	if (m_pPoints)
	{
		// leaf
		for (int s = 0; s < m_pPoints->Count(); s++)
		{
			Vec3& vPos = (*m_pPoints)[s].vPos;

			if (testBox.IsContainPoint(vPos))
			{
				return true;
			}
		}
	}

	if (m_ppChilds)
	{
		for (int childId = 0; childId < 8; childId++)
		{
			if (m_ppChilds[childId])
			{
				AABB childBox = GetChildBBox(childId, nodeBox);
				if (Overlap::AABB_AABB(testBox, childBox))
					if (m_ppChilds[childId]->IsThereAnyPointInTheBox(testBox, childBox))
						return true;
			}
		}
	}

	return false;
}

bool CPointTreeNode::TryInsertPoint(int pointId, const Vec3& vPos, const AABB& nodeBox, int recursionLevel)
{
	if (recursionLevel >= 7)
	{
		if (!m_pPoints)
			m_pPoints = new PodArray<SPointInfo>;
		SPointInfo info;
		info.vPos = vPos;
		info.id = pointId;
		m_pPoints->Add(info);
		return true;
	}

	Vec3 vNodeCenter = nodeBox.GetCenter();

	int childId =
	  ((vPos.x >= vNodeCenter.x) ? 4 : 0) |
	  ((vPos.y >= vNodeCenter.y) ? 2 : 0) |
	  ((vPos.z >= vNodeCenter.z) ? 1 : 0);

	if (!m_ppChilds)
	{
		m_ppChilds = new CPointTreeNode*[8];
		memset(m_ppChilds, 0, sizeof(m_ppChilds[0]) * 8);
	}

	AABB childBox = GetChildBBox(childId, nodeBox);

	if (!m_ppChilds[childId])
		m_ppChilds[childId] = new CPointTreeNode();

	return m_ppChilds[childId]->TryInsertPoint(pointId, vPos, childBox, recursionLevel + 1);
}

void CPointTreeNode::Clear()
{
	if (m_ppChilds)
		for (int childId = 0; childId < 8; childId++)
			SAFE_DELETE(m_ppChilds[childId]);
	SAFE_DELETE_ARRAY(m_ppChilds);
	SAFE_DELETE_ARRAY(m_pPoints);
}

AABB CPointTreeNode::GetChildBBox(int childId, const AABB& nodeBox)
{
	int x = (childId / 4);
	int y = (childId - x * 4) / 2;
	int z = (childId - x * 4 - y * 2);
	Vec3 vSize = nodeBox.GetSize() * 0.5f;
	Vec3 vOffset = vSize;
	vOffset.x *= x;
	vOffset.y *= y;
	vOffset.z *= z;
	AABB childBox;
	childBox.min = nodeBox.min + vOffset;
	childBox.max = childBox.min + vSize;
	return childBox;
}

bool CSvoEnv::GetSvoStaticTextures(I3DEngine::SSvoStaticTexInfo& svoInfo, PodArray<I3DEngine::SLightTI>* pLightsTI_S, PodArray<I3DEngine::SLightTI>* pLightsTI_D)
{
	AUTO_LOCK(m_csLockTree);

	svoInfo.pTexTree = GetRenderer()->EF_GetTextureByID(m_texNodePoolId);
	svoInfo.pTexOpac = GetRenderer()->EF_GetTextureByID(m_texOpasPoolId);

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	svoInfo.pTexTris = GetRenderer()->EF_GetTextureByID(m_texTrisPoolId);
	#endif
	svoInfo.pTexRgb0 = GetRenderer()->EF_GetTextureByID(m_texRgb0PoolId);
	svoInfo.pTexRgb1 = GetRenderer()->EF_GetTextureByID(m_texRgb1PoolId);
	svoInfo.pTexDynl = GetRenderer()->EF_GetTextureByID(m_texDynlPoolId);
	svoInfo.pTexRgb2 = GetRenderer()->EF_GetTextureByID(m_texRgb2PoolId);
	svoInfo.pTexRgb3 = GetRenderer()->EF_GetTextureByID(m_texRgb3PoolId);
	svoInfo.pTexRgb4 = GetRenderer()->EF_GetTextureByID(m_texRgb4PoolId);
	svoInfo.pTexNorm = GetRenderer()->EF_GetTextureByID(m_texNormPoolId);
	svoInfo.pTexAldi = GetRenderer()->EF_GetTextureByID(m_texAldiPoolId);
	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	svoInfo.pTexTriA = GetRenderer()->EF_GetTextureByID(gSvoEnv->m_arrRTPoolTris.m_textureId);
	svoInfo.pTexTexA = GetRenderer()->EF_GetTextureByID(gSvoEnv->m_arrRTPoolTexs.m_textureId);
	svoInfo.pTexIndA = GetRenderer()->EF_GetTextureByID(gSvoEnv->m_arrRTPoolInds.m_textureId);
	#endif

	GetGlobalEnvProbeProperties(svoInfo.pGlobalSpecCM, svoInfo.fGlobalSpecCM_Mult);

	svoInfo.nTexDimXY = CVoxelSegment::m_voxTexPoolDimXY;
	svoInfo.nTexDimZ = CVoxelSegment::m_voxTexPoolDimZ;
	svoInfo.nBrickSize = SVO_VOX_BRICK_MAX_SIZE;

	svoInfo.bSvoReady = (m_streamingStartTime < 0);
	svoInfo.bSvoFreeze = (m_svoFreezeTime >= 0) || m_bRootTeleportSkipFrame;

	m_bRootTeleportSkipFrame = false;

	ZeroStruct(svoInfo.arrPortalsPos);
	ZeroStruct(svoInfo.arrPortalsDir);
	if (GetCVars()->e_svoTI_PortalsDeform || GetCVars()->e_svoTI_PortalsInject)
	{
		if (IVisArea* pCurVisArea = GetVisAreaManager()->GetCurVisArea())
		{
			PodArray<IVisArea*> visitedAreas;
			pCurVisArea->FindSurroundingVisArea(2, true, &visitedAreas, 32);

			int portalCounter = 0;
			for (int areaID = 0; areaID < visitedAreas.Count(); areaID++)
			{
				IVisArea* pPortal = visitedAreas[areaID];

				if (!pPortal || portalCounter >= SVO_MAX_PORTALS)
					break;

				if (!pPortal->IsPortal())
					continue;

				if (!pPortal->IsConnectedToOutdoor())
					continue;

				AABB boxEx = *pPortal->GetAABBox();
				boxEx.Expand(Vec3(GetCVars()->e_svoTI_ConeMaxLength, GetCVars()->e_svoTI_ConeMaxLength, GetCVars()->e_svoTI_ConeMaxLength));
				if (!gEnv->pSystem->GetViewCamera().IsAABBVisible_E(boxEx))
					continue;

				svoInfo.arrPortalsPos[portalCounter] = Vec4(pPortal->GetAABBox()->GetCenter(), pPortal->GetAABBox()->GetRadius());
				svoInfo.arrPortalsDir[portalCounter] = Vec4(((CVisArea*)pCurVisArea)->GetConnectionNormal((CVisArea*)pPortal), 0);

				portalCounter++;
			}
		}
	}

	ZeroStruct(svoInfo.arrAnalyticalOccluders);
	for (int stage = 0; stage < 2; stage++)
	{
		if (m_analyticalOccluders[stage].Count())
		{
			uint32 bytesToCopy = min((unsigned int)sizeof(svoInfo.arrAnalyticalOccluders[stage]), m_analyticalOccluders[stage].GetDataSize());
			memcpy(&svoInfo.arrAnalyticalOccluders[stage][0], m_analyticalOccluders[stage].GetElements(), bytesToCopy);
		}
	}

	svoInfo.vSvoOriginAndSize = m_vSvoOriginAndSize;

	if (GetCVars()->e_svoTI_UseTODSkyColor)
	{
		ITimeOfDay::SVariableInfo varCol, varMul;
		Get3DEngine()->GetTimeOfDay()->GetVariableInfo(ITimeOfDay::PARAM_FOG_COLOR2, varCol);
		Get3DEngine()->GetTimeOfDay()->GetVariableInfo(ITimeOfDay::PARAM_FOG_COLOR2_MULTIPLIER, varMul);
		svoInfo.vSkyColorTop = Vec3(varCol.fValue[0] * varMul.fValue[0], varCol.fValue[1] * varMul.fValue[0], varCol.fValue[2] * varMul.fValue[0]);
		Get3DEngine()->GetTimeOfDay()->GetVariableInfo(ITimeOfDay::PARAM_FOG_COLOR, varCol);
		Get3DEngine()->GetTimeOfDay()->GetVariableInfo(ITimeOfDay::PARAM_FOG_COLOR_MULTIPLIER, varMul);
		svoInfo.vSkyColorBottom = Vec3(varCol.fValue[0] * varMul.fValue[0], varCol.fValue[1] * varMul.fValue[0], varCol.fValue[2] * varMul.fValue[0]);

		float aver = (svoInfo.vSkyColorTop.x + svoInfo.vSkyColorTop.y + svoInfo.vSkyColorTop.z) / 3;
		svoInfo.vSkyColorTop.SetLerp(Vec3(aver, aver, aver), svoInfo.vSkyColorTop, GetCVars()->e_svoTI_UseTODSkyColor);
		aver = (svoInfo.vSkyColorBottom.x + svoInfo.vSkyColorBottom.y + svoInfo.vSkyColorBottom.z) / 3;
		svoInfo.vSkyColorBottom.SetLerp(Vec3(aver, aver, aver), svoInfo.vSkyColorBottom, GetCVars()->e_svoTI_UseTODSkyColor);
	}
	else
	{
		svoInfo.vSkyColorBottom = svoInfo.vSkyColorTop = Vec3(1.f, 1.f, 1.f);
	}

	*pLightsTI_S = m_lightsTI_S;
	*pLightsTI_D = m_lightsTI_D;

	return /*!GetCVars()->e_svoQuad && */ svoInfo.pTexTree != 0;
}

void CSvoEnv::GetSvoBricksForUpdate(PodArray<I3DEngine::SSvoNodeInfo>& arrNodeInfo, float nodeSize, PodArray<SVF_P3F_C4B_T2F>* pVertsOut)
{
	FUNCTION_PROFILER_3DENGINE;

	AUTO_LOCK(m_csLockTree);

	arrNodeInfo.Clear();

	if (!CVoxelSegment::m_pBlockPacker)
		return;

	if (pVertsOut)
		*pVertsOut = m_arrSvoProxyVertices;

	if (!GetCVars()->e_svoTI_Active && !GetCVars()->e_svoDVR)
		return;

	const uint numBlocks = CVoxelSegment::m_pBlockPacker->GetNumBlocks();

	bool bSyncAll = 1;

	if (nodeSize == 0)
	{
		gSvoEnv->m_dynNodeCounter = 0;

		double fCheckVal = 0;
		fCheckVal += GetCVars()->e_svoTI_DiffuseConeWidth;
		fCheckVal += GetCVars()->e_svoTI_InjectionMultiplier;
		fCheckVal += GetCVars()->e_svoTI_PropagationBooster;
		fCheckVal += GetCVars()->e_svoTI_NumberOfBounces;
		fCheckVal += GetCVars()->e_svoTI_DynLights;
		fCheckVal += GetCVars()->e_svoTI_Saturation;
		fCheckVal += GetCVars()->e_svoTI_SkyColorMultiplier;
		fCheckVal += GetCVars()->e_svoTI_ConeMaxLength;
		fCheckVal += GetCVars()->e_svoTI_DiffuseBias * 10;
		fCheckVal += GetCVars()->e_svoMinNodeSize;
		fCheckVal += GetCVars()->e_svoMaxNodeSize;
		fCheckVal += (float)(m_streamingStartTime < 0);
		fCheckVal += GetCVars()->e_Sun;
		static int e_svoTI_Troposphere_Active_Max = 0;
		e_svoTI_Troposphere_Active_Max = max(e_svoTI_Troposphere_Active_Max, GetCVars()->e_svoTI_Troposphere_Active);
		fCheckVal += (float)e_svoTI_Troposphere_Active_Max;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_Brightness;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_Ground_Height;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_Layer0_Height;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_Layer1_Height;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_Snow_Height;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_Layer0_Rand;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_Layer1_Rand;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_Layer0_Dens * 10;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_Layer1_Dens * 10;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_CloudGen_Height;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_CloudGen_Freq;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_CloudGen_FreqStep;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_CloudGen_Scale;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_CloudGenTurb_Freq;
		fCheckVal += GetCVars()->e_svoTI_Troposphere_CloudGenTurb_Scale;
		fCheckVal += GetCVars()->e_svoTI_HalfresKernelSecondary;
		fCheckVal += GetCVars()->e_svoTI_Diffuse_Cache;
		fCheckVal += GetCVars()->e_svoTI_PortalsInject * 10;
		fCheckVal += GetCVars()->e_svoTI_SunRSMInject;
		fCheckVal += GetCVars()->e_svoTI_EmissiveMultiplier;
		fCheckVal += GetCVars()->e_svoTI_PointLightsMultiplier;
		fCheckVal += GetCVars()->e_svoTI_PointLightsBias;

		//		fCheckVal += int(Get3DEngine()->GetSunDir().x*10);
		//	fCheckVal += int(Get3DEngine()->GetSunDir().y*10);
		//fCheckVal += int(Get3DEngine()->GetSunDir().z*10);

		if (m_prevCheckVal == -1000000)
			m_prevCheckVal = fCheckVal;

		bSyncAll = (m_prevCheckVal != (fCheckVal));

		{
			static int s_prevFlagAsync = GetCVars()->e_svoTI_UpdateLighting;
			if (GetCVars()->e_svoTI_UpdateLighting && !s_prevFlagAsync)
				bSyncAll = true;
			s_prevFlagAsync = GetCVars()->e_svoTI_UpdateLighting;
		}

		{
			static int s_prevFlagAsync = (gSvoEnv->m_svoFreezeTime <= 0);
			if ((gSvoEnv->m_svoFreezeTime <= 0) && !s_prevFlagAsync)
			{
				bSyncAll = true;
				gSvoEnv->m_svoFreezeTime = -1;
			}
			s_prevFlagAsync = (gSvoEnv->m_svoFreezeTime <= 0);
		}

		m_prevCheckVal = fCheckVal;

		if (bSyncAll)
			PrintMessage("SVO radiance full update");

		bool bEditing = gEnv->IsEditing();

		if (GetCVars()->e_svoTI_UpdateLighting && GetCVars()->e_svoTI_IntegrationMode)
		{
			int autoUpdateVoxNum = 0;

			int maxVox = GetCVars()->e_svoTI_Reflect_Vox_Max * (int)30 / (int)max(30.f, gEnv->pTimer->GetFrameRate());
			int maxVoxEdit = GetCVars()->e_svoTI_Reflect_Vox_MaxEdit * (int)30 / (int)max(30.f, gEnv->pTimer->GetFrameRate());

			for (uint n = 0; n < numBlocks; n++)
			{
				static uint s_autoBlockId = 0;
				if (s_autoBlockId >= numBlocks)
					s_autoBlockId = 0;

				if (SBlockMinMax* pBlock = CVoxelSegment::m_pBlockPacker->GetBlockInfo(s_autoBlockId))
				{
					CVoxelSegment* pSeg = (CVoxelSegment*)pBlock->m_pUserData;

					if ((pSeg->GetBoxSize() >= nodeSize && pSeg->GetBoxSize() <= GetCVars()->e_svoMaxNodeSize) || !nodeSize)
						if (pSeg->m_pNode->m_requestSegmentUpdateFrametId < GetCurrPassMainFrameID() - 30)
						{
							if (autoUpdateVoxNum && (autoUpdateVoxNum + (int(sqrt((float)pSeg->m_solidVoxelsNum)) + GetCVars()->e_svoTI_Reflect_Vox_Max_Overhead) > maxVox))
							{
								if (bEditing)
								{
									if (pSeg->m_bStatLightsChanged)
										if ((pSeg->GetBoxSize() >= nodeSize && pSeg->GetBoxSize() <= GetCVars()->e_svoMaxNodeSize) || !nodeSize)
										//if(pSeg->m_pNode->m_nRequestBrickUpdateFrametId < GetCurrPassMainFrameID()-30)
										{
											if (autoUpdateVoxNum && (autoUpdateVoxNum + (int(sqrt((float)pSeg->m_solidVoxelsNum)) + GetCVars()->e_svoTI_Reflect_Vox_Max_Overhead) > maxVoxEdit))
											{
											}
											else
											{
												pSeg->m_pNode->m_requestSegmentUpdateFrametId = max(pSeg->m_pNode->m_requestSegmentUpdateFrametId, GetCurrPassMainFrameID());
												autoUpdateVoxNum += (int(sqrt((float)pSeg->m_solidVoxelsNum)) + GetCVars()->e_svoTI_Reflect_Vox_Max_Overhead);
											}
										}
								}
								else
									break;
							}
							else
							{
								pSeg->m_pNode->m_requestSegmentUpdateFrametId = max(pSeg->m_pNode->m_requestSegmentUpdateFrametId, GetCurrPassMainFrameID());
								autoUpdateVoxNum += (int(sqrt((float)pSeg->m_solidVoxelsNum)) + GetCVars()->e_svoTI_Reflect_Vox_Max_Overhead);
							}
						}
				}

				s_autoBlockId++;
			}
		}
	}

	static uint s_blockId = 0;

	uint maxUpdateBlocks = (bSyncAll || (GetCVars()->e_svoDVR != 10)) ? numBlocks : (numBlocks / 16 + 1);

	m_dynNodeCounter_DYNL = 0;

	for (uint b = 0; b < maxUpdateBlocks; b++)
	{
		s_blockId++;
		if (s_blockId >= numBlocks)
			s_blockId = 0;

		if (SBlockMinMax* pBlock = CVoxelSegment::m_pBlockPacker->GetBlockInfo(s_blockId))
		{
			CVoxelSegment* pSeg = (CVoxelSegment*)pBlock->m_pUserData;

			//			if( ((GetMainFrameID()%nNumBlocks)&63) == (nBlockId&63) && pSeg->m_pNode->m_nRequestBrickUpdateFrametId>0 && GetCVars()->e_svoDVR )
			//			pSeg->m_pNode->m_nRequestBrickUpdateFrametId = GetMainFrameID();

			if (pSeg->m_pNode->m_requestSegmentUpdateFrametId >= GetCurrPassMainFrameID() || bSyncAll || pSeg->m_bStatLightsChanged)
			{
				if (nodeSize == 0)
					gSvoEnv->m_dynNodeCounter++;

				if ((pSeg->GetBoxSize() >= nodeSize && pSeg->GetBoxSize() <= GetCVars()->e_svoMaxNodeSize) || !nodeSize)
				//					if( pSeg->GetBoxSize() <= GetCVars()->e_svoMaxNodeSize )
				{
					I3DEngine::SSvoNodeInfo nodeInfo;

					nodeInfo.wsBox.min = pSeg->m_boxOS.min + pSeg->m_vSegOrigin;
					nodeInfo.wsBox.max = pSeg->m_boxOS.max + pSeg->m_vSegOrigin;

					nodeInfo.tcBox.min.Set((float)pBlock->m_dwMinX / (float)SVO_ATLAS_DIM_MAX_XY, (float)pBlock->m_dwMinY / (float)SVO_ATLAS_DIM_MAX_XY, (float)pBlock->m_dwMinZ / (float)SVO_ATLAS_DIM_MAX_Z);
					nodeInfo.tcBox.max.Set((float)pBlock->m_dwMaxX / (float)SVO_ATLAS_DIM_MAX_XY, (float)pBlock->m_dwMaxY / (float)SVO_ATLAS_DIM_MAX_XY, (float)pBlock->m_dwMaxZ / (float)SVO_ATLAS_DIM_MAX_Z);

					nodeInfo.nAtlasOffset = pSeg->m_allocatedAtlasOffset;

					if (nodeSize)
					{
						if (nodeInfo.wsBox.GetDistance(Get3DEngine()->GetRenderingCamera().GetPosition()) > 24.f)//nodeInfo.wsBox.GetSize().z)
							continue;

						//						if(!Overlap::AABB_AABB(nodeInfo.wsBox, m_aabbLightsTI_D))
						//						continue;

						AABB wsBoxEx = nodeInfo.wsBox;
						wsBoxEx.Expand(Vec3(2, 2, 2));
						if (!gEnv->pSystem->GetViewCamera().IsAABBVisible_F(wsBoxEx))
							continue;

						m_dynNodeCounter_DYNL++;
					}

					pSeg->m_bStatLightsChanged = 0;

					arrNodeInfo.Add(nodeInfo);
				}
			}
		}
	}
}

void CSvoEnv::DetectMovement_StaticGeom()
{
	FUNCTION_PROFILER_3DENGINE;

	if (!CVoxelSegment::m_pBlockPacker)
		return;

	if (!GetCVars()->e_svoTI_Apply)
		return;

	const uint numBlocks = CVoxelSegment::m_pBlockPacker->GetNumBlocks();
	for (uint blockId = 0; blockId < numBlocks; blockId++)
	{
		if (SBlockMinMax* pBlock = CVoxelSegment::m_pBlockPacker->GetBlockInfo(blockId))
		{
			CVoxelSegment* pSeg = (CVoxelSegment*)pBlock->m_pUserData;

			Vec3i vCheckSumm = pSeg->m_pNode->GetStatGeomCheckSumm();

			//if( !pSeg->m_vStaticGeomCheckSumm.IsEquivalent(vCheckSumm, 2) )
			{
				if (pSeg->m_eStreamingStatus != ecss_NotLoaded)
				{
					CSvoNode* pNode = pSeg->m_pNode;
					while (pNode)
					{
						pNode->m_bForceRecreate = true;
						if (pNode->m_nodeBox.GetSize().z >= GetCVars()->e_svoMaxNodeSize)
							break;
						pNode = pNode->m_pParent;
					}
				}

				pSeg->m_vStaticGeomCheckSumm = vCheckSumm;
			}
		}
	}
}

void CSvoEnv::DetectMovement_StatLights()
{
	FUNCTION_PROFILER_3DENGINE;

	if (!CVoxelSegment::m_pBlockPacker)
		return;

	if (!GetCVars()->e_svoTI_Apply)
		return;

	// count stats
	const int dataSizeStatsScale = 4;
	CVoxelSegment::m_poolUsageItems = 0;
	CVoxelSegment::m_poolUsageBytes = 0;

	const uint numBlocks = CVoxelSegment::m_pBlockPacker->GetNumBlocks();
	for (uint blockId = 0; blockId < numBlocks; blockId++)
	{
		if (SBlockMinMax* pBlock = CVoxelSegment::m_pBlockPacker->GetBlockInfo(blockId))
		{
			CVoxelSegment::m_poolUsageItems++;
			CVoxelSegment::m_poolUsageBytes += dataSizeStatsScale * pBlock->m_nDataSize;

			CVoxelSegment* pSeg = (CVoxelSegment*)pBlock->m_pUserData;

			float nodeSize = pSeg->GetBoxSize();

			PodArray<IRenderNode*> lstObjects;
			AABB boxEx = pSeg->m_pNode->m_nodeBox;
			float border = nodeSize / 4.f + GetCVars()->e_svoTI_ConeMaxLength * max(0, GetCVars()->e_svoTI_NumberOfBounces - 1);
			boxEx.Expand(Vec3(border, border, border));
			Cry3DEngineBase::Get3DEngine()->GetObjectsByTypeGlobal(lstObjects, eERType_Light, &boxEx);
			Cry3DEngineBase::Get3DEngine()->GetVisAreaManager()->GetObjectsByType(lstObjects, eERType_Light, &boxEx);

			float precisioin = 1000.f;
			Vec3i vCheckSumm(0, 0, 0);

			for (int i = 0; i < lstObjects.Count(); i++)
			{
				IRenderNode* pNode = lstObjects[i];

				if (pNode->GetGIMode() == IRenderNode::eGM_StaticVoxelization)
				{
					if (pNode->GetRenderNodeType() == eERType_Light)
					{
						ILightSource* pLS = (ILightSource*)pNode;

						SRenderLight& m_light = pLS->GetLightProperties();

						if (!Overlap::Sphere_AABB(Sphere(m_light.m_BaseOrigin, m_light.m_fClipRadius), pSeg->m_pNode->m_nodeBox))
							continue;

						if ((m_light.m_Flags & DLF_PROJECT) && (m_light.m_fLightFrustumAngle < 90.f) && m_light.m_pLightImage)
						{
							CCamera lightCam = gEnv->pSystem->GetViewCamera();
							lightCam.SetPositionNoUpdate(m_light.m_Origin);
							Matrix34 entMat = ((ILightSource*)(m_light.m_pOwner))->GetMatrix();
							entMat.OrthonormalizeFast();
							Matrix33 matRot = Matrix33::CreateRotationVDir(entMat.GetColumn(0));
							lightCam.SetMatrixNoUpdate(Matrix34(matRot, m_light.m_Origin));
							lightCam.SetFrustum(1, 1, (m_light.m_fLightFrustumAngle * 2) / 180.0f * gf_PI, 0.1f, m_light.m_fRadius);
							if (!lightCam.IsAABBVisible_F(pSeg->m_pNode->m_nodeBox))
								continue;

							vCheckSumm.x += int(m_light.m_fLightFrustumAngle);
							vCheckSumm += Vec3i(entMat.GetColumn(0) * 10.f);
						}

						if (m_light.m_Flags & DLF_CASTSHADOW_MAPS)
							vCheckSumm.z += 10;

						vCheckSumm += Vec3i(m_light.m_BaseColor.toVec3() * 50.f);

						if (m_light.m_Flags & DLF_SUN)
						{
							//							vCheckSumm.x += int(Get3DEngine()->GetSunDir().x*50);
							//						vCheckSumm.y += int(Get3DEngine()->GetSunDir().y*50);
							//					vCheckSumm.z += int(Get3DEngine()->GetSunDir().z*50);
							continue;
						}
					}

					AABB box = pNode->GetBBox();
					vCheckSumm += Vec3i(box.min * precisioin);
					vCheckSumm += Vec3i(box.max * precisioin * 2);
				}
			}

			if (!vCheckSumm.IsZero())
			{
				vCheckSumm.x += (int)GetCVars()->e_svoVoxNodeRatio;
				vCheckSumm.y += (int)GetCVars()->e_svoVoxDistRatio;
			}

			if (!pSeg->m_vStatLightsCheckSumm.IsEquivalent(vCheckSumm, 2))
			{
				//				if(!pSeg->m_vStatLightsCheckSumm.IsZero())
				pSeg->m_bStatLightsChanged = 1;
				pSeg->m_vStatLightsCheckSumm = vCheckSumm;
			}
		}
	}
}

void CSvoEnv::StartupStreamingTimeTest(bool bDone)
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_svoFreezeTime > 0 && bDone && m_bStreamingDonePrev)
	{
		PrintMessage("SVO update finished in %.1f sec (%d / %d nodes, %d K tris)",
		             gEnv->pTimer->GetAsyncCurTime() - m_svoFreezeTime, m_arrVoxelizeMeshesCounter[0], m_arrVoxelizeMeshesCounter[1], CVoxelSegment::m_voxTrisCounter / 1000);
		m_svoFreezeTime = 0;

		if (GetCVars()->e_svoDebug)
		{
			AUTO_MODIFYLOCK(CVoxelSegment::m_cgfTimeStatsLock);
			PrintMessage("Voxelization time spend per CFG:");
			for (auto it = CVoxelSegment::m_cgfTimeStats.begin(); it != CVoxelSegment::m_cgfTimeStats.end(); ++it)
				if (it->second > 1.f)
					PrintMessage("  %4.1f sec %s", it->second, it->first->GetFilePath());
		}
	}

	if (m_streamingStartTime == 0)
	{
		m_streamingStartTime = Cry3DEngineBase::GetTimer()->GetAsyncCurTime();
	}
	else if (m_streamingStartTime > 0 && bDone && m_bStreamingDonePrev)
	{
		//		float time = Cry3DEngineBase::GetTimer()->GetAsyncCurTime();
		//		PrintMessage("SVO initialization finished in %.1f sec (%d K tris)",
		//		time - m_fStreamingStartTime, CVoxelSegment::m_voxTrisCounter/1000);

		m_streamingStartTime = -1;

		OnLevelGeometryChanged();

		GetCVars()->e_svoMaxBrickUpdates = 4;
		GetCVars()->e_svoMaxStreamRequests = 4;

		int maxBricksOnGPU = SVO_ATLAS_DIM_BRICKS_XY * SVO_ATLAS_DIM_BRICKS_XY * SVO_ATLAS_DIM_BRICKS_Z;
		GetCVars()->e_svoMaxBricksOnCPU = maxBricksOnGPU * 3 / 2;

		//		if(m_pTree)
		//		m_pTree->UpdateTerrainNormals();
	}

	m_bStreamingDonePrev = bDone;
}

void CSvoEnv::OnLevelGeometryChanged()
{

}

int CSvoEnv::GetWorstPointInSubSet(const int start, const int end)
{
	int p0 = -1;

	float minAverDist = 100000000;

	for (int i = start; i < end; i++)
	{
		float averDist = 0;

		for (int j = 0; j < kRaysNum; j++)
		{
			float dist = sqrt(sqrt(pKernelPoints[i].GetSquaredDistance2D(pKernelPoints[j])));

			if (j >= start && j < end)
				averDist += dist;
			else
				averDist -= dist;
		}

		if (averDist < minAverDist)
		{
			p0 = i;
			minAverDist = averDist;
		}
	}

	return p0;
}

void CSvoEnv::CheckUpdateMeshPools()
{
	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	{
		PodArrayRT<Vec4>& rAr = gSvoEnv->m_arrRTPoolTris;

		AUTO_READLOCK(rAr.m_Lock);

		if (rAr.m_bModified)
		{
			gEnv->pRenderer->RemoveTexture(rAr.m_textureId);
			rAr.m_textureId = 0;

			rAr.m_textureId = gEnv->pRenderer->UploadToVideoMemory3D((byte*)rAr.GetElements(),
			                                                         CVoxelSegment::m_voxTexPoolDimXY, CVoxelSegment::m_voxTexPoolDimXY, CVoxelSegment::m_voxTexPoolDimZ, eTF_R32G32B32A32F, eTF_R32G32B32A32F, 1, false, FILTER_POINT, rAr.m_textureId, 0, FT_DONT_STREAM);

			rAr.m_bModified = 0;
		}
	}

	{
		PodArrayRT<ColorB>& rAr = gSvoEnv->m_arrRTPoolTexs;

		AUTO_READLOCK(rAr.m_Lock);

		if (rAr.m_bModified)
		{
			gEnv->pRenderer->RemoveTexture(rAr.m_textureId);
			rAr.m_textureId = 0;

			rAr.m_textureId = gEnv->pRenderer->UploadToVideoMemory3D((byte*)rAr.GetElements(),
			                                                         CVoxelSegment::m_voxTexPoolDimXY, CVoxelSegment::m_voxTexPoolDimXY, CVoxelSegment::m_voxTexPoolDimZ, m_voxTexFormat, m_voxTexFormat, 1, false, FILTER_POINT, rAr.m_textureId, 0, FT_DONT_STREAM);

			rAr.m_bModified = 0;
		}
	}

	{
		PodArrayRT<ColorB>& rAr = gSvoEnv->m_arrRTPoolInds;

		AUTO_READLOCK(rAr.m_Lock);

		if (rAr.m_bModified)
		{
			gEnv->pRenderer->RemoveTexture(rAr.m_textureId);
			rAr.m_textureId = 0;

			rAr.m_textureId = gEnv->pRenderer->UploadToVideoMemory3D((byte*)rAr.GetElements(),
			                                                         CVoxelSegment::m_voxTexPoolDimXY, CVoxelSegment::m_voxTexPoolDimXY, CVoxelSegment::m_voxTexPoolDimZ, m_voxTexFormat, m_voxTexFormat, 1, false, FILTER_POINT, rAr.m_textureId, 0, FT_DONT_STREAM);

			rAr.m_bModified = 0;
		}
	}
	#endif
}

bool C3DEngine::GetSvoStaticTextures(I3DEngine::SSvoStaticTexInfo& svoInfo, PodArray<I3DEngine::SLightTI>* pLightsTI_S, PodArray<I3DEngine::SLightTI>* pLightsTI_D)
{
	return CSvoManager::GetSvoStaticTextures(svoInfo, pLightsTI_S, pLightsTI_D);
}

void C3DEngine::GetSvoBricksForUpdate(PodArray<SSvoNodeInfo>& arrNodeInfo, float nodeSize, PodArray<SVF_P3F_C4B_T2F>* pVertsOut)
{
	CSvoManager::GetSvoBricksForUpdate(arrNodeInfo, nodeSize, pVertsOut);
}

bool C3DEngine::IsSvoReady(bool testPostponed) const
{
	return (CVoxelSegment::m_streamingTasksInProgress == 0) && ((CVoxelSegment::m_postponedCounter == 0) || (testPostponed == false)) &&
	       (CVoxelSegment::m_updatesInProgressBri == 0) && (CVoxelSegment::m_updatesInProgressTex == 0) && (CVoxelSegment::m_bUpdateBrickRenderDataPostponed == 0);
}

int C3DEngine::GetSvoCompiledData(ICryArchive* pArchive)
{
	return CSvoManager::ExportSvo(pArchive);
}

void C3DEngine::LoadTISettings(XmlNodeRef pInputNode)
{
	const char* szXmlNodeName = "Total_Illumination_v2";

	// Total illumination
	if (GetCVars()->e_svoTI_Active >= 0)
		GetCVars()->e_svoTI_Apply = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Active", "0"));

	GetCVars()->e_svoVoxelPoolResolution = 64;
	int voxelPoolResolution = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "VoxelPoolResolution", "0"));
	while (GetCVars()->e_svoVoxelPoolResolution < voxelPoolResolution)
		GetCVars()->e_svoVoxelPoolResolution *= 2;

	GetCVars()->e_svoTI_VoxelizationLODRatio = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "VoxelizationLODRatio", "1"));
	GetCVars()->e_svoTI_VoxelizationMapBorder = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "VoxelizationMapBorder", "0"));
	GetCVars()->e_svoDVR_DistRatio = GetCVars()->e_svoTI_VoxelizationLODRatio / 2;

	GetCVars()->e_svoTI_InjectionMultiplier = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "InjectionMultiplier", "0"));
	GetCVars()->e_svoTI_NumberOfBounces = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "NumberOfBounces", "0"));
	GetCVars()->e_svoTI_Saturation = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Saturation", "0"));
	GetCVars()->e_svoTI_PropagationBooster = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "PropagationBooster", "0"));
	GetCVars()->e_svoTI_PointLightsBias = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "PointLightsBias", "0"));
	if (gEnv->IsEditor())
	{
		GetCVars()->e_svoTI_UpdateLighting = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "UpdateLighting", "0"));
		GetCVars()->e_svoTI_UpdateGeometry = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "UpdateGeometry", "0"));
	}
	GetCVars()->e_svoTI_SkyColorMultiplier = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "SkyColorMultiplier", "0"));
	GetCVars()->e_svoTI_UseLightProbes = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "UseLightProbes", "0"));
	if (!GetCVars()->e_svoTI_UseLightProbes && GetCVars()->e_svoTI_SkyColorMultiplier >= 0)
		GetCVars()->e_svoTI_SkyColorMultiplier = -GetCVars()->e_svoTI_SkyColorMultiplier - .0001f;
	GetCVars()->e_svoTI_ConeMaxLength = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "ConeMaxLength", "0"));

	GetCVars()->e_svoTI_DiffuseConeWidth = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "DiffuseConeWidth", "0"));
	GetCVars()->e_svoTI_DiffuseBias = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "DiffuseBias", "0"));
	GetCVars()->e_svoTI_DiffuseAmplifier = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "DiffuseAmplifier", "0"));
	if (GetCVars()->e_svoTI_Diffuse_Cache)
		GetCVars()->e_svoTI_NumberOfBounces++;

	GetCVars()->e_svoTI_SpecularAmplifier = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "SpecularAmplifier", "0"));
	GetCVars()->e_svoTI_SpecularFromDiffuse = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "SpecularFromDiffuse", "0"));

	GetCVars()->e_svoMinNodeSize = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "MinNodeSize", "0"));

	GetCVars()->e_svoTI_SkipNonGILights = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "SkipNonGILights", "0"));
	GetCVars()->e_svoTI_ForceGIForAllLights = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "ForceGIForAllLights", "0"));
	GetCVars()->e_svoTI_SSAOAmount = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "SSAOAmount", "0"));
	GetCVars()->e_svoTI_PortalsDeform = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "PortalsDeform", "0"));
	GetCVars()->e_svoTI_PortalsInject = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "PortalsInject", "0"));
	GetCVars()->e_svoTI_ObjectsMaxViewDistance = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "ObjectsMaxViewDistance", "0"));
	GetCVars()->e_svoTI_SunRSMInject = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "SunRSMInject", "0"));
	GetCVars()->e_svoTI_SSDepthTrace = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "SSDepthTrace", "0"));

	GetCVars()->e_svoTI_ShadowsFromSun = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "ShadowsFromSun", "0"));
	GetCVars()->e_svoTI_ShadowsSoftness = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "ShadowsSoftness", "0"));
	GetCVars()->e_svoTI_ShadowsFromHeightmap = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "ShadowsFromHeightmap", "0"));
	GetCVars()->e_svoTI_Troposphere_Active = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_Active", "0"));
	GetCVars()->e_svoTI_Troposphere_Brightness = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_Brightness", "0"));
	GetCVars()->e_svoTI_Troposphere_Ground_Height = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_Ground_Height", "0"));
	GetCVars()->e_svoTI_Troposphere_Layer0_Height = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_Layer0_Height", "0"));
	GetCVars()->e_svoTI_Troposphere_Layer1_Height = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_Layer1_Height", "0"));
	GetCVars()->e_svoTI_Troposphere_Snow_Height = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_Snow_Height", "0"));
	GetCVars()->e_svoTI_Troposphere_Layer0_Rand = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_Layer0_Rand", "0"));
	GetCVars()->e_svoTI_Troposphere_Layer1_Rand = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_Layer1_Rand", "0"));
	GetCVars()->e_svoTI_Troposphere_Layer0_Dens = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_Layer0_Dens", "0"));
	GetCVars()->e_svoTI_Troposphere_Layer1_Dens = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_Layer1_Dens", "0"));
	//GetCVars()->e_svoTI_Troposphere_CloudGen_Height = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_CloudGen_Height", "0"));
	GetCVars()->e_svoTI_Troposphere_CloudGen_Freq = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_CloudGen_Freq", "0"));
	GetCVars()->e_svoTI_Troposphere_CloudGen_FreqStep = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_CloudGen_FreqStep", "0"));
	GetCVars()->e_svoTI_Troposphere_CloudGen_Scale = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_CloudGen_Scale", "0"));
	GetCVars()->e_svoTI_Troposphere_CloudGenTurb_Freq = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_CloudGenTurb_Freq", "0"));
	GetCVars()->e_svoTI_Troposphere_CloudGenTurb_Scale = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_CloudGenTurb_Scale", "0"));
	GetCVars()->e_svoTI_Troposphere_Density = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_Density", "0"));
	//if (GetCVars()->e_svoTI_Troposphere_Subdivide >= 0)
	//GetCVars()->e_svoTI_Troposphere_Subdivide = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Troposphere_Subdivide", "0"));
	GetCVars()->e_svoTI_Troposphere_Subdivide = GetCVars()->e_svoTI_Troposphere_Active;

	GetCVars()->e_svoTI_AnalyticalOccluders = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "AnalyticalOccluders", "0"));
	GetCVars()->e_svoTI_AnalyticalGI = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "AnalyticalGI", "0"));
	GetCVars()->e_svoTI_TraceVoxels = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "TraceVoxels", "1"));

	int lowSpecMode = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "LowSpecMode", "0"));
	if (lowSpecMode > -2 && gEnv->IsEditor()) // otherwise we use value from sys_spec_Light.cfg
		GetCVars()->e_svoTI_LowSpecMode = lowSpecMode;

	GetCVars()->e_svoTI_HalfresKernelPrimary = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "HalfresKernelPrimary", "0"));
	GetCVars()->e_svoTI_HalfresKernelSecondary = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "HalfresKernelSecondary", "0"));
	GetCVars()->e_svoTI_UseTODSkyColor = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "UseTODSkyColor", "0"));

	GetCVars()->e_svoTI_HighGlossOcclusion = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "HighGlossOcclusion", "0"));
	GetCVars()->e_svoTI_TranslucentBrightness = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "TranslucentBrightness", "2.5"));

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	GetCVars()->e_svoTI_IntegrationMode = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "IntegrationMode", "0"));
	#else
	GetCVars()->e_svoTI_IntegrationMode = 0;
	#endif

	GetCVars()->e_svoTI_RT_MaxDist = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "RT_MaxDist", "0"));

	GetCVars()->e_svoTI_ConstantAmbientDebug = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "ConstantAmbientDebug", "0"));

	if (Cry3DEngineBase::GetCVars()->e_svoStreamVoxels != 2)
		GetCVars()->e_svoStreamVoxels = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "StreamVoxels", "0"));

	// fall back to run-time voxelization if voxel file data is not prepared
	if (CSvoNode::IsStreamingActive())
	{
		char szFileName[256];
		const char* pLevelPath = Cry3DEngineBase::Get3DEngine()->GetLevelFilePath(COMPILED_SVO_FOLDER_NAME);
		sprintf(szFileName, "%s*.crysvo", pLevelPath);

		_finddata_t fd;
		if (gEnv->pCryPak->FindFirst(szFileName, &fd) < 0)
		{
			PrintMessage("%s: Error: Streaming of voxels is enabled in level settings but files are missing. Falling back to run-time voxelization", __FUNCTION__);
			GetCVars()->e_svoStreamVoxels = 0;
		}
	}

	if (GetCVars()->e_svoTI_IntegrationMode < 1) // AO
	{
		GetCVars()->e_svoTI_NumberOfBounces = min(GetCVars()->e_svoTI_NumberOfBounces, 1);
	}

	if (GetCVars()->e_svoTI_ConstantAmbientDebug)
	{
		GetCVars()->e_svoTI_DiffuseAmplifier = 0;
		GetCVars()->e_svoTI_DiffuseBias = GetCVars()->e_svoTI_ConstantAmbientDebug;
		GetCVars()->e_svoTI_SpecularAmplifier = 0;
	}

	// validate MinNodeSize
	float size = (float)Get3DEngine()->GetTerrainSize();
	for (; size > 0.01f && size >= GetCVars()->e_svoMinNodeSize * 2.f; size /= 2.f)
		;
	GetCVars()->e_svoMinNodeSize = size;
}

void CVars::RegisterTICVars()
{
	#define CVAR_CPP
	#include "SceneTreeCVars.inl"

	if (GetRenderer())
	{
		GetRenderer()->GetISvoRenderer()->InitCVarValues(); // allocate SVO sub-system in renderer
	}
}

static int32 SLightTI_Compare(const void* v1, const void* v2)
{
	I3DEngine::SLightTI* p[2] = { (I3DEngine::SLightTI*)v1, (I3DEngine::SLightTI*)v2 };

	if (p[0]->fSortVal > p[1]->fSortVal)
		return 1;
	if (p[0]->fSortVal < p[1]->fSortVal)
		return -1;

	return 0;
}

void CSvoEnv::CollectLights()
{
	FUNCTION_PROFILER_3DENGINE;

	float areaRange = 128.f;
	if (!GetCVars()->e_svoTI_IntegrationMode)
		areaRange = GetCVars()->e_svoTI_PointLightsMaxDistance;

	const CCamera& camera = GetISystem()->GetViewCamera();
	AABB nodeBox(camera.GetPosition() - Vec3(areaRange), camera.GetPosition() + Vec3(areaRange));

	m_lightsTI_S.Clear();
	m_lightsTI_D.Clear();
	m_aabbLightsTI_D.Reset();

	Vec3 vCamPos = CVoxelSegment::m_voxCam.GetPosition();

	if (GetCVars()->e_svoTI_IntegrationMode || GetCVars()->e_svoTI_PointLightsMultiplier)
	{
		if (int objCount = gEnv->p3DEngine->GetObjectsByTypeInBox(eERType_Light, nodeBox, (IRenderNode**)0))
		{
			PodArray<IRenderNode*> arrObjects(objCount, objCount);
			objCount = gEnv->p3DEngine->GetObjectsByTypeInBox(eERType_Light, nodeBox, arrObjects.GetElements());

			for (int n = 0; n < objCount; n++)
			{
				ILightSource* pRN = (ILightSource*)arrObjects[n];

				static ICVar* e_svoMinNodeSize = gEnv->pConsole->GetCVar("e_svoMinNodeSize");

				SRenderLight& rLight = pRN->GetLightProperties();

				I3DEngine::SLightTI lightTI;
				ZeroStruct(lightTI);

				IRenderNode::EGIMode eVoxMode = pRN->GetGIMode();

				if (eVoxMode == IRenderNode::eGM_DynamicVoxelization || eVoxMode == IRenderNode::eGM_StaticVoxelization)
				{
					lightTI.vPosR = Vec4(rLight.m_Origin, rLight.m_fRadius);

					bool bProjector = (rLight.m_Flags & DLF_PROJECT) && (rLight.m_fLightFrustumAngle < 90.f) && rLight.m_pLightImage;
					Vec3 vLitAreaCenter;
					float litAreaRadius;

					// set direction info
					if (bProjector)
					{
						lightTI.vDirF = Vec4(pRN->GetMatrix().GetColumn(0), rLight.m_fLightFrustumAngle * 2);
						vLitAreaCenter = rLight.m_Origin + pRN->GetMatrix().GetColumn(0) * rLight.m_fRadius * 0.5f;
						litAreaRadius = rLight.m_fRadius * 0.5f + GetCVars()->e_svoTI_RsmConeMaxLength * 0.5f;
					}
					else
					{
						lightTI.vDirF = Vec4(0, 0, 0, 0);
						vLitAreaCenter = rLight.m_Origin;
						litAreaRadius = rLight.m_fRadius + GetCVars()->e_svoTI_RsmConeMaxLength * 0.5f;
					}

					// frustum culling
					Sphere sp(vLitAreaCenter, litAreaRadius);
					if (!camera.IsSphereVisible_F(sp))
						continue;

					// set color
					if (eVoxMode == IRenderNode::eGM_DynamicVoxelization)
						lightTI.vCol = rLight.m_Color.toVec4();
					else
						lightTI.vCol = rLight.m_BaseColor.toVec4();

					lightTI.vCol.w = (rLight.m_Flags & DLF_CASTSHADOW_MAPS) ? 1.f : 0.f;

					// set sort value
					if (rLight.m_Flags & DLF_SUN)
					{
						lightTI.fSortVal = -1;
					}
					else
					{
						float distance = vCamPos.GetDistance(vLitAreaCenter);

						if (distance > GetCVars()->e_svoTI_PointLightsMaxDistance)
							continue;

						lightTI.fSortVal = distance / max(1.f, rLight.m_fRadius);
						lightTI.fSortVal *= 1.f / max(0.01f, rLight.m_BaseColor.Luminance());
					}

					if (eVoxMode == IRenderNode::eGM_DynamicVoxelization || (eVoxMode == IRenderNode::eGM_StaticVoxelization && !GetCVars()->e_svoTI_IntegrationMode && !(rLight.m_Flags & DLF_SUN)))
					{
						if ((pRN->GetDrawFrame(0) > 10) && (pRN->GetDrawFrame(0) >= (int)GetCurrPassMainFrameID()))
						{
							m_lightsTI_D.Add(lightTI);
							m_aabbLightsTI_D.Add(pRN->GetBBox());
						}
					}
					else
					{
						m_lightsTI_S.Add(lightTI);
					}
				}
			}

			if (m_lightsTI_S.Count() > 1)
				qsort(m_lightsTI_S.GetElements(), m_lightsTI_S.Count(), sizeof(m_lightsTI_S[0]), SLightTI_Compare);

			if (m_lightsTI_D.Count() > 1)
				qsort(m_lightsTI_D.GetElements(), m_lightsTI_D.Count(), sizeof(m_lightsTI_D[0]), SLightTI_Compare);

			if (m_lightsTI_D.Count() > 8)
				m_lightsTI_D.PreAllocate(8, 8);
		}
	}
}

static int32 SAnalyticalOccluder_Compare(const void* v1, const void* v2)
{
	I3DEngine::SAnalyticalOccluder* p[2] = { (I3DEngine::SAnalyticalOccluder*)v1, (I3DEngine::SAnalyticalOccluder*)v2 };

	float d0 = p[0]->c.GetSquaredDistance(gEnv->pSystem->GetViewCamera().GetPosition());
	float d1 = p[1]->c.GetSquaredDistance(gEnv->pSystem->GetViewCamera().GetPosition());

	if (d0 > d1)
		return 1;
	if (d0 < d1)
		return -1;

	return 0;
}

void CSvoEnv::CollectAnalyticalOccluders()
{
	FUNCTION_PROFILER_3DENGINE;

	for (int stageId = 0; stageId < 2; stageId++)
		m_analyticalOccluders[stageId].Clear();

	if (!GetCVars()->e_svoTI_AnalyticalOccluders && !GetCVars()->e_svoTI_AnalyticalGI)
		return;

	AABB areaBox;
	areaBox.Reset();
	areaBox.Add(gEnv->pSystem->GetViewCamera().GetPosition());
	areaBox.Expand(Vec3(GetCVars()->e_svoTI_ConeMaxLength, GetCVars()->e_svoTI_ConeMaxLength, GetCVars()->e_svoTI_ConeMaxLength));

	Vec3 vCamPos = CVoxelSegment::m_voxCam.GetPosition();

	// collect from static entities
	if (int objCount = gEnv->p3DEngine->GetObjectsByTypeInBox(eERType_MovableBrush, areaBox, (IRenderNode**)0))
	{
		PodArray<IRenderNode*> arrObjects(objCount, objCount);
		objCount = gEnv->p3DEngine->GetObjectsByTypeInBox(eERType_MovableBrush, areaBox, arrObjects.GetElements());

		Vec3 camPos = gEnv->pSystem->GetViewCamera().GetPosition();

		for (int n = 0; n < objCount; n++)
		{
			IRenderNode* pRN = arrObjects[n];

			AABB aabbEx = pRN->GetBBox();
			aabbEx.Expand(Vec3(GetCVars()->e_svoTI_ConeMaxLength, GetCVars()->e_svoTI_ConeMaxLength, GetCVars()->e_svoTI_ConeMaxLength));

			if (!gEnv->pSystem->GetViewCamera().IsAABBVisible_E(aabbEx))
				continue;

			AddAnalyticalOccluder(pRN, camPos);
		}
	}

	// collect from characters
	if (int objCount = gEnv->p3DEngine->GetObjectsByTypeInBox(eERType_Character, areaBox, (IRenderNode**)0))
	{
		PodArray<IRenderNode*> arrObjects(objCount, objCount);
		objCount = gEnv->p3DEngine->GetObjectsByTypeInBox(eERType_Character, areaBox, arrObjects.GetElements());

		Vec3 camPos = gEnv->pSystem->GetViewCamera().GetPosition();

		for (int n = 0; n < objCount; n++)
		{
			IRenderNode* pRN = arrObjects[n];

			AABB aabbEx = pRN->GetBBox();
			aabbEx.Expand(Vec3(GetCVars()->e_svoTI_ConeMaxLength, GetCVars()->e_svoTI_ConeMaxLength, GetCVars()->e_svoTI_ConeMaxLength));

			if (!gEnv->pSystem->GetViewCamera().IsAABBVisible_E(aabbEx))
				continue;

			AddAnalyticalOccluder(pRN, camPos);
		}
	}

	for (int stageId = 0; stageId < 2; stageId++)
	{
		// remove invalid objects
		for (int n = 0; n < m_analyticalOccluders[stageId].Count(); n++)
		{
			if (m_analyticalOccluders[stageId][n].v0.IsZero() || m_analyticalOccluders[stageId][n].v1.IsZero())
			{
				m_analyticalOccluders[stageId].DeleteFastUnsorted(n);
				n--;
				continue;
			}

			if (m_analyticalOccluders[stageId][n].type == (float)I3DEngine::SAnalyticalOccluder::eCapsule)
				m_analyticalOccluders[stageId][n].c = (m_analyticalOccluders[stageId][n].v0 + m_analyticalOccluders[stageId][n].v1) * 0.5f;
		}

		// sort by importance
		if (m_analyticalOccluders[stageId].Count() > 1)
			qsort(m_analyticalOccluders[stageId].GetElements(), m_analyticalOccluders[stageId].Count(), sizeof(m_analyticalOccluders[stageId][0]), SAnalyticalOccluder_Compare);
	}
}

void CSvoEnv::AddAnalyticalOccluder(IRenderNode* pRN, Vec3 camPos)
{
	Matrix34A matParent;

	// make a sphere, box or capsule from geom entity
	if (CStatObj* pStatObj = (CStatObj*)pRN->GetEntityStatObj(0, &matParent, true))
	{
		bool bPO = (pRN->GetGIMode() == IRenderNode::eGM_AnalytPostOccluder) && GetCVars()->e_svoTI_AnalyticalOccluders;
		bool bAO = (pRN->GetGIMode() == IRenderNode::eGM_AnalyticalProxy_Soft) && (GetCVars()->e_svoTI_AnalyticalGI || GetCVars()->e_svoTI_AnalyticalOccluders);
		bool bAOHard = (pRN->GetGIMode() == IRenderNode::eGM_AnalyticalProxy_Hard) && (GetCVars()->e_svoTI_AnalyticalGI || GetCVars()->e_svoTI_AnalyticalOccluders);

		if (bPO || bAO || bAOHard)
		{
			bool bSphere = strstr(pStatObj->GetFilePath(), "sphere") != NULL;
			bool bCube = strstr(pStatObj->GetFilePath(), "cube") != NULL;
			bool bCylinder = strstr(pStatObj->GetFilePath(), "cylinder") != NULL;

			if (bCube || bCylinder)
			{
				Vec3 vCenter = pStatObj->GetAABB().GetCenter();
				Vec3 vX = Vec3(1, 0, 0);
				Vec3 vY = Vec3(0, 1, 0);
				Vec3 vZ = Vec3(0, 0, 1);

				vCenter = matParent.TransformPoint(vCenter);
				vX = matParent.TransformVector(vX);
				vY = matParent.TransformVector(vY);
				vZ = matParent.TransformVector(vZ);

				I3DEngine::SAnalyticalOccluder obb;

				obb.v0 = vX.GetNormalized();
				obb.e0 = vX.GetLength();
				obb.v1 = vY.GetNormalized();
				obb.e1 = vY.GetLength();
				obb.v2 = vZ.GetNormalized();
				obb.e2 = vZ.GetLength();
				obb.c = vCenter;

				if (bCube)
					obb.type = (float)I3DEngine::SAnalyticalOccluder::eOBB;
				else
					obb.type = (float)I3DEngine::SAnalyticalOccluder::eCylinder;

				if (bAOHard)
					obb.type += 2.f;

				m_analyticalOccluders[bPO].Add(obb);
			}
			else if (bSphere)
			{
				I3DEngine::SAnalyticalOccluder capsule;
				capsule.type = (float)I3DEngine::SAnalyticalOccluder::eCapsule;

				Vec3 vCenter = pStatObj->GetAABB().GetCenter();
				Vec3 vZ = Vec3(0, 0, 1);
				Vec3 vX = Vec3(1, 0, 0);

				vCenter = matParent.TransformPoint(vCenter);
				vZ = matParent.TransformVector(vZ);
				vX = matParent.TransformVector(vX);

				capsule.v0 = vCenter + vZ;
				capsule.v1 = vCenter - vZ;
				capsule.radius = vX.GetLength();

				m_analyticalOccluders[bPO].Add(capsule);
			}
		}
	}

	if (GetCVars()->e_svoTI_AnalyticalOccluders)
	{
		if (ICharacterInstance* pCharacter = (ICharacterInstance*)pRN->GetEntityCharacter(&matParent))
		{
			bool bPO = true;

			AABB aabbEx = pRN->GetBBox();
			aabbEx.Expand(Vec3(GetCVars()->e_svoTI_AnalyticalOccludersRange, GetCVars()->e_svoTI_AnalyticalOccludersRange, GetCVars()->e_svoTI_AnalyticalOccludersRange));

			if (!gEnv->pSystem->GetViewCamera().IsAABBVisible_E(aabbEx))
				return;

			// build capsules from character bones (bone names must be specified)
			if (ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose())
			{
				IDefaultSkeleton& rDefaultSkeleton = pCharacter->GetIDefaultSkeleton();

				const DynArray<SBoneShadowCapsule>& capsuleInfos = rDefaultSkeleton.GetShadowCapsules();

				AABB aabb = pRN->GetBBox();

				// for distant objects build single capsule from AABB
				if (capsuleInfos.size() && aabb.GetDistance(camPos) > 12.f)
				{
					I3DEngine::SAnalyticalOccluder capsule;
					capsule.type = (float)I3DEngine::SAnalyticalOccluder::eCapsule;

					Vec3 vSize = aabb.GetSize();
					Vec3 vCenter = aabb.GetCenter();

					capsule.radius = min(0.3f, (vSize.x + vSize.y) * 0.25f);

					capsule.v0 = Vec3(vCenter.x, vCenter.y, max(vCenter.z + .01f, aabb.max.z - capsule.radius));
					capsule.v1 = Vec3(vCenter.x, vCenter.y, min(vCenter.z - .01f, aabb.min.z + capsule.radius));

					m_analyticalOccluders[bPO].Add(capsule);

					return;
				}

				for (int32 capsuleId = 0; capsuleId < capsuleInfos.size(); capsuleId++)
				{
					const SBoneShadowCapsule& capsuleInfo = capsuleInfos[capsuleId];

					Vec3 arrBonePositions[2];

					for (int jointSlot = 0; jointSlot < 2; jointSlot++)
					{
						int jointID = capsuleInfo.arrJoints[jointSlot];

						if (jointID >= 0)
						{
							Matrix34A tm34 = matParent * Matrix34(pSkeletonPose->GetAbsJointByID(jointID));
							arrBonePositions[jointSlot] = tm34.GetTranslation();
						}
						else
						{
							arrBonePositions[jointSlot].zero();
						}
					}

					I3DEngine::SAnalyticalOccluder capsule;
					capsule.type = (float)I3DEngine::SAnalyticalOccluder::eCapsule;

					capsule.radius = capsuleInfo.radius;
					capsule.v0 = arrBonePositions[0];
					capsule.v1 = arrBonePositions[1];

					m_analyticalOccluders[bPO].Add(capsule);
				}
			}
		}
	}
}

void CSvoEnv::GetGlobalEnvProbeProperties(_smart_ptr<ITexture>& specEnvCM, float& mult)
{
	AUTO_LOCK(m_csLockGlobalEnvProbe);

	if (!m_pGlobalEnvProbe)
	{
		specEnvCM = nullptr;
		mult = 1.0f;
		return;
	}

	specEnvCM = m_pGlobalEnvProbe->GetSpecularCubemap();
	mult = m_pGlobalEnvProbe->GetSpecularMult();
}

bool CSvoNode::IsStreamingActive()
{
	return (!gEnv->IsEditor() && Cry3DEngineBase::GetCVars()->e_svoStreamVoxels) || Cry3DEngineBase::GetCVars()->e_svoStreamVoxels == 2;
}

void CSvoNode::CheckAllocateChilds()
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_nodeBox.GetSize().x <= Cry3DEngineBase::GetCVars()->e_svoMinNodeSize)
		return;

	const int currPassMainFrameID = GetCurrPassMainFrameID();

	m_pSeg->m_lastRendFrameId = currPassMainFrameID;

	for (int childId = 0; childId < 8; childId++)
	{
		bool bChildNotNeeded = (!m_pChildFileOffsets || !m_pChildFileOffsets[childId].second) && IsStreamingActive() && (m_nodeBox.GetSize().x < SVO_ROOTLESS_PARENT_SIZE);

		if (m_arrChildNotNeeded[childId] || bChildNotNeeded)
		{
			if (m_ppChilds && m_ppChilds[childId])
			{
				SAFE_DELETE(m_ppChilds[childId]);
			}
			continue;
		}

		AABB childBox = GetChildBBox(childId);

		if (!IsStreamingActive() && Cry3DEngineBase::GetCVars()->e_svoTI_VoxelizationPostpone && (!m_ppChilds || !m_ppChilds[childId]) && (childBox.GetSize().z == Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize))
		{
			bool bThisIsAreaParent, bThisIsLowLodNode;
			if (!CVoxelSegment::CheckCollectObjectsForVoxelization(childBox, nullptr, bThisIsAreaParent, bThisIsLowLodNode, true))
			{
				assert(!CVoxelSegment::m_bExportMode || !"Some meshes are not streamed in, stream cgf must be off for now during EXPORT processs");

				if (Cry3DEngineBase::GetCVars()->e_svoDebug == 7)
					Cry3DEngineBase::Get3DEngine()->DrawBBox(childBox, Col_Lime);
				CVoxelSegment::m_postponedCounter++;
				continue;
			}
		}

		//					if((m_pCloud->m_dwChildTrisTest & (1<<childId)) ||
		//					(m_pCloud->GetBoxSize() > Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize) || (Cry3DEngineBase::GetCVars()->e_rsDebugAI&2) || 1)
		{
			if (!m_ppChilds)
			{
				m_ppChilds = new CSvoNode*[8];
				memset(m_ppChilds, 0, sizeof(m_ppChilds[0]) * 8);
			}

			CSvoNode* pChild = m_ppChilds[childId];

			if (pChild)
			{
				if (pChild->m_pSeg)
				{
					pChild->m_pSeg->m_lastRendFrameId = currPassMainFrameID;
				}

				continue;
			}

			{
				m_ppChilds[childId] = new CSvoNode(childBox, this);

				m_ppChilds[childId]->AllocateSegment(CVoxelSegment::m_nextSegmentId++, 0, 0, ecss_NotLoaded, true);

				if (IsStreamingActive() && m_nodeBox.GetSize().x <= SVO_ROOTLESS_PARENT_SIZE)
				{
					if (m_ppChilds[childId]->m_nodeBox.GetSize().x == SVO_ROOTLESS_PARENT_SIZE / 2)
					{
						char szAreaFileName[256];
						m_ppChilds[childId]->MakeNodeFilePath(szAreaFileName);

						if (gEnv->pCryPak->IsFileExist(szAreaFileName))
						{
							//
							m_ppChilds[childId]->m_pSeg->m_fileStreamOffset64 = 0;

							const int parentNodeFileStreamSize = 132;
							//assert(parentNodeFileStreamSize == (sizeof(int) + sizeof(uint32) * 2 * 8));

							m_ppChilds[childId]->m_pSeg->m_fileStreamSize = parentNodeFileStreamSize;
						}
						else
						{
							m_arrChildNotNeeded[childId] = true;

							SAFE_DELETE(m_ppChilds[childId]);
						}
					}
					else
					{
						m_ppChilds[childId]->m_pSeg->m_fileStreamOffset64 = m_pChildFileOffsets[childId].first;

						m_ppChilds[childId]->m_pSeg->m_fileStreamSize = m_pChildFileOffsets[childId].second;
					}
				}
			}
		}
	}
}

void* CSvoNode::operator new(size_t)
{
	return gSvoEnv->m_nodeAllocator.GetNewElement();
}

void CSvoNode::operator delete(void* ptr)
{
	gSvoEnv->m_nodeAllocator.ReleaseElement((CSvoNode*)ptr);
}

#endif
