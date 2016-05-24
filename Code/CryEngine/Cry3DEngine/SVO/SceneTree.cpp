// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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

template<class T>
struct SThreadSafeTask
{
	SThreadSafeTask(int n)
	{
		nRequestFrameId = 0;
		pTaskObject = 0;
	}
	int nRequestFrameId;
	T   pTaskObject;
};

template<class T>
struct CTaskConveyor
{
	SThreadSafeArray<SThreadSafeTask<T>, 64>*   pTasks_Compute;
	SThreadSafeArray<SThreadSafeTask<T>, 1024>* pTasks_Store;
	volatile int                                nThreadsCounter;
	bool   nThreadsContinue;
	int    nRequestFrameId;
	string szConveyorName;

	struct SThreadInfo
	{
		CTaskConveyor* pTaskConveyor;
		int            nThreadSlotId;
	};

	SThreadInfo arrThreadInfo[16];

	CTaskConveyor(const char* szName)
		: pTasks_Compute(nullptr)
		, pTasks_Store(nullptr)
		, nThreadsCounter(0)
		, nThreadsContinue(true)
		, nRequestFrameId(0)
		, szConveyorName(szName)
		, arrThreadInfo()
	{
		Start();
	}

	~CTaskConveyor()
	{
		Stop();
	}

	static void ThreadFunc_Compute(void* pUserData)
	{
		SThreadInfo* pInf = (SThreadInfo*)pUserData;
		CTaskConveyor* pTC = pInf->pTaskConveyor;

		CryInterlockedIncrement(&pTC->nThreadsCounter);

		while (pTC->nThreadsContinue)
		{
			while (1)
			{
				SThreadSafeTask<T> nextTask = pTC->pTasks_Compute->GetNextTaskFromQeueOrdered2();

				if (!nextTask.pTaskObject)
					break;

				nextTask.pTaskObject->Task_Compute(pInf->nThreadSlotId);

				pTC->pTasks_Store->AddNewTaskToQeue(nextTask);
			}

			CrySleep(1);
		}

		CryInterlockedDecrement(&pTC->nThreadsCounter);
	}

	static void ThreadFunc_Store(void* pUserData)
	{
		SThreadInfo* pInf = (SThreadInfo*)pUserData;
		CTaskConveyor* pTC = pInf->pTaskConveyor;

		CryInterlockedIncrement(&pTC->nThreadsCounter);

		while (pTC->nThreadsContinue)
		{
			while (1)
			{
				SThreadSafeTask<T> nextTask = pTC->pTasks_Store->GetNextTaskFromQeueOrdered2();

				if (!nextTask.pTaskObject)
					break;

				nextTask.pTaskObject->Task_Store();
			}

			CrySleep(1);
		}

		CryInterlockedDecrement(&pTC->nThreadsCounter);
	}

	void Start()
	{
	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT

		pTasks_Compute = new SThreadSafeArray<SThreadSafeTask<T>, 64>;
		pTasks_Store = new SThreadSafeArray<SThreadSafeTask<T>, 1024>;

		int nThreadSlotId = 0;

		for (; nThreadSlotId < 7; nThreadSlotId++)
		{
			arrThreadInfo[nThreadSlotId].pTaskConveyor = this;
			arrThreadInfo[nThreadSlotId].nThreadSlotId = nThreadSlotId;
			_beginthread(ThreadFunc_Compute, 0, (void*)&arrThreadInfo[nThreadSlotId]);
		}

		arrThreadInfo[nThreadSlotId].pTaskConveyor = this;
		arrThreadInfo[nThreadSlotId].nThreadSlotId = nThreadSlotId;
		_beginthread(ThreadFunc_Store, 0, (void*)&arrThreadInfo[nThreadSlotId]);

		while (nThreadsCounter < 8)
			CrySleep(10);

	#endif
	}

	void Stop()
	{
		while (pTasks_Compute->Count() || pTasks_Store->Count())
			CrySleep(10);
		nThreadsContinue = false;
		while (nThreadsCounter)
			CrySleep(10);
		nThreadsContinue = true;

		T(0)->Task_OnTaskConveyorFinish(); // static
	}

	void AddTask(T task, int nTasksAll)
	{
		SThreadSafeTask<T> nextTask(0);
		nextTask.pTaskObject = task;
		nextTask.nRequestFrameId = nRequestFrameId;

		pTasks_Compute->AddNewTaskToQeue(nextTask);

		if (!(nRequestFrameId % 500))
		{
			Cry3DEngineBase::PrintMessage("%s: %d of %d tasks processed, pool %d MB of %d MB, compute=%d and store=%d",
			                              szConveyorName.c_str(),
			                              nRequestFrameId, nTasksAll, CVoxelSegment::GetBrickPoolUsageLoadedMB(), CVoxelSegment::GetBrickPoolUsageMB(),
			                              pTasks_Compute->Count(), pTasks_Store->Count());
		}

		nRequestFrameId++;
	}
};

struct SVoxRndDataFileHdr
{
	ETEX_Format nVoxTexFormat;
	uint32      nVoxFlags;
};

void CSvoEnv::ReconstructTree(bool bMultiPoint)
{
	char szFolder[256] = "";
	CVoxelSegment::MakeFolderName(szFolder);
	CVoxelSegment::m_strRenderDataFileName = szFolder;
	CVoxelSegment::m_strRenderDataFileName += GLOBAL_CLOUD_MESH_FILE_NAME;

	int nNodesCreated = 0;

	if (GetCVars()->e_svoTI_Active)
	{
		PrintMessage("Constructing voxel tree ...");

		m_nVoxTexFormat = eTF_R8G8B8A8;
		PrintMessage("Voxel texture format: %s", GetRenderer()->GetTextureFormatName(m_nVoxTexFormat));

		SAFE_DELETE(m_pSvoRoot);
		m_pSvoRoot = new CSvoNode(m_worldBox, NULL);
		m_pSvoRoot->AllocateSegment(CVoxelSegment::m_nNextCloudId++, 0, ecss_NotLoaded, ecss_NotLoaded, true);
		nNodesCreated++;

		//		PrintMessage("Collecting level geometry . . .");
		//	if(!m_pTree->m_pCloud->m_pNodeTrisXYZ)
		//	m_pTree->m_pCloud->FindTrianglesForVoxelization(0);

		m_bReady = true;

		CVoxelSegment::CheckAllocateTexturePool();
	}
	else if (FILE* f = fopen(CVoxelSegment::m_strRenderDataFileName, "rb"))
	{
		PrintMessage("Loading voxel tree ...");

		// read file header
		SVoxRndDataFileHdr header;
		fread(&header, 1, sizeof(header), f);
		m_nVoxTexFormat = header.nVoxTexFormat;
		if (m_nVoxTexFormat != eTF_BC3 && m_nVoxTexFormat != eTF_R8G8B8A8)
			m_nVoxTexFormat = eTF_BC3; // old file format
		PrintMessage("Voxel texture format: %s", GetRenderer()->GetTextureFormatName(m_nVoxTexFormat));
		fclose(f);

		SAFE_DELETE(m_pSvoRoot);
		m_pSvoRoot = new CSvoNode(m_worldBox, NULL);

		LoadTree(nNodesCreated);

		CVoxelSegment::CheckAllocateTexturePool();
	}

	m_bReady = true;

	if (nNodesCreated > 1)
		PrintMessage("%d tree nodes created", nNodesCreated);
}

void CSvoEnv::LoadTree(int& nNodesCreated)
{
	char szFolder[256] = "";
	CVoxelSegment::MakeFolderName(szFolder);
	string filespec = szFolder;
	filespec += GLOBAL_CLOUD_TREE_FILE_NAME;

	if (FILE* f = fopen(filespec, "rb"))
	{
		PrintMessage("Loading tree from %s", filespec.c_str());

		int nDataSize = 0;
		fread(&nDataSize, 1, sizeof(nDataSize), f);

		PodArray<int> arrData;
		arrData.CheckAllocated(nDataSize / 4);

		int nLoadedItems = fread(arrData.GetElements(), 1, arrData.GetDataSize(), f);
		fclose(f);

		int* pData = arrData.GetElements();

		if (m_pSvoRoot)
		{
			if (Cry3DEngineBase::GetCVars()->e_svoTI_Active)
			{
				m_pSvoRoot->AllocateSegment(CVoxelSegment::m_nNextCloudId++, 0, ecss_NotLoaded, ecss_NotLoaded, true);
				nNodesCreated++;
			}
			else
			{
				m_pSvoRoot->Load(pData, nNodesCreated, NULL, NULL);
			}
		}

		if (pData - arrData.GetElements() == arrData.Count() || Cry3DEngineBase::GetCVars()->e_svoTI_Active)
		{
			PrintMessage("%d K bytes loaded", arrData.GetDataSize() / 1024);
			m_bReady = true;
		}
		else
			PrintMessage("Error loading %s", filespec.c_str());
	}
}

Vec3* GeneratePointsOnHalfSphere(int n);
static Vec3* pKernelPoints = 0;
const int nRaysNum = 512;

bool CSvoEnv::Render()
{
	FUNCTION_PROFILER_3DENGINE;

	AUTO_LOCK(m_csLockTree);

	CVoxelSegment::CheckAllocateTexturePool();

	CollectLights();

	CollectAnalyticalOccluders();

	//if(m_pDiffCM == (ITexture *)-1)
	{
		m_pGlobalSpecCM = 0;
		m_fGlobalSpecCM_Mult = 0;

		if (int nCount = gEnv->p3DEngine->GetObjectsByTypeInBox(eERType_Light, m_worldBox, (IRenderNode**)0))
		{
			PodArray<IRenderNode*> arrObjects(nCount, nCount);
			nCount = gEnv->p3DEngine->GetObjectsByTypeInBox(eERType_Light, m_worldBox, arrObjects.GetElements());

			float fMaxRadius = 999;

			for (int nL = 0; nL < nCount; nL++)
			{
				ILightSource* pRN = (ILightSource*)arrObjects[nL];

				CDLight& rLight = pRN->GetLightProperties();

				if (rLight.m_fRadius > fMaxRadius && rLight.m_Flags & DLF_DEFERRED_CUBEMAPS)
				{
					fMaxRadius = rLight.m_fRadius;
					m_pGlobalSpecCM = rLight.GetSpecularCubemap();
					m_fGlobalSpecCM_Mult = rLight.m_SpecMult;
				}
			}
		}
	}

	static int nPrev_UpdateLighting = GetCVars()->e_svoTI_UpdateLighting;
	if ((!nPrev_UpdateLighting || gEnv->IsEditing()) && GetCVars()->e_svoTI_UpdateLighting && GetCVars()->e_svoTI_IntegrationMode && m_bReady && m_pSvoRoot)
		DetectMovement_StatLights();
	nPrev_UpdateLighting = GetCVars()->e_svoTI_UpdateLighting;

	static int nPrev_UpdateGeometry = GetCVars()->e_svoTI_UpdateGeometry;
	if (!nPrev_UpdateGeometry && GetCVars()->e_svoTI_UpdateGeometry && m_bReady && m_pSvoRoot)
		DetectMovement_StaticGeom();
	nPrev_UpdateGeometry = GetCVars()->e_svoTI_UpdateGeometry;

	bool bMultiUserMode = false;
	int nUserId = /*gSSRSystem ? gSSRSystem->GetCurRenderUserId(bMultiUserMode) : */ 0;

	if (IEntity* pEnt = GetSystem()->GetIEntitySystem()->FindEntityByName("MRI_Proxy"))
	{
		gSvoEnv->m_matDvrTm = pEnt->GetWorldTM();

		float fScale = 1.f / 32.f;
		Matrix44 mScale;
		mScale.SetIdentity();
		mScale.m00 = mScale.m11 = mScale.m22 = mScale.m33 = fScale;
		gSvoEnv->m_matDvrTm = gSvoEnv->m_matDvrTm * mScale;

		Matrix44 mTrans;
		mTrans.SetIdentity();
		mTrans.SetTranslation(Vec3(-128 * fScale, -128 * fScale, -128 * fScale));

		gSvoEnv->m_matDvrTm = gSvoEnv->m_matDvrTm * mTrans;
	}
	else
	{
		gSvoEnv->m_matDvrTm.SetIdentity();
	}

	CRenderObject* pObjVox = 0;
	{
		pObjVox = Cry3DEngineBase::GetIdentityCRenderObject((*CVoxelSegment::m_pCurrPassInfo).ThreadID());
		pObjVox->m_II.m_AmbColor = Cry3DEngineBase::Get3DEngine()->GetSkyColor();
		pObjVox->m_II.m_Matrix.SetIdentity();
		pObjVox->m_nSort = 0;
		pObjVox->m_ObjFlags |= (/*FOB_NO_Z_PASS | */ FOB_NO_FOG);

		SRenderObjData* pData = pObjVox->GetObjData();
		pData->m_fTempVars[0] = (float)nAtlasDimMaxXY;
		pData->m_fTempVars[1] = (float)nAtlasDimMaxXY;
		pData->m_fTempVars[2] = (float)nAtlasDimMaxZ;
		pData->m_fTempVars[3] = (float)(nVoxTexMaxDim / nVoxBloMaxDim);
		pData->m_fTempVars[4] = 0;//GetCVars()->e_svoDVR!=1 ? 0 : 11.111f;
	}

	if (m_bReady && m_pSvoRoot)
	{
		//		UpdatePVS();

		static Vec3 arrLastCamPos[16];
		static Vec3 arrLastCamDir[16];
		static float arrLastUpdateTime[16];
		int nPoolId = max(0, nUserId);
		PodArray<SVF_P3F_C4B_T2F>& arrVertsOut = m_arrSvoProxyVertices;//arrVertsOutPool[nPoolId];

		if (fabs(arrLastUpdateTime[nPoolId] - GetCurTimeSec()) > (/*bMultiUserMode ? 0.125f : 0.125f/64*/ 0.125f) || GetCVars()->e_svoEnabled >= 2 || (m_fStreamingStartTime >= 0) || (GetCVars()->e_svoDVR != 10))
		{
			arrLastUpdateTime[nPoolId] = GetCurTimeSec();
			arrLastCamPos[nPoolId] = CVoxelSegment::m_voxCam.GetPosition();
			arrLastCamDir[nPoolId] = CVoxelSegment::m_voxCam.GetViewdir();

			CVoxelSegment::m_nPostponedCounter = CVoxelSegment::m_nCheckReadyCounter = CVoxelSegment::m_nAddPolygonToSceneCounter = 0;

			arrVertsOut.Clear();

			m_pSvoRoot->CheckReadyForRendering(0, m_arrForStreaming);

			m_pSvoRoot->Render(0, 1, pObjVox, 0, arrVertsOut, m_arrForStreaming);

			CheckUpdateMeshPools();
		}

		// render DVR proxies
		/*if(arrVertsOut.Count() && GetCVars()->e_svoDVR == 10)
		   {
		   uint16 nDummyIndex = 0;

		   static SInputShaderResources res;
		   static _smart_ptr< IMaterial > pMat = Cry3DEngineBase::MakeSystemMaterialFromShader("Total_Illumination.RenderDvrProxiesPass_GS", &res);
		   pObjVox->m_pCurrMaterial = pMat;

		   Cry3DEngineBase::GetRenderer()->EF_AddPolygonToScene( pMat->GetShaderItem(),
		    arrVertsOut.Count(), &arrVertsOut[0], NULL, pObjVox, (*CVoxelSegment::m_pCurrPassInfo), &nDummyIndex, 1, false );
		   }*/
	}

	int nMaxLoadedNodes = Cry3DEngineBase::GetCVars()->e_svoMaxBricksOnCPU;

	if (m_nVoxTexFormat != eTF_R8G8B8A8)
		nMaxLoadedNodes *= 4;

	//	if(GetCVars()->e_rsMode != RS_FAT_CLIENT)
	{
		FRAME_PROFILER("CGlobalCloud::Render_StartStreaming", GetSystem(), PROFILE_3DENGINE);

		for (int nTreeLevel = 0; (nTreeLevel < nVoxStreamQueueMaxSize); nTreeLevel++)
			for (int nDistId = 0; (nDistId < nVoxStreamQueueMaxSize); nDistId++)
			{
				if (!m_bFirst_StartStreaming)
					for (int i = 0; (i < m_arrForStreaming[nTreeLevel][nDistId].Count()) && (CVoxelSegment::m_nStreamingTasksInProgress < Cry3DEngineBase::GetCVars()->e_svoMaxStreamRequests); i++)
					{
						if (CVoxelSegment::m_arrLoadedSegments.Count() > nMaxLoadedNodes)
							break;

						if (/*m_arrForStreaming[nTreeLevel][nDistId][i]->m_nFileStreamSize>0 && */ !m_arrForStreaming[nTreeLevel][nDistId][i]->StartStreaming())
							break;
					}

				m_arrForStreaming[nTreeLevel][nDistId].Clear();
			}

		m_bFirst_StartStreaming = false;
	}

	if (CVoxelSegment::m_arrLoadedSegments.Count() > (nMaxLoadedNodes - Cry3DEngineBase::GetCVars()->e_svoMaxStreamRequests))
	{
		{
			FRAME_PROFILER("CGlobalCloud::Render_UnloadStreamable_Sort", GetSystem(), PROFILE_3DENGINE);

			qsort(CVoxelSegment::m_arrLoadedSegments.GetElements(), CVoxelSegment::m_arrLoadedSegments.Count(), sizeof(CVoxelSegment::m_arrLoadedSegments[0]), CVoxelSegment::ComparemLastVisFrameID);
		}

		{
			FRAME_PROFILER("CGlobalCloud::Render_UnloadStreamable_FreeRenderData", GetSystem(), PROFILE_3DENGINE);

			int nNumNodesToDelete = 4 + Cry3DEngineBase::GetCVars()->e_svoMaxStreamRequests;//CVoxelSegment::m_arrLoadedSegments.Count()/1000;

			int nNodesUnloaded = 0;
			while ((nNodesUnloaded < nNumNodesToDelete) &&
			       (CVoxelSegment::m_arrLoadedSegments[nNodesUnloaded]->m_eStreamingStatus != ecss_InProgress) &&
			       (CVoxelSegment::m_arrLoadedSegments[nNodesUnloaded]->m_nLastRendFrameId < (GetCurrPassMainFrameID() - 32)))
			{
				CVoxelSegment::m_arrLoadedSegments[nNodesUnloaded]->FreeRenderData();
				CVoxelSegment::m_arrLoadedSegments[nNodesUnloaded]->m_eStreamingStatus = ecss_NotLoaded;

				if (CSvoNode* pNode = CVoxelSegment::m_arrLoadedSegments[nNodesUnloaded]->m_pNode)
				{
					CSvoNode** ppChilds = CVoxelSegment::m_arrLoadedSegments[nNodesUnloaded]->m_pParentCloud->m_pNode->m_ppChilds;

					for (int nChildId = 0; nChildId < 8; nChildId++)
					{
						if (ppChilds[nChildId] == pNode)
						{
							ppChilds[nChildId] = 0;
						}
					}

					SAFE_DELETE(pNode);
				}

				nNodesUnloaded++;
			}

			CVoxelSegment::m_arrLoadedSegments.Delete(0, nNodesUnloaded);
		}
	}

	CVoxelSegment::UpdateStreamingEngine();

	CVoxelSegment::m_nMaxBrickUpdates = max(1, bMultiUserMode ? Cry3DEngineBase::GetCVars()->e_svoMaxBrickUpdates * 16 : (Cry3DEngineBase::GetCVars()->e_svoMaxBrickUpdates / max(1, Cry3DEngineBase::GetCVars()->e_svoTI_LowSpecMode / (int)2 + 1)));
	if (!(m_fStreamingStartTime < 0)) // if not bSvoReady yet
		CVoxelSegment::m_nMaxBrickUpdates *= 100;

	//if(nUserId == 0 || !bMultiUserMode)
	{
		FRAME_PROFILER("CGlobalCloud::Render_BrickUpdate", GetSystem(), PROFILE_3DENGINE);

		CVoxelSegment::m_nUpdatesInProgressTex = 0;
		CVoxelSegment::m_nUpdatesInProgressBri = 0;

		for (int nTreeLevel = 0; (nTreeLevel < 16); nTreeLevel++)
		{
			for (int i = 0; (i < gSvoEnv->m_arrForBrickUpdate[nTreeLevel].Count()) && (CVoxelSegment::m_nUpdatesInProgressBri < CVoxelSegment::m_nMaxBrickUpdates); i++)
			{
				CVoxelSegment* pSeg = gSvoEnv->m_arrForBrickUpdate[nTreeLevel][i];

				if (pSeg->m_eStreamingStatus == ecss_Ready)
				{
					CVoxelSegment::m_nUpdatesInProgressBri++;

					bool bWas = pSeg->CheckUpdateBrickRenderData(true);

					if (!pSeg->CheckUpdateBrickRenderData(false))
					{
						break;
					}

					if (!bWas)
						pSeg->PropagateDirtyFlag();
				}
			}

			gSvoEnv->m_arrForBrickUpdate[nTreeLevel].Clear();
		}

		if (m_nTexNodePoolId)
		{
			FRAME_PROFILER("CGlobalCloud::Render_UpdateNodeRenderDataPtrs", GetSystem(), PROFILE_3DENGINE);

			m_pSvoRoot->UpdateNodeRenderDataPtrs();
		}
	}

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
		   GetRenderer()->GetISvoRenderer()->GetDynamicTextures(nRGBD, nNORM, nOPAC, NULL, NULL);
		   pObjVox->m_nTextureID = nRGBD;

		   pObjVox->m_pCurrMaterial = pMat;

		   Cry3DEngineBase::GetRenderer()->EF_AddPolygonToScene( pMat->GetShaderItem(),
		   4, &arrVerts[0], NULL, pObjVox, (*CVoxelSegment::m_pCurrPassInfo), &arrIndices[0], 6, false );
		 */
	}
	else if (GetCVars()->e_svoDVR > 1)
	{
		SVF_P3F_C4B_T2F arrVerts[4];
		ZeroStruct(arrVerts);
		uint16 arrIndices[] = { 0, 2, 1, 2, 3, 1 };

		for (int x = 0; x < 2; x++)
		{
			for (int y = 0; y < 2; y++)
			{
				int i = x * 2 + y;
				float X = (float)(GetRenderer()->GetWidth() * x);
				float Y = (float)(GetRenderer()->GetHeight() * y);
				GetRenderer()->UnProjectFromScreen(X, Y, 0.05f, &arrVerts[i].xyz.x, &arrVerts[i].xyz.y, &arrVerts[i].xyz.z);
			}
		}

		SInputShaderResourcesPtr pInputResource = GetRenderer()->EF_CreateInputShaderResource();
		SInputShaderResources& res = *pInputResource;

		static _smart_ptr<IMaterial> pMat = Cry3DEngineBase::MakeSystemMaterialFromShader("Total_Illumination.SvoDebugDraw", &res);
		pObjVox->m_pCurrMaterial = pMat;

		Cry3DEngineBase::GetRenderer()->EF_AddPolygonToScene(pMat->GetShaderItem(),
		                                                     4, &arrVerts[0], NULL, pObjVox, (*CVoxelSegment::m_pCurrPassInfo), &arrIndices[0], 6, false);

		if (ICVar* pV = gEnv->pConsole->GetCVar("r_UseAlphaBlend"))
			pV->Set(1);
	}

	if (Cry3DEngineBase::GetCVars()->e_svoDebug == 4 && 0)
	{
		if (!pKernelPoints)
		{
			pKernelPoints = GeneratePointsOnHalfSphere(nRaysNum);
			//memcpy(pKernelPoints, &arrAO_Kernel[0], sizeof(Vec3)*32);

			Cry3DEngineBase::PrintMessage("Organizing subsets . . .");

			// try to organize point into 2 subsets, each subset should also present nice hemisphere
			for (int i = 0; i < 1000; i++)
			{
				// find worst points in each of subsets and swap them
				int m0 = GetWorstPointInSubSet(0, nRaysNum / 2);
				int m1 = GetWorstPointInSubSet(nRaysNum / 2, nRaysNum);
				std::swap(pKernelPoints[m0], pKernelPoints[m1]);
			}

			Cry3DEngineBase::PrintMessage("Writing kernel.txt . . .");

			FILE* f = 0;
			fopen_s(&f, "kernel.txt", "wt");
			if (f)
			{
				//        fprintf(f, "#define nSampleNum %d\n", nRaysNum);
				//        fprintf(f, "static float3 kernel[nSampleNum] = \n");
				fprintf(f, "static float3 kernel_HS_%d[%d] = \n", nRaysNum, nRaysNum);
				fprintf(f, "{\n");
				for (int p = 0; p < nRaysNum; p++)
					fprintf(f, "  float3( %.6f , %.6f, %.6f ),\n", pKernelPoints[p].x, pKernelPoints[p].y, pKernelPoints[p].z);
				fprintf(f, "};\n");
				fclose(f);
			}

			Cry3DEngineBase::PrintMessage("Done");
		}

		static Vec3 vPos = CVoxelSegment::m_voxCam.GetPosition();
		Cry3DEngineBase::DrawSphere(vPos, 4.f);
		for (int i = 0; i < nRaysNum; i++)
			Cry3DEngineBase::DrawSphere(vPos + pKernelPoints[i] * 4.f, .1f, (i < nRaysNum / 2) ? Col_Yellow : Col_Cyan);
	}

	StartupStreamingTimeTest(CVoxelSegment::m_nStreamingTasksInProgress == 0 && CVoxelSegment::m_nUpdatesInProgressBri == 0 && CVoxelSegment::m_nUpdatesInProgressTex == 0 && CVoxelSegment::m_nTasksInProgressALL == 0);

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

CSvoNode* CSvoNode::FindNodeByPosition(const Vec3& vPosWS, int nTreeLevelToFind, int nTreeLevelCur)
{
	if (nTreeLevelToFind == nTreeLevelCur)
		return this;

	Vec3 vNodeCenter = m_nodeBox.GetCenter();

	int nChildId =
	  ((vPosWS.x > vNodeCenter.x) ? 4 : 0) |
	  ((vPosWS.y > vNodeCenter.y) ? 2 : 0) |
	  ((vPosWS.z > vNodeCenter.z) ? 1 : 0);

	if (m_ppChilds && m_ppChilds[nChildId])
		return m_ppChilds[nChildId]->FindNodeByPosition(vPosWS, nTreeLevelToFind, nTreeLevelCur + 1);

	return NULL;
}

void CSvoNode::OnStatLightsChanged(const AABB& objBox)
{
	if (Overlap::AABB_AABB(objBox, m_nodeBox))
	{
		m_pSeg->m_bStatLightsChanged = 1;

		if (m_ppChilds)
		{
			for (int nChildId = 0; nChildId < 8; nChildId++)
			{
				if (m_ppChilds[nChildId])
				{
					m_ppChilds[nChildId]->OnStatLightsChanged(objBox);
				}
			}
		}
	}
}

AABB CSvoNode::GetChildBBox(int nChildId)
{
	int x = (nChildId / 4);
	int y = (nChildId - x * 4) / 2;
	int z = (nChildId - x * 4 - y * 2);
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

bool CSvoNode::CheckReadyForRendering(int nTreeLevel, PodArray<CVoxelSegment*> arrForStreaming[nVoxStreamQueueMaxSize][nVoxStreamQueueMaxSize])
{
	bool bAllReady = true;

	if (m_pSeg)
	{
		CVoxelSegment* pCloud = m_pSeg;

		//		if(CVoxelSegment::voxCamera.IsAABBVisible_E(m_nodeBox))
		if (GetCurrPassMainFrameID() > 1)
			pCloud->m_nLastRendFrameId = max(pCloud->m_nLastRendFrameId, GetCurrPassMainFrameID() - 1);

		CVoxelSegment::m_nCheckReadyCounter++;

		if (pCloud->m_eStreamingStatus == ecss_NotLoaded)
		{
			float fBoxSize = m_nodeBox.GetSize().x;

			int nDistId = min(nVoxStreamQueueMaxSize - 1, (int)(m_nodeBox.GetDistance(CVoxelSegment::m_voxCam.GetPosition()) / fBoxSize));
			int nTLevId = min(nVoxStreamQueueMaxSize - 1, nTreeLevel);
			if (arrForStreaming[nTLevId][nDistId].Count() < Cry3DEngineBase::GetCVars()->e_svoMaxStreamRequests)
				arrForStreaming[nTLevId][nDistId].Add(pCloud);
		}

		if (pCloud->m_eStreamingStatus != ecss_Ready)
		{
			bAllReady = false;
		}

		if (pCloud->m_eStreamingStatus == ecss_Ready)
		{
			if (!pCloud->CheckUpdateBrickRenderData(true))
			{
				if (gSvoEnv->m_arrForBrickUpdate[nTreeLevel].Count() < CVoxelSegment::m_nMaxBrickUpdates)
					gSvoEnv->m_arrForBrickUpdate[nTreeLevel].Add(pCloud);
				bAllReady = false;
			}
		}
	}

	return bAllReady;
}

CSvoNode* CSvoNode::FindNodeByPoolAffset(int nAllocatedAtlasOffset)
{
	if (nAllocatedAtlasOffset == m_pSeg->m_nAllocatedAtlasOffset)
		return this;

	if (m_ppChilds)
	{
		for (int nChildId = 0; nChildId < 8; nChildId++)
		{
			if (m_ppChilds[nChildId])
			{
				if (CSvoNode* pRes = m_ppChilds[nChildId]->FindNodeByPoolAffset(nAllocatedAtlasOffset))
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
			for (int nChildId = 0; nChildId < 8; nChildId++)
			{
				if (m_ppChilds[nChildId])
				{
					m_ppChilds[nChildId]->RegisterMovement(objBox);
				}
			}
		}
	}
}

Vec3i ComputeDataCoord(int nAtlasOffset)
{
	static const Vec3i vAtlasDimInt(32, 32, 32);

	//	nAtlasOffset = clamp(nAtlasOffset, 0, vAtlasDimInt*vAtlasDimInt*vAtlasDimInt);

	Vec3i vOffset3D;
	vOffset3D.z = nAtlasOffset / vAtlasDimInt.x / vAtlasDimInt.y;
	vOffset3D.y = (nAtlasOffset / vAtlasDimInt.x - vOffset3D.z * vAtlasDimInt.y);
	vOffset3D.x = nAtlasOffset - vOffset3D.z * vAtlasDimInt.y * vAtlasDimInt.y - vOffset3D.y * vAtlasDimInt.x;

	return vOffset3D;
}

void CSvoNode::Render(PodArray<struct SPvsItem>* pSortedPVS, uint64 nNodeKey, CRenderObject* pObj, int nTreeLevel, PodArray<SVF_P3F_C4B_T2F>& arrVertsOut, PodArray<CVoxelSegment*> arrForStreaming[nVoxStreamQueueMaxSize][nVoxStreamQueueMaxSize])
{
	//	if(!CVoxelSegment::voxCamera.IsAABBVisible_E(m_nodeBox))
	//	return;

	float fBoxSize = m_nodeBox.GetSize().x;
	Vec3 vCamPos = CVoxelSegment::m_voxCam.GetPosition();

	bool bDrawThisNode = false;

	//	const Plane * pNearPlane = CVoxelSegment::voxCamera.GetFrustumPlane(FR_PLANE_NEAR);
	//float fDistToPlane = max(0,-pNearPlane->DistFromPlane(m_nodeBox.GetCenter()) - m_nodeBox.GetSize().x/2);
	const float fDistToPlane = m_nodeBox.GetCenter().GetDistance(CVoxelSegment::m_voxCam.GetPosition()) / 1.50f * 1.25f;

	const float fBoxSizeRated = fBoxSize * Cry3DEngineBase::GetCVars()->e_svoTI_VoxelizaionLODRatio / 1.50f * 1.25f;

	if (m_ppChilds)
	{
		for (int nChildId = 0; nChildId < 8; nChildId++)
		{
			if (m_ppChilds[nChildId] && m_ppChilds[nChildId]->m_bForceRecreate && !m_ppChilds[nChildId]->IsStreamingInProgress())
			{
				SAFE_DELETE(m_ppChilds[nChildId]);
				gSvoEnv->m_fSvoFreezeTime = gEnv->pTimer->GetAsyncCurTime();
				ZeroStruct(gSvoEnv->m_arrVoxelizeMeshesCounter);
			}
		}
	}

	// auto allocate new childs
	if (Cry3DEngineBase::GetCVars()->e_svoTI_Active && m_pSeg && m_pSeg->m_eStreamingStatus == ecss_Ready && m_pSeg->m_pBlockInfo)
		if (m_pSeg->m_dwChildTrisTest || (m_pSeg->GetBoxSize() > Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize) || (Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Subdivide))
		{
			if (fDistToPlane < fBoxSizeRated && !bDrawThisNode)
			{
				CheckAllocateChilds();
			}
		}

	if ((!m_ppChilds || (fDistToPlane > fBoxSizeRated) || (fBoxSize <= Cry3DEngineBase::GetCVars()->e_svoMinNodeSize))
	    || (!CVoxelSegment::m_voxCam.IsAABBVisible_E(m_nodeBox) && (fDistToPlane * 1.5 > fBoxSizeRated)))
	{
		bDrawThisNode = true;
	}

	nTreeLevel++;

	if (!bDrawThisNode && m_ppChilds)
	{
		if (Cry3DEngineBase::GetCVars()->e_svoDVR)
		{
			// check one child per frame
			int nChildId = GetCurrPassMainFrameID() % 8;

			{
				CSvoNode* pChild = m_ppChilds[nChildId];
				if (pChild && pChild->m_pSeg)
					if (!pChild->CheckReadyForRendering(nTreeLevel, arrForStreaming))
						//					if(CVoxelSegment::voxCamera.IsAABBVisible_E(pChild->m_nodeBox))
						bDrawThisNode = true;
			}
		}
		else
		{
			for (int nChildId = 0; nChildId < 8; nChildId++)
			{
				CSvoNode* pChild = m_ppChilds[nChildId];
				if (pChild && pChild->m_pSeg)
					if (!pChild->CheckReadyForRendering(nTreeLevel, arrForStreaming))
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
					m_pSeg->RenderMesh(pObj, arrVertsOut);
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

		int nFirst =
		  ((vCamPos.x > vSrcCenter.x) ? 4 : 0) |
		  ((vCamPos.y > vSrcCenter.y) ? 2 : 0) |
		  ((vCamPos.z > vSrcCenter.z) ? 1 : 0);

		byte childId[8];
		childId[0] = nFirst;
		childId[1] = nFirst ^ 1;
		childId[2] = nFirst ^ 2;
		childId[3] = nFirst ^ 4;
		childId[4] = nFirst ^ 3;
		childId[5] = nFirst ^ 5;
		childId[6] = nFirst ^ 6;
		childId[7] = nFirst ^ 7;

		for (int c = 0; c < 8; c++)
		{
			int nChildId = childId[c];
			CSvoNode* pChild = m_ppChilds[nChildId];
			if (pChild)
				pChild->Render(NULL, (nNodeKey << 3) + (nChildId), pObj, nTreeLevel, arrVertsOut, arrForStreaming);
		}
	}
}

void CSvoNode::CollectResetVisNodes(PvsMap& arrNodes, uint64 nNodeKey)
{
	//  if(m_nVisId)
	//  {
	//    if(m_ppChilds)
	//    {
	//      std::pair<byte,byte> childVisMask;
	//      childVisMask.first = childVisMask.second = 0;
	//
	//      for(int nChildId=0; nChildId<8; nChildId++)
	//      {
	//        if(m_ppChilds[nChildId])
	//        {
	//          childVisMask.first |= (1<<nChildId); // mark this info bit as valid
	//          if(m_ppChilds[nChildId]->m_nVisId)
	//            childVisMask.second |= (1<<nChildId);  // store vis flag
	//
	//          m_ppChilds[nChildId]->CollectResetVisNodes(arrNodes, (nNodeKey<<3) + nChildId);
	//        }
	//      }
	//
	//      arrNodes[nNodeKey] = childVisMask;
	//    }
	//
	//    m_nVisId = 0;
	//  }
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

	float fMinZ = 0.5f;

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

		p[i].z += fMinZ;

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

		p[minp2].z = max(p[minp2].z, fMinZ);
		p[minp1].z = max(p[minp1].z, fMinZ);

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

	m_nDebugDrawVoxelsCounter = 0;
	m_nVoxTexFormat = eTF_R8G8B8A8; // eTF_BC3

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
	m_nTexTrisPoolId = 0;
		#endif
	m_nTexRgb0PoolId = 0;
	m_nTexRgb1PoolId = 0;
	m_nTexDynlPoolId = 0;
	m_nTexRgb2PoolId = 0;
	m_nTexRgb3PoolId = 0;
	m_nTexRgb4PoolId = 0;
	m_nTexNormPoolId = 0;
	m_nTexAldiPoolId = 0;
	#endif
	m_nTexOpasPoolId = 0;
	m_nTexNodePoolId = 0;

	m_prevCheckVal = -1000000;
	m_worldBox.Reset();
	m_matDvrTm = Matrix44(Matrix34::CreateIdentity());
	m_fStreamingStartTime = 0;
	m_nNodeCounter = 0;
	m_nDynNodeCounter_DYNL = m_nDynNodeCounter = 0;

	if (GetCVars()->e_svoTI_Active)
		GetCVars()->e_svoVoxelPoolResolution = min((int)256, GetCVars()->e_svoVoxelPoolResolution);

	CVoxelSegment::nVoxTexPoolDimXY = min(GetCVars()->e_svoVoxelPoolResolution, (int)512);
	CVoxelSegment::nVoxTexPoolDimZ = min(GetCVars()->e_svoVoxelPoolResolution, (int)1024);
	CVoxelSegment::nVoxTexPoolDimZ = max(CVoxelSegment::nVoxTexPoolDimZ, (int)256);

	m_worldBox = worldBox;
	m_bReady = false;
	m_pSvoRoot = new CSvoNode(m_worldBox, NULL);
	m_pGlobalSpecCM = (ITexture*)0;
	m_fGlobalSpecCM_Mult = 1;
	m_fSvoFreezeTime = -1;
	ZeroStruct(gSvoEnv->m_arrVoxelizeMeshesCounter);
	GetRenderer()->GetISvoRenderer(); // allocate SVO sub-system in renderer
	m_bFirst_SvoFreezeTime = m_bFirst_StartStreaming = true;
	m_bStreamingDonePrev = false;
	ZeroStruct(m_arrOccluderBoneNames);
}

CSvoEnv::~CSvoEnv()
{
	// wait until all streaming requests are complete
	while (CVoxelSegment::m_nStreamingTasksInProgress)
	{
		CrySleep(20);
		CVoxelSegment::UpdateStreamingEngine();
	}

	SAFE_DELETE(m_pSvoRoot);

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
	GetRenderer()->RemoveTexture(m_nTexTrisPoolId);
		#endif
	GetRenderer()->RemoveTexture(m_nTexRgb0PoolId);
	GetRenderer()->RemoveTexture(m_nTexRgb1PoolId);
	GetRenderer()->RemoveTexture(m_nTexDynlPoolId);
	GetRenderer()->RemoveTexture(m_nTexRgb2PoolId);
	GetRenderer()->RemoveTexture(m_nTexRgb3PoolId);
	GetRenderer()->RemoveTexture(m_nTexRgb4PoolId);
	GetRenderer()->RemoveTexture(m_nTexNormPoolId);
	GetRenderer()->RemoveTexture(m_nTexAldiPoolId);
	#endif
	GetRenderer()->RemoveTexture(m_nTexOpasPoolId);
	GetRenderer()->RemoveTexture(m_nTexNodePoolId);

	GetCVars()->e_svoTI_Active = 0;
	GetCVars()->e_svoTI_Apply = 0;
	GetCVars()->e_svoLoadTree = 0;
	GetCVars()->e_svoEnabled = 0;
	gSvoEnv = NULL;
	GetRenderer()->GetISvoRenderer()->Release();

	{
		AUTO_MODIFYLOCK(CVoxelSegment::m_arrLockedTextures.m_Lock);
		for (int i = 0; i < CVoxelSegment::m_arrLockedTextures.Count(); i++)
			CVoxelSegment::m_arrLockedTextures[i]->Release();
		CVoxelSegment::m_arrLockedTextures.Reset();
	}

	{
		AUTO_MODIFYLOCK(CVoxelSegment::m_arrLockedMaterials.m_Lock);
		for (int i = 0; i < CVoxelSegment::m_arrLockedMaterials.Count(); i++)
			CVoxelSegment::m_arrLockedMaterials[i]->Release();
		CVoxelSegment::m_arrLockedMaterials.Reset();
	}
}

void CSvoNode::CheckAllocateSegment(int nLod)
{
	if (!m_pSeg)
	{
		int nCloudId = CVoxelSegment::m_nNextCloudId;
		CVoxelSegment::m_nNextCloudId++;

		AllocateSegment(nCloudId, -1, nLod, ecss_Ready, false);
	}
}

CSvoNode::CSvoNode(const AABB& box, CSvoNode* pParent)
{
	ZeroStruct(*this);

	m_pParent = pParent;
	m_nodeBox = box;

	gSvoEnv->m_nNodeCounter++;
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

	gSvoEnv->m_nNodeCounter--;
}

void CSvoNode::Load(int*& pData, int& nNodesCreated, CSvoNode* pParent, AABB* pAreaBox)
{
	m_pParent = pParent;

	int nChildsMask = *pData;
	pData++;

	if (nChildsMask)
	{
		if (!m_ppChilds)
		{
			m_ppChilds = new CSvoNode*[8];
			memset(m_ppChilds, 0, sizeof(m_ppChilds[0]) * 8);
		}
	}

	for (int nChildId = 0; nChildId < 8; nChildId++)
	{
		if (nChildsMask & (1 << nChildId))
		{
			if (!m_ppChilds[nChildId])
				m_ppChilds[nChildId] = new CSvoNode(GetChildBBox(nChildId), this);
			nNodesCreated++;
		}
		else if (m_ppChilds && m_ppChilds[nChildId])
		{
			SAFE_DELETE(m_ppChilds[nChildId]);
		}
	}

	int nCloudsNum = *pData;
	pData++;

	if (nCloudsNum)
	{
		int nCloudId = *pData;
		pData++;
		if (!m_pSeg)
			AllocateSegment(nCloudId, 0, ecss_NotLoaded, ecss_NotLoaded, true);
		m_pSeg->m_nFileStreamOffset64 = *(int64*)pData;
		pData++;
		pData++;
		m_pSeg->m_nFileStreamSize = *pData;
		pData++;
	}
	else
	{
		SAFE_DELETE(m_pSeg);
	}

	if (pAreaBox && !Overlap::AABB_AABB(*pAreaBox, m_nodeBox))
		return;

	for (int nChildId = 0; nChildId < 8; nChildId++)
	{
		if (m_ppChilds && m_ppChilds[nChildId])
			m_ppChilds[nChildId]->Load(pData, nNodesCreated, this, pAreaBox);
	}
}

CVoxelSegment* CSvoNode::AllocateSegment(int nCloudId, int nStationId, int nLod, EFileStreamingStatus eStreamingStatus, bool bDroppedOnDisk)
{
	CVoxelSegment* pCloud = new CVoxelSegment(this, true, eStreamingStatus, bDroppedOnDisk);

	AABB cloudBox = m_nodeBox;
	Vec3 vCenter = m_nodeBox.GetCenter();
	cloudBox.min -= vCenter;
	cloudBox.max -= vCenter;
	pCloud->SetBoxOS(cloudBox);
	//	pCloud->SetRenderBBox(m_nodeBox);

	pCloud->SetID(nCloudId);
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

	float fNodeSize = m_nodeBox.GetSize().x;

	if (fNodeSize > Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize)
		return vCheckSumm;

	PodArray<IRenderNode*> lstObjects;
	AABB boxEx = m_nodeBox;
	float fBorder = fNodeSize / nVoxTexMaxDim;
	boxEx.Expand(Vec3(fBorder, fBorder, fBorder));
	Cry3DEngineBase::Get3DEngine()->GetObjectsByTypeGlobal(lstObjects, eERType_Brush, &boxEx);
	Cry3DEngineBase::Get3DEngine()->GetVisAreaManager()->GetObjectsByType(lstObjects, eERType_Brush, &boxEx);

	float fPrecisioin = 1000;

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
			vCheckSumm += Vec3i(box.min * fPrecisioin);
			vCheckSumm += Vec3i(box.max * fPrecisioin * 2);
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

		vCheckSumm += Vec3i(pVisArea->GetAABBox()->min * fPrecisioin);
		vCheckSumm += Vec3i(pVisArea->GetAABBox()->max * fPrecisioin * 2);
	}

	return vCheckSumm;
}

void CSvoNode::UpdateNodeRenderDataPtrs()
{
	if (m_pSeg && m_ppChilds && m_pSeg->m_pBlockInfo && m_pSeg->m_nChildOffsetsDirty)
	{
		ZeroStruct(m_pSeg->m_arrChildOffset);

		for (int nChildId = 0; nChildId < 8; nChildId++)
		{
			CSvoNode* pChildNode = m_ppChilds[nChildId];

			if (pChildNode && pChildNode->m_pSeg)
			{
				pChildNode->UpdateNodeRenderDataPtrs();

				if (pChildNode->m_pSeg->CheckUpdateBrickRenderData(true))
				{
					m_pSeg->m_arrChildOffset[nChildId] = pChildNode->m_pSeg->m_nAllocatedAtlasOffset;

					int nAllChildsNum = 0;
					int nReadyChildsNum = 0;

					if (pChildNode->m_ppChilds)
					{
						for (int nSubChildId = 0; nSubChildId < 8; nSubChildId++)
						{
							if (CSvoNode* pSubChildNode = pChildNode->m_ppChilds[nSubChildId])
							{
								if (pSubChildNode->m_pSeg)
								{
									nAllChildsNum++;
									if (pSubChildNode->m_pSeg->CheckUpdateBrickRenderData(true))
										nReadyChildsNum++;
								}
							}
						}
					}

					if (nAllChildsNum != nReadyChildsNum || !nAllChildsNum)
						m_pSeg->m_arrChildOffset[nChildId] = -m_pSeg->m_arrChildOffset[nChildId];
				}
			}
			/*	else if(m_arrChildNotNeeded[nChildId])
			   {
			    m_pCloud->m_arrChildOffset[nChildId] = 0;//-3000;
			   }*/
		}

		if (m_pSeg->m_nChildOffsetsDirty == 2)
			m_pSeg->UpdateNodeRenderData();

		m_pSeg->m_nChildOffsetsDirty = 0;
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
		for (int nChildId = 0; nChildId < 8; nChildId++)
			if (m_ppChilds[nChildId] && m_ppChilds[nChildId]->IsStreamingInProgress())
				return true;

	return false;
}

void CSvoNode::GetTrisInAreaStats(int& nTrisCount, int& nVertCount, int& nTrisBytes, int& nVertBytes, int& nMaxVertPerArea, int& nMatsCount)
{

	if (m_pSeg && m_pSeg->m_pTrisInArea)
	{
		if (m_pSeg && m_pSeg->m_pTrisInArea)
		{
			AUTO_READLOCK(m_pSeg->m_superMeshLock);

			nTrisBytes += m_pSeg->m_pTrisInArea->ComputeSizeInMemory();
			nVertBytes += m_pSeg->m_pVertInArea->ComputeSizeInMemory();

			nTrisCount += m_pSeg->m_pTrisInArea->Count();
			nVertCount += m_pSeg->m_pVertInArea->Count();

			nMaxVertPerArea = max(nMaxVertPerArea, m_pSeg->m_pVertInArea->Count());

			nMatsCount += m_pSeg->m_pMatsInArea->Count();
		}
	}
	else if (m_ppChilds)
	{
		for (int nChildId = 0; nChildId < 8; nChildId++)
			if (m_ppChilds[nChildId])
				m_ppChilds[nChildId]->GetTrisInAreaStats(nTrisCount, nVertCount, nTrisBytes, nVertBytes, nMaxVertPerArea, nMatsCount);
	}
}

void CSvoNode::GetVoxSegMemUsage(int& nAllocated)
{
	if (m_pSeg)
	{
		nAllocated += m_pSeg->m_nodeTrisAllMerged.capacity() * sizeof(int);
		nAllocated += sizeof(*m_pSeg);
	}

	if (m_ppChilds)
	{
		for (int nChildId = 0; nChildId < 8; nChildId++)
			if (m_ppChilds[nChildId])
				m_ppChilds[nChildId]->GetVoxSegMemUsage(nAllocated);
	}
}

void CSvoNode::DeleteChilds()
{
	if (m_ppChilds)
	{
		for (int nChildId = 0; nChildId < 8; nChildId++)
			SAFE_DELETE(m_ppChilds[nChildId]);
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
				arrIds.Add((*m_pPoints)[s].nId);
			}
		}
	}

	if (m_ppChilds)
	{
		for (int nChildId = 0; nChildId < 8; nChildId++)
		{
			if (m_ppChilds[nChildId])
			{
				AABB childBox = GetChildBBox(nChildId, nodeBox);
				if (Overlap::AABB_AABB(testBox, childBox))
					if (m_ppChilds[nChildId]->GetAllPointsInTheBox(testBox, childBox, arrIds))
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
		for (int nChildId = 0; nChildId < 8; nChildId++)
		{
			if (m_ppChilds[nChildId])
			{
				AABB childBox = GetChildBBox(nChildId, nodeBox);
				if (Overlap::AABB_AABB(testBox, childBox))
					if (m_ppChilds[nChildId]->IsThereAnyPointInTheBox(testBox, childBox))
						return true;
			}
		}
	}

	return false;
}

bool CPointTreeNode::TryInsertPoint(int nId, const Vec3& vPos, const AABB& nodeBox, int nRecursionLevel)
{
	if (nRecursionLevel >= 7)
	{
		if (!m_pPoints)
			m_pPoints = new PodArray<SPointInfo>;
		SPointInfo info;
		info.vPos = vPos;
		info.nId = nId;
		m_pPoints->Add(info);
		return true;
	}

	Vec3 vNodeCenter = nodeBox.GetCenter();

	int nChildId =
	  ((vPos.x >= vNodeCenter.x) ? 4 : 0) |
	  ((vPos.y >= vNodeCenter.y) ? 2 : 0) |
	  ((vPos.z >= vNodeCenter.z) ? 1 : 0);

	if (!m_ppChilds)
	{
		m_ppChilds = new CPointTreeNode*[8];
		memset(m_ppChilds, 0, sizeof(m_ppChilds[0]) * 8);
	}

	AABB childBox = GetChildBBox(nChildId, nodeBox);

	if (!m_ppChilds[nChildId])
		m_ppChilds[nChildId] = new CPointTreeNode();

	return m_ppChilds[nChildId]->TryInsertPoint(nId, vPos, childBox, nRecursionLevel + 1);
}

void CPointTreeNode::Clear()
{
	if (m_ppChilds)
		for (int nChildId = 0; nChildId < 8; nChildId++)
			SAFE_DELETE(m_ppChilds[nChildId]);
	SAFE_DELETE_ARRAY(m_ppChilds);
	SAFE_DELETE_ARRAY(m_pPoints);
}

AABB CPointTreeNode::GetChildBBox(int nChildId, const AABB& nodeBox)
{
	int x = (nChildId / 4);
	int y = (nChildId - x * 4) / 2;
	int z = (nChildId - x * 4 - y * 2);
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

	svoInfo.pTexTree = GetRenderer()->EF_GetTextureByID(m_nTexNodePoolId);
	svoInfo.pTexOpac = GetRenderer()->EF_GetTextureByID(m_nTexOpasPoolId);

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
	svoInfo.pTexTris = GetRenderer()->EF_GetTextureByID(m_nTexTrisPoolId);
		#endif
	svoInfo.pTexRgb0 = GetRenderer()->EF_GetTextureByID(m_nTexRgb0PoolId);
	svoInfo.pTexRgb1 = GetRenderer()->EF_GetTextureByID(m_nTexRgb1PoolId);
	svoInfo.pTexDynl = GetRenderer()->EF_GetTextureByID(m_nTexDynlPoolId);
	svoInfo.pTexRgb2 = GetRenderer()->EF_GetTextureByID(m_nTexRgb2PoolId);
	svoInfo.pTexRgb3 = GetRenderer()->EF_GetTextureByID(m_nTexRgb3PoolId);
	svoInfo.pTexRgb4 = GetRenderer()->EF_GetTextureByID(m_nTexRgb4PoolId);
	svoInfo.pTexNorm = GetRenderer()->EF_GetTextureByID(m_nTexNormPoolId);
	svoInfo.pTexAldi = GetRenderer()->EF_GetTextureByID(m_nTexAldiPoolId);
	svoInfo.pTexTriA = GetRenderer()->EF_GetTextureByID(gSvoEnv->m_arrRTPoolTris.m_nTexId);
	svoInfo.pTexTexA = GetRenderer()->EF_GetTextureByID(gSvoEnv->m_arrRTPoolTexs.m_nTexId);
	svoInfo.pTexIndA = GetRenderer()->EF_GetTextureByID(gSvoEnv->m_arrRTPoolInds.m_nTexId);
	svoInfo.pGlobalSpecCM = m_pGlobalSpecCM;
	#endif

	svoInfo.fGlobalSpecCM_Mult = m_fGlobalSpecCM_Mult;

	svoInfo.nTexDimXY = CVoxelSegment::nVoxTexPoolDimXY;
	svoInfo.nTexDimZ = CVoxelSegment::nVoxTexPoolDimZ;
	svoInfo.nBrickSize = nVoxTexMaxDim;

	svoInfo.bSvoReady = (m_fStreamingStartTime < 0);
	svoInfo.bSvoFreeze = (m_fSvoFreezeTime >= 0);

	ZeroStruct(svoInfo.arrPortalsPos);
	ZeroStruct(svoInfo.arrPortalsDir);
	if (GetCVars()->e_svoTI_PortalsDeform || GetCVars()->e_svoTI_PortalsInject)
		if (IVisArea* pCurVisArea = GetVisAreaManager()->GetCurVisArea())
		{
			PodArray<IVisArea*> visitedAreas;
			pCurVisArea->FindSurroundingVisArea(2, true, &visitedAreas, 32);

			int nPortalCounter = 0;
			for (int nAreaID = 0; nAreaID < visitedAreas.Count(); nAreaID++)
			{
				IVisArea* pPortal = visitedAreas[nAreaID];

				if (!pPortal || nPortalCounter >= SVO_MAX_PORTALS)
					break;

				if (!pPortal->IsPortal())
					continue;

				if (!pPortal->IsConnectedToOutdoor())
					continue;

				AABB boxEx = *pPortal->GetAABBox();
				boxEx.Expand(Vec3(GetCVars()->e_svoTI_ConeMaxLength, GetCVars()->e_svoTI_ConeMaxLength, GetCVars()->e_svoTI_ConeMaxLength));
				if (!gEnv->pSystem->GetViewCamera().IsAABBVisible_E(boxEx))
					continue;

				svoInfo.arrPortalsPos[nPortalCounter] = Vec4(pPortal->GetAABBox()->GetCenter(), pPortal->GetAABBox()->GetRadius());
				svoInfo.arrPortalsDir[nPortalCounter] = Vec4(((CVisArea*)pCurVisArea)->GetConnectionNormal((CVisArea*)pPortal), 0);

				nPortalCounter++;
			}
		}

	ZeroStruct(svoInfo.arrAnalyticalOccluders);
	for (int i = 0; (i < SVO_MAX_ANALYTICAL_OCCLUDERS) && (i < m_AnalyticalOccluders.Count()); i++)
	{
		svoInfo.arrAnalyticalOccluders[i] = m_AnalyticalOccluders[i];
	}

	if (GetCVars()->e_svoTI_UseTODSkyColor)
	{
		ITimeOfDay::SVariableInfo varCol, varMul;
		Get3DEngine()->GetTimeOfDay()->GetVariableInfo(ITimeOfDay::PARAM_FOG_COLOR2, varCol);
		Get3DEngine()->GetTimeOfDay()->GetVariableInfo(ITimeOfDay::PARAM_FOG_COLOR2_MULTIPLIER, varMul);
		svoInfo.vSkyColorTop = Vec3(varCol.fValue[0] * varMul.fValue[0], varCol.fValue[1] * varMul.fValue[0], varCol.fValue[2] * varMul.fValue[0]);
		Get3DEngine()->GetTimeOfDay()->GetVariableInfo(ITimeOfDay::PARAM_FOG_COLOR, varCol);
		Get3DEngine()->GetTimeOfDay()->GetVariableInfo(ITimeOfDay::PARAM_FOG_COLOR_MULTIPLIER, varMul);
		svoInfo.vSkyColorBottom = Vec3(varCol.fValue[0] * varMul.fValue[0], varCol.fValue[1] * varMul.fValue[0], varCol.fValue[2] * varMul.fValue[0]);

		float fAver = (svoInfo.vSkyColorTop.x + svoInfo.vSkyColorTop.y + svoInfo.vSkyColorTop.z) / 3;
		svoInfo.vSkyColorTop.SetLerp(Vec3(fAver, fAver, fAver), svoInfo.vSkyColorTop, GetCVars()->e_svoTI_UseTODSkyColor);
		fAver = (svoInfo.vSkyColorBottom.x + svoInfo.vSkyColorBottom.y + svoInfo.vSkyColorBottom.z) / 3;
		svoInfo.vSkyColorBottom.SetLerp(Vec3(fAver, fAver, fAver), svoInfo.vSkyColorBottom, GetCVars()->e_svoTI_UseTODSkyColor);
	}
	else
	{
		svoInfo.vSkyColorBottom = svoInfo.vSkyColorTop = Vec3(1.f, 1.f, 1.f);
	}

	*pLightsTI_S = m_lightsTI_S;
	*pLightsTI_D = m_lightsTI_D;

	return /*!GetCVars()->e_svoQuad && */ svoInfo.pTexTree != 0;
}

void CSvoEnv::GetSvoBricksForUpdate(PodArray<I3DEngine::SSvoNodeInfo>& arrNodeInfo, float fNodeSize, PodArray<SVF_P3F_C4B_T2F>* pVertsOut)
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

	//  uint nOldestVisFrameId[4] = { ~0, ~0, ~0, ~0 };
	//  const uint nMaxAllowedFrameId = GetCurrPassMainFrameID()-16;
	const uint nNumBlocks = CVoxelSegment::m_pBlockPacker->GetNumBlocks();

	bool bSyncAll = 1;

	if (fNodeSize == 0)
	{
		gSvoEnv->m_nDynNodeCounter = 0;

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
		fCheckVal += (float)(m_fStreamingStartTime < 0);
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
		fCheckVal += GetCVars()->e_svoTI_HalfresKernel;
		fCheckVal += GetCVars()->e_svoTI_Diffuse_Cache;
		fCheckVal += GetCVars()->e_svoTI_PortalsInject * 10;
		fCheckVal += GetCVars()->e_svoTI_SunRSMInject;
		fCheckVal += GetCVars()->e_svoTI_EmissiveMultiplier;
		fCheckVal += GetCVars()->e_svoTI_PointLightsMultiplier;

		//		fCheckVal += int(Get3DEngine()->GetSunDir().x*10);
		//	fCheckVal += int(Get3DEngine()->GetSunDir().y*10);
		//fCheckVal += int(Get3DEngine()->GetSunDir().z*10);

		if (m_prevCheckVal == -1000000)
			m_prevCheckVal = fCheckVal;

		bSyncAll = (m_prevCheckVal != (fCheckVal));

		{
			static int nPrevFlagAsync = GetCVars()->e_svoTI_UpdateLighting;
			if (GetCVars()->e_svoTI_UpdateLighting && !nPrevFlagAsync)
				bSyncAll = true;
			nPrevFlagAsync = GetCVars()->e_svoTI_UpdateLighting;
		}

		{
			static int nPrevFlagAsync = (gSvoEnv->m_fSvoFreezeTime <= 0);
			if ((gSvoEnv->m_fSvoFreezeTime <= 0) && !nPrevFlagAsync)
			{
				bSyncAll = true;
				gSvoEnv->m_fSvoFreezeTime = -1;
			}
			nPrevFlagAsync = (gSvoEnv->m_fSvoFreezeTime <= 0);
		}

		m_prevCheckVal = fCheckVal;

		if (bSyncAll)
			PrintMessage("SVO radiance full update");

		bool bEditing = gEnv->IsEditing();

		if (GetCVars()->e_svoTI_UpdateLighting && GetCVars()->e_svoTI_IntegrationMode)
		{
			int nAutoUpdateVoxNum = 0;

			int nMaxVox = GetCVars()->e_svoTI_Reflect_Vox_Max * (int)30 / (int)max(30.f, gEnv->pTimer->GetFrameRate());
			int nMaxVoxEdit = GetCVars()->e_svoTI_Reflect_Vox_MaxEdit * (int)30 / (int)max(30.f, gEnv->pTimer->GetFrameRate());

			for (uint n = 0; n < nNumBlocks; n++)
			{
				static uint nAutoBlockId = 0;
				if (nAutoBlockId >= nNumBlocks)
					nAutoBlockId = 0;

				if (SBlockMinMax* pBlock = CVoxelSegment::m_pBlockPacker->GetBlockInfo(nAutoBlockId))
				{
					CVoxelSegment* pSeg = (CVoxelSegment*)pBlock->m_pUserData;

					if ((pSeg->GetBoxSize() >= fNodeSize && pSeg->GetBoxSize() <= GetCVars()->e_svoMaxNodeSize) || !fNodeSize)
						if (pSeg->m_pNode->m_nRequestSegmentUpdateFrametId < GetCurrPassMainFrameID() - 30)
						{
							if (nAutoUpdateVoxNum && (nAutoUpdateVoxNum + (int(sqrt((float)pSeg->m_nVoxNum)) + GetCVars()->e_svoTI_Reflect_Vox_Max_Overhead) > nMaxVox))
							{
								if (bEditing)
								{
									if (pSeg->m_bStatLightsChanged)
										if ((pSeg->GetBoxSize() >= fNodeSize && pSeg->GetBoxSize() <= GetCVars()->e_svoMaxNodeSize) || !fNodeSize)
										//if(pSeg->m_pNode->m_nRequestBrickUpdateFrametId < GetCurrPassMainFrameID()-30)
										{
											if (nAutoUpdateVoxNum && (nAutoUpdateVoxNum + (int(sqrt((float)pSeg->m_nVoxNum)) + GetCVars()->e_svoTI_Reflect_Vox_Max_Overhead) > nMaxVoxEdit))
											{
											}
											else
											{
												pSeg->m_pNode->m_nRequestSegmentUpdateFrametId = max(pSeg->m_pNode->m_nRequestSegmentUpdateFrametId, GetCurrPassMainFrameID());
												nAutoUpdateVoxNum += (int(sqrt((float)pSeg->m_nVoxNum)) + GetCVars()->e_svoTI_Reflect_Vox_Max_Overhead);
											}
										}
								}
								else
									break;
							}
							else
							{
								pSeg->m_pNode->m_nRequestSegmentUpdateFrametId = max(pSeg->m_pNode->m_nRequestSegmentUpdateFrametId, GetCurrPassMainFrameID());
								nAutoUpdateVoxNum += (int(sqrt((float)pSeg->m_nVoxNum)) + GetCVars()->e_svoTI_Reflect_Vox_Max_Overhead);
							}
						}
				}

				nAutoBlockId++;
			}
		}
	}

	static uint nBlockId = 0;

	uint nMaxUpdateBlocks = (bSyncAll || (GetCVars()->e_svoDVR != 10)) ? nNumBlocks : (nNumBlocks / 16 + 1);

	m_nDynNodeCounter_DYNL = 0;

	for (uint nBl = 0; nBl < nMaxUpdateBlocks; nBl++)
	{
		nBlockId++;
		if (nBlockId >= nNumBlocks)
			nBlockId = 0;

		if (SBlockMinMax* pBlock = CVoxelSegment::m_pBlockPacker->GetBlockInfo(nBlockId))
		{
			CVoxelSegment* pSeg = (CVoxelSegment*)pBlock->m_pUserData;

			//			if( ((GetMainFrameID()%nNumBlocks)&63) == (nBlockId&63) && pSeg->m_pNode->m_nRequestBrickUpdateFrametId>0 && GetCVars()->e_svoDVR )
			//			pSeg->m_pNode->m_nRequestBrickUpdateFrametId = GetMainFrameID();

			if (pSeg->m_pNode->m_nRequestSegmentUpdateFrametId >= GetCurrPassMainFrameID() || bSyncAll || pSeg->m_bStatLightsChanged)
			{
				if (fNodeSize == 0)
					gSvoEnv->m_nDynNodeCounter++;

				if ((pSeg->GetBoxSize() >= fNodeSize && pSeg->GetBoxSize() <= GetCVars()->e_svoMaxNodeSize) || !fNodeSize)
				//					if( pSeg->GetBoxSize() <= GetCVars()->e_svoMaxNodeSize )
				{
					I3DEngine::SSvoNodeInfo nodeInfo;

					nodeInfo.wsBox.min = pSeg->m_boxOS.min + pSeg->m_vSegOrigin;
					nodeInfo.wsBox.max = pSeg->m_boxOS.max + pSeg->m_vSegOrigin;

					nodeInfo.tcBox.min.Set((float)pBlock->m_dwMinX / (float)nAtlasDimMaxXY, (float)pBlock->m_dwMinY / (float)nAtlasDimMaxXY, (float)pBlock->m_dwMinZ / (float)nAtlasDimMaxZ);
					nodeInfo.tcBox.max.Set((float)pBlock->m_dwMaxX / (float)nAtlasDimMaxXY, (float)pBlock->m_dwMaxY / (float)nAtlasDimMaxXY, (float)pBlock->m_dwMaxZ / (float)nAtlasDimMaxZ);

					nodeInfo.nAtlasOffset = pSeg->m_nAllocatedAtlasOffset;

					if (fNodeSize)
					{
						if (nodeInfo.wsBox.GetDistance(Get3DEngine()->GetRenderingCamera().GetPosition()) > 24.f)//nodeInfo.wsBox.GetSize().z)
							continue;

						//						if(!Overlap::AABB_AABB(nodeInfo.wsBox, m_aabbLightsTI_D))
						//						continue;

						AABB wsBoxEx = nodeInfo.wsBox;
						wsBoxEx.Expand(Vec3(2, 2, 2));
						if (!gEnv->pSystem->GetViewCamera().IsAABBVisible_F(wsBoxEx))
							continue;

						m_nDynNodeCounter_DYNL++;
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

	const uint nNumBlocks = CVoxelSegment::m_pBlockPacker->GetNumBlocks();
	for (uint nBlockId = 0; nBlockId < nNumBlocks; nBlockId++)
	{
		if (SBlockMinMax* pBlock = CVoxelSegment::m_pBlockPacker->GetBlockInfo(nBlockId))
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
	const int nDataSizeStatsScale = 4;
	CVoxelSegment::m_nPoolUsageItems = 0;
	CVoxelSegment::m_nPoolUsageBytes = 0;

	const uint nNumBlocks = CVoxelSegment::m_pBlockPacker->GetNumBlocks();
	for (uint nBlockId = 0; nBlockId < nNumBlocks; nBlockId++)
	{
		if (SBlockMinMax* pBlock = CVoxelSegment::m_pBlockPacker->GetBlockInfo(nBlockId))
		{
			CVoxelSegment::m_nPoolUsageItems++;
			CVoxelSegment::m_nPoolUsageBytes += nDataSizeStatsScale * pBlock->m_nDataSize;

			CVoxelSegment* pSeg = (CVoxelSegment*)pBlock->m_pUserData;

			float fNodeSize = pSeg->GetBoxSize();

			PodArray<IRenderNode*> lstObjects;
			AABB boxEx = pSeg->m_pNode->m_nodeBox;
			float fBorder = fNodeSize / 4.f + GetCVars()->e_svoTI_ConeMaxLength * max(0, GetCVars()->e_svoTI_NumberOfBounces - 1);
			boxEx.Expand(Vec3(fBorder, fBorder, fBorder));
			Cry3DEngineBase::Get3DEngine()->GetObjectsByTypeGlobal(lstObjects, eERType_Light, &boxEx);
			Cry3DEngineBase::Get3DEngine()->GetVisAreaManager()->GetObjectsByType(lstObjects, eERType_Light, &boxEx);

			float fPrecisioin = 1000.f;
			Vec3i vCheckSumm(0, 0, 0);

			for (int i = 0; i < lstObjects.Count(); i++)
			{
				IRenderNode* pNode = lstObjects[i];

				if (pNode->GetVoxMode() == IRenderNode::VM_Static)
				{
					if (pNode->GetRenderNodeType() == eERType_Light)
					{
						ILightSource* pLS = (ILightSource*)pNode;

						CDLight& m_light = pLS->GetLightProperties();

						if (!Overlap::Sphere_AABB(Sphere(m_light.m_BaseOrigin, m_light.m_fBaseRadius), pSeg->m_pNode->m_nodeBox))
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
					vCheckSumm += Vec3i(box.min * fPrecisioin);
					vCheckSumm += Vec3i(box.max * fPrecisioin * 2);
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
	if (m_fSvoFreezeTime > 0 && bDone && m_bStreamingDonePrev)
	{
		PrintMessage("SVO update finished in %.1f sec (%d / %d nodes, %d K tris)",
		             gEnv->pTimer->GetAsyncCurTime() - m_fSvoFreezeTime, m_arrVoxelizeMeshesCounter[0], m_arrVoxelizeMeshesCounter[1], CVoxelSegment::m_nVoxTrisCounter / 1000);
		m_fSvoFreezeTime = 0;

		if (GetCVars()->e_svoDebug)
		{
			AUTO_MODIFYLOCK(CVoxelSegment::m_cgfTimeStatsLock);
			PrintMessage("Voxelization time spend per CFG:");
			for (auto it = CVoxelSegment::m_cgfTimeStats.begin(); it != CVoxelSegment::m_cgfTimeStats.end(); ++it)
				if (it->second > 1.f)
					PrintMessage("  %4.1f sec %s", it->second, it->first->GetFilePath());
		}
	}

	if (m_fStreamingStartTime == 0)
	{
		m_fStreamingStartTime = Cry3DEngineBase::GetTimer()->GetAsyncCurTime();
	}
	else if (m_fStreamingStartTime > 0 && bDone && m_bStreamingDonePrev)
	{
		//		float fTime = Cry3DEngineBase::GetTimer()->GetAsyncCurTime();
		//		PrintMessage("SVO initialization finished in %.1f sec (%d K tris)",
		//		fTime - m_fStreamingStartTime, CVoxelSegment::m_nVoxTrisCounter/1000);

		m_fStreamingStartTime = -1;

		OnLevelGeometryChanged();

		GetCVars()->e_svoMaxBrickUpdates = 4;
		GetCVars()->e_svoMaxStreamRequests = 4;

		int nGpuMax = nAtlasDimBriXY * nAtlasDimBriXY * nAtlasDimBriZ;
		GetCVars()->e_svoMaxBricksOnCPU = nGpuMax * 3 / 2;

		//		if(m_pTree)
		//		m_pTree->UpdateTerrainNormals();
	}

	m_bStreamingDonePrev = bDone;
}

void CSvoEnv::OnLevelGeometryChanged()
{

}

int CSvoEnv::GetWorstPointInSubSet(const int nStart, const int nEnd)
{
	int p0 = -1;

	float fMinAverDist = 100000000;

	for (int i = nStart; i < nEnd; i++)
	{
		float fAverDist = 0;

		for (int j = 0; j < nRaysNum; j++)
		{
			float fDist = sqrt(sqrt(pKernelPoints[i].GetSquaredDistance2D(pKernelPoints[j])));

			if (j >= nStart && j < nEnd)
				fAverDist += fDist;
			else
				fAverDist -= fDist;
		}

		if (fAverDist < fMinAverDist)
		{
			p0 = i;
			fMinAverDist = fAverDist;
		}
	}

	return p0;
}

void CSvoEnv::CheckUpdateMeshPools()
{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	{
		PodArrayRT<Vec4>& rAr = gSvoEnv->m_arrRTPoolTris;

		AUTO_READLOCK(rAr.m_Lock);

		if (rAr.m_bModified)
		{
			gEnv->pRenderer->RemoveTexture(rAr.m_nTexId);
			rAr.m_nTexId = 0;

			rAr.m_nTexId = gEnv->pRenderer->DownLoadToVideoMemory3D((byte*)rAr.GetElements(),
			                                                        CVoxelSegment::nVoxTexPoolDimXY, CVoxelSegment::nVoxTexPoolDimXY, CVoxelSegment::nVoxTexPoolDimZ, eTF_R32G32B32A32F, eTF_R32G32B32A32F, 1, false, FILTER_POINT, rAr.m_nTexId, 0, FT_DONT_STREAM);

			rAr.m_bModified = 0;
		}
	}

	{
		PodArrayRT<ColorB>& rAr = gSvoEnv->m_arrRTPoolTexs;

		AUTO_READLOCK(rAr.m_Lock);

		if (rAr.m_bModified)
		{
			gEnv->pRenderer->RemoveTexture(rAr.m_nTexId);
			rAr.m_nTexId = 0;

			rAr.m_nTexId = gEnv->pRenderer->DownLoadToVideoMemory3D((byte*)rAr.GetElements(),
			                                                        CVoxelSegment::nVoxTexPoolDimXY, CVoxelSegment::nVoxTexPoolDimXY, CVoxelSegment::nVoxTexPoolDimZ, m_nVoxTexFormat, m_nVoxTexFormat, 1, false, FILTER_POINT, rAr.m_nTexId, 0, FT_DONT_STREAM);

			rAr.m_bModified = 0;
		}
	}

	{
		PodArrayRT<ColorB>& rAr = gSvoEnv->m_arrRTPoolInds;

		AUTO_READLOCK(rAr.m_Lock);

		if (rAr.m_bModified)
		{
			gEnv->pRenderer->RemoveTexture(rAr.m_nTexId);
			rAr.m_nTexId = 0;

			rAr.m_nTexId = gEnv->pRenderer->DownLoadToVideoMemory3D((byte*)rAr.GetElements(),
			                                                        CVoxelSegment::nVoxTexPoolDimXY, CVoxelSegment::nVoxTexPoolDimXY, CVoxelSegment::nVoxTexPoolDimZ, m_nVoxTexFormat, m_nVoxTexFormat, 1, false, FILTER_POINT, rAr.m_nTexId, 0, FT_DONT_STREAM);

			rAr.m_bModified = 0;
		}
	}
	#endif
}

bool C3DEngine::GetSvoStaticTextures(I3DEngine::SSvoStaticTexInfo& svoInfo, PodArray<I3DEngine::SLightTI>* pLightsTI_S, PodArray<I3DEngine::SLightTI>* pLightsTI_D)
{
	return CSvoManager::GetSvoStaticTextures(svoInfo, pLightsTI_S, pLightsTI_D);
}

void C3DEngine::GetSvoBricksForUpdate(PodArray<SSvoNodeInfo>& arrNodeInfo, float fNodeSize, PodArray<SVF_P3F_C4B_T2F>* pVertsOut)
{
	CSvoManager::GetSvoBricksForUpdate(arrNodeInfo, fNodeSize, pVertsOut);
}

void C3DEngine::LoadTISettings(XmlNodeRef pInputNode)
{
	const char* szXmlNodeName = "Total_Illumination_v2";

	// Total illumination
	if (GetCVars()->e_svoTI_Active >= 0)
	{
		GetCVars()->e_svoTI_Apply = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Active", "0"));

		//    if(GetCVars()->e_svoTI_Apply)
		//    {
		//      GetCVars()->e_svoTI_Active = 1;
		//      GetCVars()->e_GI = 0;
		//      GetCVars()->e_Clouds = 0;
		//      if(ICVar * pV = gEnv->pConsole->GetCVar("r_UseAlphaBlend"))
		//        pV->Set(0);
		//    }
	}

	GetCVars()->e_svoVoxelPoolResolution = 64;
	int nVoxelPoolResolution = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "VoxelPoolResolution", "0"));
	while (GetCVars()->e_svoVoxelPoolResolution < nVoxelPoolResolution)
		GetCVars()->e_svoVoxelPoolResolution *= 2;

	GetCVars()->e_svoTI_VoxelizaionLODRatio = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "VoxelizaionLODRatio", "0"));
	GetCVars()->e_svoDVR_DistRatio = GetCVars()->e_svoTI_VoxelizaionLODRatio / 2;

	GetCVars()->e_svoTI_InjectionMultiplier = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "InjectionMultiplier", "0"));
	GetCVars()->e_svoTI_NumberOfBounces = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "NumberOfBounces", "0"));
	GetCVars()->e_svoTI_Saturation = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "Saturation", "0"));
	GetCVars()->e_svoTI_PropagationBooster = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "PropagationBooster", "0"));
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

	GetCVars()->e_svoMinNodeSize = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "MinNodeSize", "0"));

	GetCVars()->e_svoTI_SkipNonGILights = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "SkipNonGILights", "0"));
	GetCVars()->e_svoTI_ForceGIForAllLights = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "ForceGIForAllLights", "0"));
	GetCVars()->e_svoTI_SSAOAmount = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "SSAOAmount", "0"));
	GetCVars()->e_svoTI_PortalsDeform = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "PortalsDeform", "0"));
	GetCVars()->e_svoTI_PortalsInject = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "PortalsInject", "0"));
	GetCVars()->e_svoTI_ObjectsMaxViewDistance = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "ObjectsMaxViewDistance", "0"));
	GetCVars()->e_svoTI_SunRSMInject = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "SunRSMInject", "0"));
	GetCVars()->e_svoTI_SSDepthTrace = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "SSDepthTrace", "0"));

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

	int nLowSpecMode = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "LowSpecMode", "0"));
	if (nLowSpecMode > -2 && gEnv->IsEditor()) // otherwise we use value from sys_spec_Light.cfg
		GetCVars()->e_svoTI_LowSpecMode = nLowSpecMode;

	GetCVars()->e_svoTI_HalfresKernel = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "HalfresKernel", "0"));
	GetCVars()->e_svoTI_UseTODSkyColor = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "UseTODSkyColor", "0"));

	GetCVars()->e_svoTI_HighGlossOcclusion = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "HighGlossOcclusion", "0"));

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	GetCVars()->e_svoTI_IntegrationMode = (int)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "IntegrationMode", "0"));
	#else
	GetCVars()->e_svoTI_IntegrationMode = 0;
	#endif

	GetCVars()->e_svoTI_RT_MaxDist = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "RT_MaxDist", "0"));

	GetCVars()->e_svoTI_ConstantAmbientDebug = (float)atof(GetXMLAttribText(pInputNode, szXmlNodeName, "ConstantAmbientDebug", "0"));

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
	float fSize = (float)Get3DEngine()->GetTerrainSize();
	for (; fSize > 0.01f && fSize >= GetCVars()->e_svoMinNodeSize * 2.f; fSize /= 2.f)
		;
	GetCVars()->e_svoMinNodeSize = fSize;

	if (gSvoEnv)
		ZeroStruct(gSvoEnv->m_arrOccluderBoneNames);
}

void CVars::RegisterTICVars()
{
	#define CVAR_CPP
	#include "SceneTreeCVars.inl"
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

	//AABB nodeBox = AABB(Vec3(0,0,0), Vec3( (float)gEnv->p3DEngine->GetTerrainSize(), (float)gEnv->p3DEngine->GetTerrainSize(), (float)gEnv->p3DEngine->GetTerrainSize()));
	AABB nodeBox;
	nodeBox.Reset();

	nodeBox.Add(gEnv->pSystem->GetViewCamera().GetPosition());

	nodeBox.Expand(Vec3(256, 256, 256));

	m_lightsTI_S.Clear();
	m_lightsTI_D.Clear();
	m_aabbLightsTI_D.Reset();

	Vec3 vCamPos = CVoxelSegment::m_voxCam.GetPosition();

	if (int nCount = gEnv->p3DEngine->GetObjectsByTypeInBox(eERType_Light, nodeBox, (IRenderNode**)0))
	{
		PodArray<IRenderNode*> arrObjects(nCount, nCount);
		nCount = gEnv->p3DEngine->GetObjectsByTypeInBox(eERType_Light, nodeBox, arrObjects.GetElements());

		for (int nL = 0; nL < nCount; nL++)
		{
			ILightSource* pRN = (ILightSource*)arrObjects[nL];

			static ICVar* e_svoMinNodeSize = gEnv->pConsole->GetCVar("e_svoMinNodeSize");

			CDLight& rLight = pRN->GetLightProperties();

			I3DEngine::SLightTI lightTI;
			ZeroStruct(lightTI);

			IRenderNode::EVoxMode eVoxMode = pRN->GetVoxMode();

			if (eVoxMode)
			{
				lightTI.vPosR = Vec4(rLight.m_Origin, rLight.m_fRadius);

				if ((rLight.m_Flags & DLF_PROJECT) && (rLight.m_fLightFrustumAngle < 90.f) && rLight.m_pLightImage)
					lightTI.vDirF = Vec4(pRN->GetMatrix().GetColumn(0), rLight.m_fLightFrustumAngle * 2);
				else
					lightTI.vDirF = Vec4(0, 0, 0, 0);

				if (eVoxMode == IRenderNode::VM_Dynamic)
					lightTI.vCol = rLight.m_Color.toVec4();
				else
					lightTI.vCol = rLight.m_BaseColor.toVec4();

				lightTI.vCol.w = (rLight.m_Flags & DLF_CASTSHADOW_MAPS) ? 1.f : 0.f;

				if (rLight.m_Flags & DLF_SUN)
					lightTI.fSortVal = -1;
				else
					lightTI.fSortVal = vCamPos.GetDistance(rLight.m_Origin) / max(24.f, rLight.m_fRadius);

				if (eVoxMode == IRenderNode::VM_Dynamic)
				{
					if ((pRN->GetDrawFrame(0) > 10) && (pRN->GetDrawFrame(0) >= (int)GetCurrPassMainFrameID()))
					{
						m_lightsTI_D.Add(lightTI);
						m_aabbLightsTI_D.Add(pRN->GetBBox());
					}
				}
				else
					m_lightsTI_S.Add(lightTI);
			}
		}

		if (m_lightsTI_S.Count() > 1)
			qsort(m_lightsTI_S.GetElements(), m_lightsTI_S.Count(), sizeof(m_lightsTI_S[0]), SLightTI_Compare);

		if (m_lightsTI_D.Count() > 1)
			qsort(m_lightsTI_D.GetElements(), m_lightsTI_D.Count(), sizeof(m_lightsTI_D[0]), SLightTI_Compare);

		if (m_lightsTI_D.Count() > 8)
			m_lightsTI_D.PreAllocate(8);
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

	m_AnalyticalOccluders.Clear();

	if (!GetCVars()->e_svoTI_AnalyticalOccluders)
		return;

	AABB areaBox;
	areaBox.Reset();
	areaBox.Add(gEnv->pSystem->GetViewCamera().GetPosition());
	areaBox.Expand(Vec3(48, 48, 48));

	Vec3 vCamPos = CVoxelSegment::m_voxCam.GetPosition();

	if (int nCount = gEnv->p3DEngine->GetObjectsByTypeInBox(eERType_RenderProxy, areaBox, (IRenderNode**)0))
	{
		PodArray<IRenderNode*> arrObjects(nCount, nCount);
		nCount = gEnv->p3DEngine->GetObjectsByTypeInBox(eERType_RenderProxy, areaBox, arrObjects.GetElements());

		Vec3 camPos = gEnv->pSystem->GetViewCamera().GetPosition();

		for (int nL = 0; nL < nCount; nL++)
		{
			IRenderNode* pRN = arrObjects[nL];

			if (pRN->GetDrawFrame(0) < (int)GetCurrPassMainFrameID())
				continue;

			Matrix34A matParent;

			// make a sphere, box or capsule from geom entity (for now only capsule is supported)
			if (CStatObj* pStatObj = (CStatObj*)pRN->GetEntityStatObj(0, 0, &matParent, true))
				if (strstr(pRN->GetName(), "_TI_AO"))
				{
					bool bSphere = strstr(pStatObj->GetFilePath(), "sphere") != NULL;
					bool bCube = strstr(pStatObj->GetFilePath(), "cube") != NULL;

					if (bCube)
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
						obb.type = (float)I3DEngine::SAnalyticalOccluder::eOBB;

						m_AnalyticalOccluders.Add(obb);
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

						m_AnalyticalOccluders.Add(capsule);
					}
				}

			if (ICharacterInstance* pCharacter = (ICharacterInstance*)pRN->GetEntityCharacter(0, &matParent))
			{
				AABB aabb = pRN->GetBBox();
				float fDist = aabb.GetDistance(camPos);

				// build single capsule from AABB
				if (fDist > 12.f)
				{
					I3DEngine::SAnalyticalOccluder capsule;
					capsule.type = (float)I3DEngine::SAnalyticalOccluder::eCapsule;

					Vec3 vSize = aabb.GetSize();
					Vec3 vCenter = aabb.GetCenter();

					capsule.radius = min(0.3f, (vSize.x + vSize.y) * 0.25f);

					capsule.v0 = Vec3(vCenter.x, vCenter.y, max(vCenter.z + .01f, aabb.max.z - capsule.radius));
					capsule.v1 = Vec3(vCenter.x, vCenter.y, min(vCenter.z - .01f, aabb.min.z + capsule.radius));

					m_AnalyticalOccluders.Add(capsule);

					continue;
				}

				// build capsules from character bones (bone names must be specified)
				if (GetCVars()->e_svoTI_AnalyticalOccluders == 1)
				{
					if (ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose())
					{
						if (!m_arrOccluderBoneNames[0][0])
							InitOccluderBoneNames();

						if (m_arrOccluderBoneNames[0][0])
						{
							Vec3 arrBonePos[MAX_BONES];

							for (int i = 0; i < MAX_BONES; i++)
							{
								int nJointID = pCharacter->GetIDefaultSkeleton().GetJointIDByName(m_arrOccluderBoneNames[i]);
								if (nJointID >= 0)
								{
									Matrix34A tm34 = matParent * Matrix34(pSkeletonPose->GetAbsJointByID(nJointID));
									arrBonePos[i] = tm34.GetTranslation();
								}
								else
								{
									arrBonePos[i].zero();
								}
							}

							I3DEngine::SAnalyticalOccluder capsule;
							capsule.type = (float)I3DEngine::SAnalyticalOccluder::eCapsule;

							// head
							capsule.radius = .08;
							capsule.v0 = arrBonePos[0];
							capsule.v1 = arrBonePos[1];
							m_AnalyticalOccluders.Add(capsule);

							// spine
							capsule.radius = .1;
							capsule.v0 = arrBonePos[1];
							capsule.v1 = arrBonePos[2];
							m_AnalyticalOccluders.Add(capsule);

							// hand R
							capsule.radius = .06;
							capsule.v0 = arrBonePos[1];
							capsule.v1 = arrBonePos[9];
							m_AnalyticalOccluders.Add(capsule);
							capsule.v0 = arrBonePos[9];
							capsule.v1 = arrBonePos[11];
							m_AnalyticalOccluders.Add(capsule);

							// hand L
							capsule.v0 = arrBonePos[1];
							capsule.v1 = arrBonePos[10];
							m_AnalyticalOccluders.Add(capsule);
							capsule.v0 = arrBonePos[10];
							capsule.v1 = arrBonePos[12];
							m_AnalyticalOccluders.Add(capsule);

							// leg R
							capsule.radius = .08;
							capsule.v0 = arrBonePos[2];
							capsule.v1 = arrBonePos[3];
							m_AnalyticalOccluders.Add(capsule);
							capsule.v0 = arrBonePos[3];
							capsule.v1 = arrBonePos[5];
							m_AnalyticalOccluders.Add(capsule);
							capsule.v0 = arrBonePos[5];
							capsule.v1 = arrBonePos[7];
							m_AnalyticalOccluders.Add(capsule);

							// leg R
							capsule.v0 = arrBonePos[2];
							capsule.v1 = arrBonePos[4];
							m_AnalyticalOccluders.Add(capsule);
							capsule.v0 = arrBonePos[4];
							capsule.v1 = arrBonePos[6];
							m_AnalyticalOccluders.Add(capsule);
							capsule.v0 = arrBonePos[6];
							capsule.v1 = arrBonePos[8];
							m_AnalyticalOccluders.Add(capsule);
						}
					}
				}

				// convert bounding boxes of physical proxy primitives into capsules
				if (GetCVars()->e_svoTI_AnalyticalOccluders == 2)
				{
					IPhysicalEntity* pPE = pCharacter->GetPhysEntity();

					for (int i = 0; i < 64; i++)
					{
						pe_params_part pp;
						pp.ipart = i;
						if (!pPE->GetParams(&pp))
							break;

						if (!pp.pPhysGeom && pp.pPhysGeom->pGeom)
							continue;

						int nType = pp.pPhysGeom->pGeom->GetType();

						if (nType == GEOM_CAPSULE || nType == GEOM_BOX || nType == GEOM_SPHERE || nType == GEOM_CYLINDER || nType == GEOM_TRIMESH)
						{
							primitives::box box;
							pp.pPhysGeom->pGeom->GetBBox(&box);

							int longestAxis = 0;
							if ((box.size.x > box.size.y) && (box.size.x > box.size.z))
								longestAxis = 0;
							else if ((box.size.y > box.size.x) && (box.size.y > box.size.z))
								longestAxis = 1;
							else
								longestAxis = 2;

							float longestLen = box.size[longestAxis];

							int shortestAxis = 0;
							if ((box.size.x < box.size.y) && (box.size.x < box.size.z))
								shortestAxis = 0;
							else if ((box.size.y < box.size.x) && (box.size.y < box.size.z))
								shortestAxis = 1;
							else
								shortestAxis = 2;

							float shortestLen = box.size[shortestAxis];

							if (shortestLen + longestLen < .1)
								continue;

							pe_params_articulated_body pab;
							pPE->GetParams(&pab);

							Vec3 center_world = pp.q * box.center + pp.pos + pab.posHostPivot;
							Vec3 axisMain = pp.q * box.Basis.TransformVector(Vec3(longestAxis == 0, longestAxis == 1, longestAxis == 2)) * max(0.01f, longestLen - shortestLen * 0.5f);
							Vec3 axisSide = pp.q * box.Basis.TransformVector(Vec3(shortestAxis == 0, shortestAxis == 1, shortestAxis == 2)) * shortestLen * 0.5f;

							Vec3 v0 = center_world - axisMain;
							Vec3 v1 = center_world + axisMain;

							v0 = matParent.TransformPoint(v0);
							v1 = matParent.TransformPoint(v1);

							I3DEngine::SAnalyticalOccluder capsule;
							capsule.type = (float)I3DEngine::SAnalyticalOccluder::eCapsule;

							capsule.radius = .1f;// axisSide.GetLength();
							capsule.v0 = v0;
							capsule.v1 = v1;

							m_AnalyticalOccluders.Add(capsule);
						}
					}
				}
			}
		}

		// remove invalid objects
		for (int n = 0; n < m_AnalyticalOccluders.Count(); n++)
		{
			if (m_AnalyticalOccluders[n].v0.IsZero() || m_AnalyticalOccluders[n].v1.IsZero())
			{
				m_AnalyticalOccluders.DeleteFastUnsorted(n);
				n--;
				continue;
			}
			
			if (m_AnalyticalOccluders[n].type == (float)I3DEngine::SAnalyticalOccluder::eCapsule)
				m_AnalyticalOccluders[n].c = (m_AnalyticalOccluders[n].v0 + m_AnalyticalOccluders[n].v1) * 0.5f;
		}

		// sort by importance
		if (m_AnalyticalOccluders.Count() > 1)
			qsort(m_AnalyticalOccluders.GetElements(), m_AnalyticalOccluders.Count(), sizeof(m_AnalyticalOccluders[0]), SAnalyticalOccluder_Compare);
	}
}

void CSvoEnv::InitOccluderBoneNames()
{
	const char* szBoneNames = GetCVars()->e_svoTI_AnalyticalOccludersBoneNames->GetString();

	ZeroStruct(m_arrOccluderBoneNames);

	if (sscanf(szBoneNames, "%s %s %s %s %s %s %s %s",
		m_arrOccluderBoneNames[0], m_arrOccluderBoneNames[1], m_arrOccluderBoneNames[2], m_arrOccluderBoneNames[3], m_arrOccluderBoneNames[5], m_arrOccluderBoneNames[7], m_arrOccluderBoneNames[9], m_arrOccluderBoneNames[11]))
	{
		if (sscanf(szBoneNames, "%s %s %s %s %s %s %s %s",
			m_arrOccluderBoneNames[0], m_arrOccluderBoneNames[1], m_arrOccluderBoneNames[2], m_arrOccluderBoneNames[4], m_arrOccluderBoneNames[6], m_arrOccluderBoneNames[8], m_arrOccluderBoneNames[10], m_arrOccluderBoneNames[12]))
		{
			// restore spaces
			for (int i = 0; i < sizeof(m_arrOccluderBoneNames); i++)
			{
				if (((char*)&m_arrOccluderBoneNames[0])[i] == '#')
					((char*)&m_arrOccluderBoneNames[0])[i] = ' ';
			}

			// replace '*'
			for (int i = 0; i < MAX_BONES; i++)
			{
				if (char* p = strstr(m_arrOccluderBoneNames[i], "*"))
				{
					const char * pInsert = (i & 1) ? "Right" : "Left";

					memmove(p + strlen(pInsert), p + 1, strlen(p + 1));

					memcpy(p, pInsert, strlen(pInsert));
				}
			}

			// replace '$'
			for (int i = 0; i < MAX_BONES; i++)
			{
				if (char* p = strstr(m_arrOccluderBoneNames[i], "$"))
				{
					const char * pInsert = (i & 1) ? "R" : "L";

					memmove(p + strlen(pInsert), p + 1, strlen(p + 1));

					memcpy(p, pInsert, strlen(pInsert));
				}
			}
		}
	}
}

void CSvoNode::CheckAllocateChilds()
{
	if (m_nodeBox.GetSize().x <= Cry3DEngineBase::GetCVars()->e_svoMinNodeSize)
		return;

	const int nCurrPassMainFrameID = GetCurrPassMainFrameID();

	m_pSeg->m_nLastRendFrameId = nCurrPassMainFrameID;

	for (int nChildId = 0; nChildId < 8; nChildId++)
	{
		if (m_arrChildNotNeeded[nChildId])
		{
			if (m_ppChilds[nChildId])
				SAFE_DELETE(m_ppChilds[nChildId]);
			continue;
			;
		}

		AABB childBox = GetChildBBox(nChildId);

		if (Cry3DEngineBase::GetCVars()->e_svoTI_VoxelizaionPostpone && (!m_ppChilds || !m_ppChilds[nChildId]) && (childBox.GetSize().z == Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize))
		{
			bool bThisIsAreaParent, bThisIsLowLodNode;
			if (!CVoxelSegment::CheckCollectObjectsForVoxelization(childBox, NULL, bThisIsAreaParent, bThisIsLowLodNode, true))
			{
				if (Cry3DEngineBase::GetCVars()->e_svoDebug == 7)
					Cry3DEngineBase::Get3DEngine()->DrawBBox(childBox, Col_Lime);
				CVoxelSegment::m_nPostponedCounter++;
				continue;
			}
		}

		//					if((m_pCloud->m_dwChildTrisTest & (1<<nChildId)) ||
		//					(m_pCloud->GetBoxSize() > Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize) || (Cry3DEngineBase::GetCVars()->e_rsDebugAI&2) || 1)
		{
			if (!m_ppChilds)
			{
				m_ppChilds = new CSvoNode*[8];
				memset(m_ppChilds, 0, sizeof(m_ppChilds[0]) * 8);
			}

			CSvoNode* pChild = m_ppChilds[nChildId];

			if (pChild)
			{
				if (pChild->m_pSeg)
				{
					pChild->m_pSeg->m_nLastRendFrameId = nCurrPassMainFrameID;
				}

				continue;
			}

			{
				m_ppChilds[nChildId] = new CSvoNode(childBox, this);

				m_ppChilds[nChildId]->AllocateSegment(CVoxelSegment::m_nNextCloudId++, 0, 0, ecss_NotLoaded, true);
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
