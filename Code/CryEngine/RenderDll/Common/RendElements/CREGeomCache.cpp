// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   CREGeomCache.cpp
//  Created:     17/10/2012 by Axel Gneiting
//  Description: Backend part of geometry cache rendering
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if defined(USE_GEOM_CACHES)

#include <CryRenderer/RenderElements/RendElement.h>
#include <Cry3DEngine/CREGeomCache.h>
#include <Cry3DEngine/I3DEngine.h>
#include "../Renderer.h"
#include "../Common/PostProcess/PostEffects.h"
#include "XRenderD3D9/DriverD3D.h"

//#include "XRenderD3D9/DriverD3D.h"

std::vector<CREGeomCache*> CREGeomCache::ms_updateList[2];
CryCriticalSection CREGeomCache::ms_updateListCS[2];

CREGeomCache::CREGeomCache()
{
	m_bUpdateFrame[0] = false;
	m_bUpdateFrame[1] = false;
	m_transformUpdateState[0] = 0;
	m_transformUpdateState[1] = 0;

	mfSetType(eDATA_GeomCache);
	mfUpdateFlags(FCEF_TRANSFORM);
}

CREGeomCache::~CREGeomCache()
{
	CryAutoLock<CryCriticalSection> lock1(ms_updateListCS[0]);
	CryAutoLock<CryCriticalSection> lock2(ms_updateListCS[1]);

	stl::find_and_erase(ms_updateList[0], this);
	stl::find_and_erase(ms_updateList[1], this);
}

void CREGeomCache::InitializeRenderElement(const uint numMeshes, _smart_ptr<IRenderMesh>* pMeshes, uint16 materialId)
{
	m_bUpdateFrame[0] = false;
	m_bUpdateFrame[1] = false;

	m_meshFillData[0].clear();
	m_meshFillData[1].clear();
	m_meshRenderData.clear();

	m_meshFillData[0].reserve(numMeshes);
	m_meshFillData[1].reserve(numMeshes);
	m_meshRenderData.reserve(numMeshes);

	for (uint i = 0; i < numMeshes; ++i)
	{
		SMeshRenderData meshRenderData;
		meshRenderData.m_pRenderMesh = pMeshes[i];
		m_meshRenderData.push_back(meshRenderData);
		m_meshFillData[0].push_back(meshRenderData);
		m_meshFillData[1].push_back(meshRenderData);
	}

	m_materialId = materialId;
}

void CREGeomCache::mfPrepare(bool bCheckOverflow)
{
	FUNCTION_PROFILER_RENDER_FLAT

	CRenderer* const pRenderer = gRenDev;

	if (bCheckOverflow)
	{
		pRenderer->FX_CheckOverflow(0, 0, this);
	}

	pRenderer->m_RP.m_CurVFormat = GetVertexFormat();
	pRenderer->m_RP.m_pRE = this;
	pRenderer->m_RP.m_FirstVertex = 0;
	pRenderer->m_RP.m_FirstIndex = 0;
	pRenderer->m_RP.m_RendNumIndices = 0;
	pRenderer->m_RP.m_RendNumVerts = 0;
}

void CREGeomCache::SetupMotionBlur(CRenderObject* pRenderObject, const SRenderingPassInfo& passInfo)
{
	CMotionBlur::SetupObject(pRenderObject, passInfo);

	if (pRenderObject->m_fDistance < CRenderer::CV_r_MotionBlurMaxViewDist)
	{
		// Motion blur is temporary disabled because of CE-11256
		//pRenderObject->m_ObjFlags |= FOB_HAS_PREVMATRIX | FOB_MOTION_BLUR;
	}
}

bool CREGeomCache::Update(const int flags, const bool bTessellation)
{
	FUNCTION_PROFILER_RENDER_FLAT

	// Wait until render node update has finished
	const int threadId = gRenDev->m_RP.m_nProcessThreadID;
	while (m_transformUpdateState[threadId])
	{
		CrySleep(0);
	}

	// Check if update was successful and if so copy data to render buffer
	if (m_bUpdateFrame[threadId])
	{
		m_meshRenderData = m_meshFillData[threadId];
	}

	const uint numMeshes = m_meshFillData[threadId].size();
	bool bRet = true;

	for (uint nMesh = 0; nMesh < numMeshes; ++nMesh)
	{
		SMeshRenderData& meshData = m_meshFillData[threadId][nMesh];
		CRenderMesh* const pRenderMesh = static_cast<CRenderMesh*>(meshData.m_pRenderMesh.get());

		if (pRenderMesh && pRenderMesh->m_Modified[threadId].linked())
		{
			// Sync the async render mesh update. This waits for the fill thread started from main thread if it's still running.
			// We need to do this manually, because geom caches don't use CREMesh.
			pRenderMesh->SyncAsyncUpdate(threadId);

			CRenderMesh* pVertexContainer = pRenderMesh->_GetVertexContainer();
			bool bSucceed = pRenderMesh->RT_CheckUpdate(pVertexContainer, pRenderMesh->GetVertexFormat(), flags | VSM_MASK, bTessellation);
			if (bSucceed)
			{
				pRenderMesh->m_Modified[threadId].erase();
			}

			if (!bSucceed || !pVertexContainer->_HasVBStream(VSF_GENERAL))
			{
				bRet = false;
			}
		}
	}

	return bRet;
}

void CREGeomCache::UpdateModified()
{
	FUNCTION_PROFILER_RENDER_FLAT

	const int threadId = gRenDev->m_RP.m_nProcessThreadID;
	CryAutoLock<CryCriticalSection> lock(ms_updateListCS[threadId]);

	for (std::vector<CREGeomCache*>::iterator iter = ms_updateList[threadId].begin();
	     iter != ms_updateList[threadId].end(); iter = ms_updateList[threadId].erase(iter))
	{
		CREGeomCache* pRenderElement = *iter;
		pRenderElement->Update(0, false);
	}
}

bool CREGeomCache::mfUpdate(InputLayoutHandle eVertFormat, int Flags, bool bTessellation)
{
	const bool bRet = Update(Flags, bTessellation);

	const int threadId = gRenDev->m_RP.m_nProcessThreadID;
	CryAutoLock<CryCriticalSection> lock(ms_updateListCS[threadId]);
	stl::find_and_erase(ms_updateList[threadId], this);

	m_Flags &= ~FCEF_DIRTY;
	return bRet;
}

volatile int* CREGeomCache::SetAsyncUpdateState(int& threadId)
{
	FUNCTION_PROFILER_RENDER_FLAT

	  ASSERT_IS_MAIN_THREAD(gRenDev->m_pRT);
	threadId = gRenDev->m_RP.m_nFillThreadID;

	m_bUpdateFrame[threadId] = false;

	CryAutoLock<CryCriticalSection> lock(ms_updateListCS[threadId]);
	stl::push_back_unique(ms_updateList[threadId], this);

	CryInterlockedIncrement(&m_transformUpdateState[threadId]);
	return &m_transformUpdateState[threadId];
}

DynArray<CREGeomCache::SMeshRenderData>* CREGeomCache::GetMeshFillDataPtr()
{
	FUNCTION_PROFILER_RENDER_FLAT

	  assert(gRenDev->m_pRT->IsMainThread(true));
	const int threadId = gRenDev->m_RP.m_nFillThreadID;
	return &m_meshFillData[threadId];
}

DynArray<CREGeomCache::SMeshRenderData>* CREGeomCache::GetRenderDataPtr()
{
	FUNCTION_PROFILER_RENDER_FLAT

	  assert(gRenDev->m_pRT->IsMainThread(true));
	return &m_meshRenderData;
}

void CREGeomCache::DisplayFilledBuffer(const int threadId)
{
	if (m_bUpdateFrame[threadId])
	{
		// You need to call SetAsyncUpdateState before DisplayFilledBuffer
		__debugbreak();
	}
	m_bUpdateFrame[threadId] = true;
}

InputLayoutHandle CREGeomCache::GetVertexFormat() const
{
	return EDefaultInputLayouts::P3F_C4B_T2F;
}

bool CREGeomCache::GetGeometryInfo(SGeometryInfo& streams, bool bSupportTessellation)
{
	ZeroStruct(streams);
	streams.eVertFormat = GetVertexFormat();
	streams.nFirstIndex = 0;
	streams.nFirstVertex = 0;
	streams.nNumIndices = 0;
	streams.primitiveType = eptTriangleList;
	return true;
}

void CREGeomCache::DrawToCommandList(CRenderObject* pObj, const SGraphicsPipelinePassContext& ctx)
{
	//mfUpdate(0, FCEF_TRANSFORM, false); //TODO: check if correct

}

inline static void getObjMatrix(UFloat4* sData, const register float* pData, const bool bRelativeToCamPos, const Vec3& vRelativeToCamPos)
{
	sData[0].Load(&pData[0]);
	sData[1].Load(&pData[4]);
	sData[2].Load(&pData[8]);

	if (bRelativeToCamPos)
	{
		sData[0].f[3] -= vRelativeToCamPos.x;
		sData[1].f[3] -= vRelativeToCamPos.y;
		sData[2].f[3] -= vRelativeToCamPos.z;
	}
}

// Each call of CREGeomCache::mfDraw render *all* meshes that share the same material in the geom cache. See CGeomCacheRenderNode::Render
bool CREGeomCache::mfDraw(CShader* ef, SShaderPass* sfm)
{
	PROFILE_FRAME(CREGeomCache::mfDraw);

	const uint numMeshes = m_meshRenderData.size();
	CD3D9Renderer* const pRenderer = gcpRendD3D;

	SRenderPipeline& rRP = pRenderer->m_RP;
	CRenderObject* const pRenderObject = rRP.m_pCurObject;
	Matrix34A matrix = pRenderObject->m_II.m_Matrix;
	CHWShader_D3D* const pCurVS = (CHWShader_D3D*)sfm->m_VShader;

	const CCamera& camera = gRenDev->GetCamera();

	Matrix44A prevMatrix;
	CMotionBlur::GetPrevObjToWorldMat(rRP.m_pCurObject, prevMatrix);

	const uint64 oldFlagsShader_RT = rRP.m_FlagsShader_RT;
	uint64 flagsShader_RT = rRP.m_FlagsShader_RT;
	const int oldFlagsPerFlush = rRP.m_FlagsPerFlush;
	bool bResetVertexDecl = false;

	const bool bRelativeToCam = !(rRP.m_pCurObject->m_ObjFlags & FOB_NEAREST);
	const Vec3& vRelativeToCamPos = bRelativeToCam ? gRenDev->GetRCamera().vOrigin : Vec3(ZERO);

#ifdef SUPPORTS_STATIC_INST_CB
	if (pRenderer->CV_r_StaticInstCB)
	{
		flagsShader_RT &= ~g_HWSR_MaskBit[HWSR_STATIC_INST_DATA];
	}
#endif

	for (uint nMesh = 0; nMesh < numMeshes; ++nMesh)
	{
		const SMeshRenderData& meshData = m_meshRenderData[nMesh];

		CRenderMesh* const pRenderMesh = static_cast<CRenderMesh*>(meshData.m_pRenderMesh.get());
		const uint numInstances = meshData.m_instances.size();

		if (pRenderMesh && numInstances > 0)
		{
			PROFILE_LABEL(pRenderMesh->GetSourceName() ? pRenderMesh->GetSourceName() : "Unknown mesh-resource name");

			const CRenderMesh* const pVertexContainer = pRenderMesh->_GetVertexContainer();

			if (!pVertexContainer->_HasVBStream(VSF_GENERAL) || !pRenderMesh->_HasIBStream())
			{
				// Should never happen. Video buffer is missing
				continue;
			}

			const bool bHasVelocityStream = pRenderMesh->_HasVBStream(VSF_VERTEX_VELOCITY);
			const bool bIsMotionBlurPass = (rRP.m_PersFlags2 & RBPF2_MOTIONBLURPASS) != 0;

			pRenderMesh->BindStreamsToRenderPipeline();

			rRP.m_RendNumVerts = pRenderMesh->_GetNumVerts();

			if (ef->m_HWTechniques.Num() && pRenderMesh->CanRender())
			{
				const TRenderChunkArray& chunks = pRenderMesh->GetChunks();
				const uint numChunks = chunks.size();

				for (uint i = 0; i < numChunks; ++i)
				{
					const CRenderChunk& chunk = chunks[i];
					if (chunk.m_nMatID != m_materialId)
					{
						continue;
					}

					rRP.m_FirstIndex = chunk.nFirstIndexId;
					rRP.m_RendNumIndices = chunk.nNumIndices;

					const bool bUseInstancing = false;

					TempDynInstVB instVB;
					uint numInstancesToDraw = 0;
					byte* __restrict pInstanceMatricesVB = NULL;

					// Note: Geom cache instancing is a horrible mess at the moment, because it re-uses
					// FX_DrawInstances which supports both constant based and attribute based instancing
					// and all platforms.
					//
					// This only sets up the data structures for D3D11 PC & Durango attribute based
					// instancing. Need to clean this up later and ideally use constant based instancing.

					const uint64 lastFlagsShader_RT = rRP.m_FlagsShader_RT;
					rRP.m_FlagsShader_RT = flagsShader_RT; // | (bUseInstancing ? g_HWSR_MaskBit[HWSR_INSTANCING_ATTR] : 0);
					if (lastFlagsShader_RT != rRP.m_FlagsShader_RT)
					{
						pCurVS->mfSet(bUseInstancing ? HWSF_INSTANCED : 0);
					}

					CHWShader_D3D::SHWSInstance* pVPInst = pCurVS->m_pCurInst;
					int32 nUsedAttr = 3, nInstAttrMask = 0;
					byte Attributes[32];

					if (bUseInstancing)
					{
						pVPInst->GetInstancingAttribInfo(Attributes, nUsedAttr, nInstAttrMask);
						instVB.Allocate(numInstances, nUsedAttr * INST_PARAM_SIZE);
						pInstanceMatricesVB = (byte*)(instVB.Lock());
					}

					const uint32 nStride = nUsedAttr * sizeof(float[4]);

					// Fill the stream 3 for per-instance data
					byte* pWalkData = pInstanceMatricesVB;
					for (uint nInstance = 0; nInstance < numInstances; ++nInstance)
					{
						const SMeshInstance& instance = meshData.m_instances[nInstance];

						Matrix34A pieceMatrix = matrix * instance.m_matrix;

						AABB pieceWorldAABB;
						pieceWorldAABB.SetTransformedAABB(pieceMatrix, instance.m_aabb);
						if (!camera.IsAABBVisible_F(pieceWorldAABB))
						{
							continue;
						}

						// Needs to be in this scope, because it's used by FX_DrawIndexedMesh
						Matrix44A prevPieceMatrix = prevMatrix * instance.m_prevMatrix;

						if (bIsMotionBlurPass)
						{
							const float fThreshold = 0.01f;
							if (bUseInstancing || (rRP.m_nBatchFilter & FB_Z) || !Matrix34::IsEquivalent(pieceMatrix, Matrix34(prevPieceMatrix), fThreshold) || bHasVelocityStream)
							{
								rRP.m_FlagsPerFlush |= RBSI_CUSTOM_PREVMATRIX;
								rRP.m_pPrevMatrix = &prevPieceMatrix;
							}
							else
							{
								// Don't draw pieces without any motion in motion blur pass
								continue;
							}
						}

						if (!bUseInstancing)
						{
							pRenderObject->m_II.m_Matrix = pieceMatrix;
							pCurVS->mfSetParametersPI(NULL, ef);

							// Check if instancing messed with vertex declaration
							if (bResetVertexDecl)
							{
								pRenderer->FX_SetVertexDeclaration(rRP.m_FlagsStreams_Decl, rRP.m_CurVFormat);
								bResetVertexDecl = false;
							}

							pRenderer->FX_DrawIndexedMesh(pRenderMesh->_GetPrimitiveType());
						}
						else
						{
							UFloat4* __restrict vMatrix = (UFloat4*)pWalkData;
							getObjMatrix(vMatrix, pieceMatrix.GetData(), bRelativeToCam, vRelativeToCamPos);

							float* __restrict fParm = (float*)&pWalkData[3 * sizeof(float[4])];
							pWalkData += nStride;

							if (pVPInst->m_nParams_Inst >= 0)
							{
								SCGParamsGroup& Group = CGParamManager::s_Groups[pVPInst->m_nParams_Inst];
								fParm = pCurVS->mfSetParametersPI(Group.pParams, Group.nParams, fParm, eHWSC_Vertex, 40);
							}

							++numInstancesToDraw;
						}
					}

					if (bUseInstancing)
					{
						instVB.Unlock();
						instVB.Bind(3, nUsedAttr * INST_PARAM_SIZE);
						instVB.Release();

						pCurVS->mfSetParametersPI(NULL, ef);
						pRenderer->FX_DrawInstances(ef, sfm, 0, 0, numInstancesToDraw - 1, nUsedAttr, pInstanceMatricesVB, nInstAttrMask, Attributes, 0);
						bResetVertexDecl = true;
					}
				}
			}
		}
	}

	// Reset matrix to original value for cases when render object gets reused
	pRenderObject->m_II.m_Matrix = matrix;
	rRP.m_FlagsShader_RT = oldFlagsShader_RT;
	rRP.m_FlagsPerFlush = oldFlagsPerFlush;

	return true;
}

#endif
