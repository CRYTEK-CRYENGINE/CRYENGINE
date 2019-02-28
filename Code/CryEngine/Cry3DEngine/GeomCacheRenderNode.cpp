// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   GeomCacheRenderNode.cpp
//  Created:     19/7/2012 by Axel Gneiting
//  Description: Draws geometry caches
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "xxhash.h"

#if defined(USE_GEOM_CACHES)

	#include "GeomCacheRenderNode.h"
	#include "GeomCacheManager.h"
	#include "MatMan.h"

	#include <CryThreading/IJobManager_JobDelegator.h>

DECLARE_JOB("GeomCacheUpdateMesh", TGeomCacheUpdateMeshJob, CGeomCacheRenderNode::UpdateMesh_JobEntry);

namespace
{
// Constants
const float kDefaultMaxViewDist = 1000.0f;
}

CGeomCacheRenderNode::CGeomCacheRenderNode()
	: m_pGeomCache(NULL)
	, m_playbackTime(0.0f)
	, m_pPhysicalEntity(NULL)
	, m_maxViewDist(kDefaultMaxViewDist)
	, m_bBox(0.0f)
	, m_currentAABB(0.0f)
	, m_currentDisplayAABB(0.0f)
	, m_standInVisible(eStandInType_None)
	, m_pStandIn(NULL)
	, m_pFirstFrameStandIn(NULL)
	, m_pLastFrameStandIn(NULL)
	, m_standInDistance(0.0f)
	, m_streamInDistance(0.0f)
	, m_bInitialized(false)
	, m_bLooping(false)
	, m_bIsStreaming(false)
	, m_bFilledFrameOnce(false)
	, m_bBoundsChanged(true)
	, m_bDrawing(true)
	, m_bTransformReady(true)
{
	GetInstCount(GetRenderNodeType())++;

	m_matrix.SetIdentity();
	m_pMaterial = GetMatMan()->GetDefaultMaterial();

	SetRndFlags(ERF_HAS_CASTSHADOWMAPS, true);
	SetRndFlags(ERF_CASTSHADOWMAPS, true);
}

CGeomCacheRenderNode::~CGeomCacheRenderNode()
{
	GetInstCount(GetRenderNodeType())--;

	if (m_pGeomCache)
	{
		m_pGeomCache->RemoveListener(this);
	}

	Clear(true);
	Get3DEngine()->FreeRenderNodeState(this);
}

const char* CGeomCacheRenderNode::GetEntityClassName() const
{
	return "GeomCache";
}

const char* CGeomCacheRenderNode::GetName() const
{
	if (m_pGeomCache)
	{
		return m_pGeomCache->GetFilePath();
	}

	return "GeomCacheNotSet";
}

Vec3 CGeomCacheRenderNode::GetPos(bool bWorldOnly) const
{
	assert(bWorldOnly);
	return m_matrix.GetTranslation();
}

void CGeomCacheRenderNode::SetBBox(const AABB& bBox)
{
	m_bBox = bBox;
}

const AABB CGeomCacheRenderNode::GetBBox() const
{
	return m_bBox;
}

void CGeomCacheRenderNode::UpdateBBox()
{
	AABB newAABB;

	const Vec3 vCamPos = Get3DEngine()->GetRenderingCamera().GetPosition();
	float distance = Distance::Point_Point(vCamPos, m_matrix.GetTranslation());

	const bool bGeomCacheLoaded = m_pGeomCache ? m_pGeomCache->IsLoaded() : false;

	const bool bAllowStandIn = GetCVars()->e_Lods != 0;
	const bool bInStandInDistance = distance > m_standInDistance && bAllowStandIn;

	EStandInType selectedStandIn = SelectStandIn();
	IStatObj* pStandIn = GetStandIn(selectedStandIn);

	if (pStandIn && (bInStandInDistance || !bGeomCacheLoaded))
	{
		m_standInVisible = selectedStandIn;
		newAABB = pStandIn->GetAABB();
	}
	else
	{
		m_standInVisible = eStandInType_None;
		newAABB = m_currentDisplayAABB;
	}

	if (newAABB.min != m_currentAABB.min || newAABB.max != m_currentAABB.max)
	{
		m_bBoundsChanged = true;
		m_currentAABB = newAABB;
	}

	if (m_streamInDistance > 0.0f)
	{
		m_bIsStreaming = distance <= m_streamInDistance;
	}
}

void CGeomCacheRenderNode::GetLocalBounds(AABB& bbox)
{
	bbox = m_currentAABB;
}

void CGeomCacheRenderNode::OffsetPosition(const Vec3& delta)
{
	m_matrix.SetTranslation(m_matrix.GetTranslation() + delta);
	UpdateBBox();
}

bool CGeomCacheRenderNode::DidBoundsChange()
{
	bool bBoundsChanged = m_bBoundsChanged;

	if (bBoundsChanged)
	{
		CalcBBox();
	}

	m_bBoundsChanged = false;
	return bBoundsChanged;
}

/* Geom caches are rendered using a custom render element for performance reasons (CREGeomCache).
 * We only call mfAdd once per material, so a lot of meshes can be rendered with just one CRenderObject in the render pipeline.
 * Mesh and transform updates run asynchronously started from FillFrameAsync and are synchronized in the render thread (CREGeomCache::Update)
 * Visible meshes are added to a SMeshRenderData vector in UpdateTransformsRec. The lists are rendered in CREGeomCache::mfDraw
 * Downside is that meshes in the cache are not sorted by depth for transparency passes
 */
void CGeomCacheRenderNode::Render(const struct SRendParams& rendParams, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!m_bInitialized || !m_bDrawing || (m_renderMeshes.empty() && m_renderMeshUpdateContexts.empty())
	    || !m_pGeomCache || m_dwRndFlags & ERF_HIDDEN || !passInfo.RenderGeomCaches())
	{
		return;
	}

	m_pGeomCache->SetLastDrawMainFrameId(passInfo.GetMainFrameID());

	SRendParams drawParams = rendParams;

	drawParams.pMatrix = &m_matrix;

	static ICVar* pGraphicsPipelineCV = gEnv->pConsole->GetCVar("r_GraphicsPipeline");
	static ICVar* pMotionVectorsCV = gEnv->pConsole->GetCVar("r_MotionVectors");

	switch (m_standInVisible)
	{
	case eStandInType_None:
		{
	#ifndef _RELEASE
			if (GetCVars()->e_GeomCacheDebugDrawMode == 3)
			{
				break;
			}
	#endif

			drawParams.pMaterial = m_pMaterial;

			IRenderer* const pRenderer = GetRenderer();
			CRenderObject* pRenderObject = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();

			if (pRenderObject)
			{
				FillRenderObject(drawParams, passInfo, m_pMaterial, pRenderObject);

				if (m_pRenderElements.size() > 0 && passInfo.IsGeneralPass())
				{
					// Only need to call this once because SRenderObjData::m_pInstance is the same for all of them
					m_pRenderElements.begin()->second.m_pRenderElement->SetupMotionBlur(pRenderObject, passInfo);
				}

				for (TRenderElementMap::iterator iter = m_pRenderElements.begin(); iter != m_pRenderElements.end(); ++iter)
				{
					const uint materialId = iter->first;
					CREGeomCache* pCREGeomCache = iter->second.m_pRenderElement;

					SShaderItem& shaderItem = m_pMaterial->GetShaderItem(materialId);
					const int afterWater = rendParams.nAfterWater;

					if (pGraphicsPipelineCV->GetIVal() == 0)
					{
						passInfo.GetIRenderView()->AddRenderObject(pCREGeomCache, shaderItem, pRenderObject, passInfo, EFSLIST_GENERAL, afterWater);
					}
					else
					{
						for (auto& meshData : *(pCREGeomCache->GetMeshFillDataPtr()))
						{
							if (meshData.m_pRenderMesh == nullptr)
								continue;

							for (auto& chunk : meshData.m_pRenderMesh->GetChunks())
							{
								if (chunk.m_nMatID != materialId)
								{
									continue;
								}

								CRenderElement* RESTRICT_POINTER pREMesh = chunk.pRE;
								IRenderShaderResources* pR = shaderItem.m_pShaderResources;
								IShader* RESTRICT_POINTER pS = shaderItem.m_pShader;

								for (auto& instance : meshData.m_instances)
								{
									if (pREMesh == NULL || pS == NULL || pR == NULL)
										continue;

									if (pS->GetFlags2() & EF2_NODRAW)
										continue;

									if (passInfo.IsShadowPass() && (pR->GetResFlags() & MTL_FLAG_NOSHADOW))
										continue;

									Matrix34A pieceMatrix = m_matrix * instance.m_matrix;

									AABB pieceWorldAABB;
									pieceWorldAABB.SetTransformedAABB(pieceMatrix, instance.m_aabb);
									if (!passInfo.GetCamera().IsAABBVisible_F(pieceWorldAABB))
									{
										continue;
									}

									auto pInstanceRenderObject = pRenderer->EF_DuplicateRO(pRenderObject, passInfo);
									pInstanceRenderObject->SetMatrix(pieceMatrix, passInfo);

									if (pMotionVectorsCV->GetIVal() && passInfo.IsGeneralPass() && ((pInstanceRenderObject->m_ObjFlags & FOB_DYNAMIC_OBJECT) != 0))
									{
										if (SRenderObjData* pRenderObjData = pInstanceRenderObject->GetObjData())
										{
											uint8 hashableData[] =
											{
												0, 0, 0, 0, 0, 0, 0, 0,  //this part will be filled with the address of pCREGeomCache
												(uint8)std::distance(pCREGeomCache->GetMeshFillDataPtr()->begin(), &meshData),
												(uint8)std::distance(meshData.m_pRenderMesh->GetChunks().begin(),  &chunk),
												(uint8)std::distance(meshData.m_instances.begin(),                 &instance)
											};

											memcpy(hashableData, &pCREGeomCache, sizeof(CREGeomCache*)); //copy the address of the pCREGeomCache into our hash-data-object
											pRenderObjData->m_uniqueObjectId = static_cast<uintptr_t>(XXH64(hashableData, sizeof(hashableData), 0)) + reinterpret_cast<uintptr_t>(this);

											pCREGeomCache->SetupMotionBlur(pInstanceRenderObject, passInfo);
										}
									}

									passInfo.GetIRenderView()->AddRenderObject(pREMesh, shaderItem, pInstanceRenderObject, passInfo, EFSLIST_GENERAL, afterWater);
								}
							}
						}
					}
				}
			}
			break;
		}
	case eStandInType_Default:
		{
			// Override material if there stand in has a material that is not default
			IMaterial* pStandInMaterial = m_pStandIn->GetMaterial();
			drawParams.pMaterial = (pStandInMaterial && !pStandInMaterial->IsDefault()) ? pStandInMaterial : drawParams.pMaterial;

			m_pStandIn->Render(drawParams, passInfo);
			break;
		}
	case eStandInType_FirstFrame:
		{
			// Override material if there stand in has a material that is not default
			IMaterial* pStandInMaterial = m_pFirstFrameStandIn->GetMaterial();
			drawParams.pMaterial = (pStandInMaterial && !pStandInMaterial->IsDefault()) ? pStandInMaterial : drawParams.pMaterial;

			m_pFirstFrameStandIn->Render(drawParams, passInfo);
			break;
		}
	case eStandInType_LastFrame:
		{
			// Override material if there stand in has a material that is not default
			IMaterial* pStandInMaterial = m_pLastFrameStandIn->GetMaterial();
			drawParams.pMaterial = (pStandInMaterial && !pStandInMaterial->IsDefault()) ? pStandInMaterial : drawParams.pMaterial;

			m_pLastFrameStandIn->Render(drawParams, passInfo);
			break;
		}
	}
}

void CGeomCacheRenderNode::SetMaterial(IMaterial* pMat)
{
	if (pMat)
	{
		m_pMaterial = pMat;
	}
	else if (m_pGeomCache)
	{
		IMaterial* pMaterial = m_pGeomCache->GetMaterial();
		m_pMaterial = pMaterial;
	}
	else
	{
		m_pMaterial = GetMatMan()->GetDefaultMaterial();
	}

	UpdatePhysicalMaterials();
}

IMaterial* CGeomCacheRenderNode::GetMaterial(Vec3* pHitPos) const
{
	if (m_pMaterial)
	{
		return m_pMaterial;
	}
	else if (m_pGeomCache)
	{
		return m_pGeomCache->GetMaterial();
	}

	return NULL;
}

float CGeomCacheRenderNode::GetMaxViewDist()
{
	return m_maxViewDist;
}

void CGeomCacheRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "GeomCache");
	pSizer->AddObject(this, sizeof(*this));
}

//////////////////////////////////////////////////////////////////////////
void CGeomCacheRenderNode::UpdateStreamingPriority(const SUpdateStreamingPriorityContext& ctx)
{
	float fObjScale = max(0.001f, GetMatrix().GetColumn0().GetLength());
	float fInvObjScale = 1.0f / fObjScale;
	UpdateStreamableComponents(ctx.importance, ctx.distance, ctx.bFullUpdate, ctx.lod, fInvObjScale, ctx.bFullUpdate);
}

void CGeomCacheRenderNode::SetMatrix(const Matrix34& matrix)
{
	m_matrix = matrix;
	CalcBBox();
}

void CGeomCacheRenderNode::CalcBBox()
{
	m_bBox = AABB(0.0f);

	if (!m_pGeomCache || !m_pGeomCache->IsValid())
	{
		return;
	}

	m_bBox.SetTransformedAABB(m_matrix, m_currentAABB);
}

bool CGeomCacheRenderNode::LoadGeomCache(const char* sGeomCacheFileName)
{
	Clear(false);

	m_pGeomCache = static_cast<CGeomCache*>(Get3DEngine()->LoadGeomCache(sGeomCacheFileName));

	if (m_pGeomCache && !m_pGeomCache->IsValid())
	{
		m_pGeomCache = NULL;
	}

	if (m_pGeomCache)
	{
		m_currentAABB = m_pGeomCache->GetAABB();
		m_bBoundsChanged = true;
		m_pMaterial = m_pGeomCache->GetMaterial();

		const std::vector<SGeomCacheStaticNodeData>& staticNodeData = m_pGeomCache->GetStaticNodeData();
		m_nodeMatrices.resize(staticNodeData.size());
		uint currentNodeIndex = 0;
		InitTransformsRec(currentNodeIndex, staticNodeData, QuatTNS(IDENTITY));

		m_pGeomCache->AddListener(this);

		if (m_pGeomCache->IsLoaded())
		{
			return Initialize();
		}
	}

	return true;
}

bool CGeomCacheRenderNode::Initialize()
{
	assert(!m_bInitialized);
	if (m_bInitialized)
	{
		return true;
	}

	if (m_pGeomCache)
	{
		if (!InitializeRenderMeshes())
		{
			return false;
		}

		const std::vector<SGeomCacheStaticMeshData>& staticMeshData = m_pGeomCache->GetStaticMeshData();

		const uint numMeshes = staticMeshData.size();
		for (uint i = 0; i < numMeshes; ++i)
		{
			const SGeomCacheStaticMeshData& meshData = staticMeshData[i];
			uint numMaterials = meshData.m_materialIds.size();

			for (uint j = 0; j < numMaterials; ++j)
			{
				const uint16 materialId = meshData.m_materialIds[j];
				if (m_pRenderElements.find(materialId) == m_pRenderElements.end())
				{
					CREGeomCache* pRenderElement = static_cast<CREGeomCache*>(GetRenderer()->EF_CreateRE(eDATA_GeomCache));

					SGeomCacheRenderElementData renderElementData;
					renderElementData.m_pRenderElement = pRenderElement;
					renderElementData.m_pUpdateState = (volatile int*)NULL;
					renderElementData.m_pCurrentFillData = NULL;

					m_pRenderElements[materialId] = renderElementData;
					pRenderElement->InitializeRenderElement(numMeshes, &m_renderMeshes[0], materialId);
				}
			}
		}

		GetGeomCacheManager()->RegisterForStreaming(this);

		m_bInitialized = true;

		return true;
	}

	return false;
}

void CGeomCacheRenderNode::Clear(bool bWaitForStreamingJobs)
{
	m_bInitialized = false;

	GetGeomCacheManager()->UnRegisterForStreaming(this, bWaitForStreamingJobs);

	m_renderMeshes.clear();
	m_renderMeshUpdateContexts.clear();

	for (TRenderElementMap::iterator iter = m_pRenderElements.begin(); iter != m_pRenderElements.end(); ++iter)
	{
		CREGeomCache* pCREGeomCache = iter->second.m_pRenderElement;
		pCREGeomCache->Release();
	}

	m_currentAABB = AABB(0.0f);
	m_currentDisplayAABB = AABB(0.0f);
	m_pRenderElements.clear();
}

void CGeomCacheRenderNode::SetPlaybackTime(const float time)
{
	if (m_pGeomCache)
	{
		const float duration = m_pGeomCache->GetDuration();
		const bool bInsideTimeRange = (time >= 0.0f && (m_bLooping || time <= duration));

		float clampedTime = time < 0.0f ? 0.0f : time;
		if (!m_bLooping)
		{
			clampedTime = time > duration ? duration : time;
		}

		m_playbackTime = clampedTime;
		m_streamingTime = clampedTime;

		if (m_pGeomCache && bInsideTimeRange)
		{
			StartStreaming(clampedTime);
			return;
		}
	}

	StopStreaming();
}

void CGeomCacheRenderNode::StartStreaming(const float time)
{
	if (m_pGeomCache && time >= 0.0f && (m_bLooping || time <= m_pGeomCache->GetDuration()))
	{
		m_streamingTime = time;
		m_bIsStreaming = true;
	}
}

void CGeomCacheRenderNode::StopStreaming()
{
	m_bIsStreaming = false;
}

bool CGeomCacheRenderNode::IsLooping() const
{
	return m_bLooping;
}

void CGeomCacheRenderNode::SetLooping(const bool bEnable)
{
	m_bLooping = bEnable;
}

bool CGeomCacheRenderNode::IsStreaming() const
{
	return m_bIsStreaming && m_pGeomCache && !m_pGeomCache->PlaybackFromMemory();
}

float CGeomCacheRenderNode::GetPrecachedTime() const
{
	return GetGeomCacheManager()->GetPrecachedTime(this);
}

void CGeomCacheRenderNode::StartAsyncUpdate()
{
	FUNCTION_PROFILER_3DENGINE;

	m_bTransformReady = false;

	for (TRenderElementMap::iterator iter = m_pRenderElements.begin(); iter != m_pRenderElements.end(); ++iter)
	{
		SGeomCacheRenderElementData& data = iter->second;
		CREGeomCache* pCREGeomCache = data.m_pRenderElement;
		data.m_pUpdateState = pCREGeomCache->SetAsyncUpdateState(data.m_threadId);
		data.m_pCurrentFillData = pCREGeomCache->GetMeshFillDataPtr();
	}

	const std::vector<SGeomCacheStaticMeshData>& staticMeshData = m_pGeomCache->GetStaticMeshData();

	const uint numDynamicRenderMeshes = m_renderMeshUpdateContexts.size();
	for (uint i = 0; i < numDynamicRenderMeshes; ++i)
	{
		SGeomCacheRenderMeshUpdateContext& updateContext = m_renderMeshUpdateContexts[i];

		_smart_ptr<IRenderMesh> pRenderMesh = SetupDynamicRenderMesh(updateContext);
		m_renderMeshUpdateContexts[i].m_pRenderMesh = pRenderMesh;

		const SGeomCacheStaticMeshData& currentMeshData = staticMeshData[updateContext.m_meshId];
		const uint numMaterials = currentMeshData.m_materialIds.size();
		for (uint j = 0; j < numMaterials; ++j)
		{
			const uint16 materialId = currentMeshData.m_materialIds[j];
			SGeomCacheRenderElementData& data = m_pRenderElements[materialId];
			CREGeomCache::SMeshRenderData& meshData = (*data.m_pCurrentFillData)[updateContext.m_meshId];
			meshData.m_pRenderMesh = pRenderMesh;
		}
	}
}

void CGeomCacheRenderNode::SkipFrameFill()
{
	const uint numDynamicRenderMeshes = m_renderMeshUpdateContexts.size();
	for (uint i = 0; i < numDynamicRenderMeshes; ++i)
	{
		SGeomCacheRenderMeshUpdateContext& updateContext = m_renderMeshUpdateContexts[i];
		if (updateContext.m_pUpdateState)
		{
			CryInterlockedDecrement(updateContext.m_pUpdateState);
		}
	}

	for (TRenderElementMap::iterator iter = m_pRenderElements.begin(); iter != m_pRenderElements.end(); ++iter)
	{
		CryInterlockedDecrement(iter->second.m_pUpdateState);
	}

	m_bTransformReady = true;
	m_bTransformReadyCV.Notify();
}

bool CGeomCacheRenderNode::FillFrameAsync(const char* const pFloorFrameData, const char* const pCeilFrameData, const float lerpFactor)
{
	FUNCTION_PROFILER_3DENGINE;

	CryAutoLock<CryCriticalSection> fillLock(m_fillCS);

	if ((m_renderMeshes.empty() && m_renderMeshUpdateContexts.empty())
	    || (!m_renderMeshUpdateContexts.empty() && m_renderMeshUpdateContexts[0].m_pUpdateState == NULL)
	    || (m_standInVisible != eStandInType_None && m_bFilledFrameOnce))
	{
		return false;
	}

	const CGeomCache* const pGeomCache = m_pGeomCache;
	assert(pGeomCache);

	if (!pGeomCache)
	{
		return false;
	}

	const std::vector<SGeomCacheStaticMeshData>& staticMeshData = pGeomCache->GetStaticMeshData();
	const std::vector<SGeomCacheStaticNodeData>& staticNodeData = pGeomCache->GetStaticNodeData();

	const uint numMeshes = staticMeshData.size();
	const uint numNodes = staticNodeData.size();

	if (numMeshes == 0 || numNodes == 0)
	{
		return false;
	}

	// Computer pointers to mesh & node data in frames
	const GeomCacheFile::SFrameHeader* const floorFrameHeader = reinterpret_cast<const GeomCacheFile::SFrameHeader* const>(pFloorFrameData);
	const char* pFloorMeshData = pFloorFrameData + sizeof(GeomCacheFile::SFrameHeader);
	const char* const pFloorNodeData = pFloorFrameData + sizeof(GeomCacheFile::SFrameHeader) + floorFrameHeader->m_nodeDataOffset;

	const GeomCacheFile::SFrameHeader* const ceilFrameHeader = reinterpret_cast<const GeomCacheFile::SFrameHeader* const>(pCeilFrameData);
	const char* pCeilMeshData = pCeilFrameData + sizeof(GeomCacheFile::SFrameHeader);
	const char* const pCeilNodeData = pCeilFrameData + sizeof(GeomCacheFile::SFrameHeader) + ceilFrameHeader->m_nodeDataOffset;

	// Update geom cache AABB
	AABB floorAABB(Vec3(floorFrameHeader->m_frameAABBMin[0], floorFrameHeader->m_frameAABBMin[1], floorFrameHeader->m_frameAABBMin[2]),
	               Vec3(floorFrameHeader->m_frameAABBMax[0], floorFrameHeader->m_frameAABBMax[1], floorFrameHeader->m_frameAABBMax[2]));
	AABB ceilAABB(Vec3(ceilFrameHeader->m_frameAABBMin[0], ceilFrameHeader->m_frameAABBMin[1], ceilFrameHeader->m_frameAABBMin[2]),
	              Vec3(ceilFrameHeader->m_frameAABBMax[0], ceilFrameHeader->m_frameAABBMax[1], ceilFrameHeader->m_frameAABBMax[2]));

	m_currentDisplayAABB = floorAABB;
	m_currentDisplayAABB.Add(ceilAABB);

	// Update meshes & clear instances
	for (uint meshId = 0; meshId < numMeshes; ++meshId)
	{
		const SGeomCacheStaticMeshData& meshData = staticMeshData[meshId];
		for (TRenderElementMap::iterator iter = m_pRenderElements.begin(); iter != m_pRenderElements.end(); ++iter)
		{
			SGeomCacheRenderElementData& data = iter->second;
			(*data.m_pCurrentFillData)[meshId].m_instances.clear();
		}
	}

	// Add instance for current frame
	uint currentMeshIndex = 0;
	uint currentNodeIndex = 0;
	uint currentNodeDataOffset = 0;

	UpdateTransformsRec(currentMeshIndex, currentNodeIndex, staticNodeData, staticMeshData,
	                    currentNodeDataOffset, pFloorNodeData, pCeilNodeData, QuatTNS(IDENTITY), lerpFactor);

	m_bTransformReady = true;
	m_bTransformReadyCV.Notify();

	UpdatePhysicalEntity(NULL);

	uint currentRenderMesh = 0;
	for (uint meshId = 0; meshId < numMeshes; ++meshId)
	{
		const SGeomCacheStaticMeshData* pStaticMeshData = &staticMeshData[meshId];
		if (pStaticMeshData->m_animatedStreams != 0)
		{
			size_t offsetToNextMesh = 0;
			float meshLerpFactor = lerpFactor;
			SGeomCacheRenderMeshUpdateContext* pUpdateContext = &m_renderMeshUpdateContexts[currentRenderMesh++];

			if (GeomCacheDecoder::PrepareFillMeshData(*pUpdateContext, *pStaticMeshData, pFloorMeshData, pCeilMeshData, offsetToNextMesh, meshLerpFactor))
			{
				TGeomCacheUpdateMeshJob meshUpdateJob(pUpdateContext, pStaticMeshData, pFloorMeshData, pCeilMeshData, meshLerpFactor);
				meshUpdateJob.SetClassInstance(this);
				meshUpdateJob.SetPriorityLevel(JobManager::eRegularPriority);
				meshUpdateJob.Run();
			}
			else
			{
				CryInterlockedDecrement(pUpdateContext->m_pUpdateState);
			}

			pFloorMeshData += offsetToNextMesh;
			pCeilMeshData += offsetToNextMesh;
		}
	}

	for (TRenderElementMap::iterator iter = m_pRenderElements.begin(); iter != m_pRenderElements.end(); ++iter)
	{
		SGeomCacheRenderElementData& data = iter->second;
		data.m_pRenderElement->DisplayFilledBuffer(data.m_threadId);
		CryInterlockedDecrement(iter->second.m_pUpdateState);
	}

	m_bFilledFrameOnce = true;
	return true;
}

void CGeomCacheRenderNode::UpdateMesh_JobEntry(SGeomCacheRenderMeshUpdateContext* pUpdateContext, SGeomCacheStaticMeshData* pStaticMeshData,
                                               const char* pFloorMeshData, const char* pCeilMeshData, float lerpFactor)
{
	GeomCacheDecoder::FillMeshDataFromDecodedFrame(m_bFilledFrameOnce, *pUpdateContext, *pStaticMeshData, pFloorMeshData, pCeilMeshData, lerpFactor);
	CryInterlockedDecrement(pUpdateContext->m_pUpdateState);
}

void CGeomCacheRenderNode::ClearFillData()
{
	FUNCTION_PROFILER_3DENGINE;

	const std::vector<SGeomCacheStaticMeshData>& staticMeshData = m_pGeomCache->GetStaticMeshData();
	const uint numMeshes = staticMeshData.size();

	// Clear dynamic render meshes in fill buffer to release their unused memory
	for (uint meshId = 0; meshId < numMeshes; ++meshId)
	{
		const SGeomCacheStaticMeshData& meshData = staticMeshData[meshId];

		if (meshData.m_animatedStreams != 0)
		{
			for (TRenderElementMap::iterator iter = m_pRenderElements.begin(); iter != m_pRenderElements.end(); ++iter)
			{
				SGeomCacheRenderElementData& data = iter->second;
				DynArray<CREGeomCache::SMeshRenderData>* pFillData = data.m_pRenderElement->GetMeshFillDataPtr();
				(*pFillData)[meshId].m_pRenderMesh = NULL;
			}
		}
	}
}

void CGeomCacheRenderNode::InitTransformsRec(uint& currentNodeIndex, const std::vector<SGeomCacheStaticNodeData>& staticNodeData, const QuatTNS& currentTransform)
{
	const SGeomCacheStaticNodeData& currentNodeData = staticNodeData[currentNodeIndex];
	const QuatTNS newTransformQuat = currentTransform * currentNodeData.m_localTransform;
	const Matrix34 newTransformMatrix(newTransformQuat);
	m_nodeMatrices[currentNodeIndex] = newTransformMatrix;

	currentNodeIndex += 1;

	const uint32 numChildren = currentNodeData.m_numChildren;
	for (uint32 i = 0; i < numChildren; ++i)
	{
		InitTransformsRec(currentNodeIndex, staticNodeData, newTransformQuat);
	}
}

void CGeomCacheRenderNode::UpdateTransformsRec(uint& currentNodeIndex, uint& currentMeshIndex, const std::vector<SGeomCacheStaticNodeData>& staticNodeData,
                                               const std::vector<SGeomCacheStaticMeshData>& staticMeshData, uint& currentNodeDataOffset, const char* const pFloorNodeData,
                                               const char* const pCeilNodeData, const QuatTNS& currentTransform, const float lerpFactor)
{
	const SGeomCacheStaticNodeData& currentNodeData = staticNodeData[currentNodeIndex];

	const uint32 floorNodeFlags = *reinterpret_cast<const uint32* const>(pFloorNodeData + currentNodeDataOffset);
	const uint32 ceilNodeFlags = *reinterpret_cast<const uint32* const>(pCeilNodeData + currentNodeDataOffset);
	currentNodeDataOffset += sizeof(uint32);

	QuatTNS newTransformQuat;

	// Update transform
	if (currentNodeData.m_transformType == GeomCacheFile::eTransformType_Constant)
	{
		// Matrix from static data
		newTransformQuat = currentTransform * currentNodeData.m_localTransform;
	}
	else
	{
		// Matrix from frame data
		const QuatTNS* const pFloorTransform = reinterpret_cast<const QuatTNS* const>(pFloorNodeData + currentNodeDataOffset);
		const QuatTNS* const pCeilTransform = reinterpret_cast<const QuatTNS* const>(pCeilNodeData + currentNodeDataOffset);

		QuatTNS interpolatedTransform;
		if (!(floorNodeFlags& GeomCacheFile::eFrameFlags_Hidden) && !(ceilNodeFlags & GeomCacheFile::eFrameFlags_Hidden))
		{
			interpolatedTransform.q = Quat::CreateSlerp(pFloorTransform->q, pCeilTransform->q, lerpFactor);
			interpolatedTransform.t = Vec3::CreateLerp(pFloorTransform->t, pCeilTransform->t, lerpFactor);
			interpolatedTransform.s = Diag33::CreateLerp(pFloorTransform->s, pCeilTransform->s, lerpFactor);
		}
		else if (!(floorNodeFlags & GeomCacheFile::eFrameFlags_Hidden))
		{
			interpolatedTransform = *pFloorTransform;
		}
		else if (!(ceilNodeFlags & GeomCacheFile::eFrameFlags_Hidden))
		{
			interpolatedTransform = *pCeilTransform;
		}
		else
		{
			interpolatedTransform.SetIdentity();
		}

		newTransformQuat = currentTransform * interpolatedTransform;
		currentNodeDataOffset += sizeof(QuatTNS);
	}

	Matrix34 newTransformMatrix(newTransformQuat);

	if (currentNodeData.m_type == GeomCacheFile::eNodeType_Mesh)
	{
		const SGeomCacheStaticMeshData& currentMeshData = staticMeshData[currentNodeData.m_meshOrGeometryIndex];

		const bool bVisible = ((floorNodeFlags& GeomCacheFile::eFrameFlags_Hidden) == 0);

		if (bVisible)
		{
			CREGeomCache::SMeshInstance meshInstance;
			meshInstance.m_aabb = currentMeshData.m_aabb;
			meshInstance.m_matrix = newTransformMatrix;
			meshInstance.m_prevMatrix = m_bFilledFrameOnce ? m_nodeMatrices[currentNodeIndex] : newTransformMatrix;

	#ifndef _RELEASE
			const int debugDrawMode = GetCVars()->e_GeomCacheDebugDrawMode;
			if (debugDrawMode == 0 || debugDrawMode > 2
			    || (debugDrawMode == 1 && currentMeshData.m_animatedStreams != 0)
			    || (debugDrawMode == 2 && currentMeshData.m_animatedStreams == 0))
	#endif
			{
				const uint numMaterials = currentMeshData.m_materialIds.size();
				for (uint i = 0; i < numMaterials; ++i)
				{
					const uint16 materialId = currentMeshData.m_materialIds[i];
					SGeomCacheRenderElementData& data = m_pRenderElements[materialId];
					(*data.m_pCurrentFillData)[currentNodeData.m_meshOrGeometryIndex].m_instances.push_back(meshInstance);
				}
			}
		}
	}

	m_nodeMatrices[currentNodeIndex] = newTransformMatrix;

	currentNodeIndex += 1;

	const uint32 numChildren = currentNodeData.m_numChildren;
	for (uint32 i = 0; i < numChildren; ++i)
	{
		UpdateTransformsRec(currentNodeIndex, currentMeshIndex, staticNodeData, staticMeshData, currentNodeDataOffset,
		                    pFloorNodeData, pCeilNodeData, newTransformQuat, lerpFactor);
	}
}

void CGeomCacheRenderNode::FillRenderObject(const SRendParams& rendParams, const SRenderingPassInfo& passInfo, IMaterial* pMaterial, CRenderObject* pRenderObject)
{
	FUNCTION_PROFILER_3DENGINE;

	IRenderNode* const pRenderNode = rendParams.pRenderNode;
	IRenderer* const pRenderer = GetRenderer();

	pRenderObject->m_pRenderNode = rendParams.pRenderNode;
	pRenderObject->m_fDistance = rendParams.fDistance;

	pRenderObject->m_ObjFlags |= FOB_TRANS_MASK | FOB_DYNAMIC_OBJECT;
	pRenderObject->m_ObjFlags |= rendParams.dwFObjFlags;

	pRenderObject->SetAmbientColor(rendParams.AmbientColor, passInfo);

	if (rendParams.nTextureID >= 0)
	{
		pRenderObject->m_nTextureID = rendParams.nTextureID;
	}

	pRenderObject->SetMatrix(*rendParams.pMatrix, passInfo);
	pRenderObject->m_nClipVolumeStencilRef = rendParams.nClipVolumeStencilRef;

	SRenderObjData* pRenderObjData = pRenderObject->GetObjData();
	if (pRenderObjData)
	{
		pRenderObjData->m_uniqueObjectId = reinterpret_cast<uintptr_t>(this);
		pRenderObjData->m_pLayerEffectParams = rendParams.pLayerEffectParams;
		pRenderObjData->m_nVisionParams = rendParams.nVisionParams;
		pRenderObjData->m_nHUDSilhouetteParams = rendParams.nHUDSilhouettesParams;
	}

	pRenderObject->m_ObjFlags |= FOB_INSHADOW;
	pRenderObject->m_fAlpha = rendParams.fAlpha;
	pRenderObject->m_DissolveRef = rendParams.nDissolveRef;

	if (rendParams.nAfterWater)
	{
		pRenderObject->m_ObjFlags |= FOB_AFTER_WATER;
	}
	else
	{
		pRenderObject->m_ObjFlags &= ~FOB_AFTER_WATER;
	}

	pRenderObject->m_pCurrMaterial = pMaterial;
}

bool CGeomCacheRenderNode::InitializeRenderMeshes()
{
	const std::vector<SGeomCacheStaticMeshData>& staticMeshData = m_pGeomCache->GetStaticMeshData();

	const uint numMeshes = staticMeshData.size();
	for (uint i = 0; i < numMeshes; ++i)
	{
		const SGeomCacheStaticMeshData& meshData = staticMeshData[i];

		IRenderMesh* pRenderMesh = NULL;

		// Only meshes with constant topology for now. TODO: Add support for heterogeneous meshes.
		if (meshData.m_animatedStreams == 0)
		{
			pRenderMesh = GetGeomCacheManager()->GetMeshManager().GetStaticRenderMesh(meshData.m_hash);

			assert(pRenderMesh != NULL);
			if (!pRenderMesh)
			{
				return false;
			}
		}
		else if (meshData.m_animatedStreams != 0)
		{
			SGeomCacheRenderMeshUpdateContext updateContext;
			updateContext.m_prevPositions.resize(meshData.m_numVertices, Vec3(0.0f, 0.0f, 0.0f));
			updateContext.m_meshId = i;
			m_renderMeshUpdateContexts.push_back(updateContext);
			pRenderMesh = NULL;
		}

		m_renderMeshes.push_back(pRenderMesh);
	}

	return true;
}

_smart_ptr<IRenderMesh> CGeomCacheRenderNode::SetupDynamicRenderMesh(SGeomCacheRenderMeshUpdateContext& updateContext)
{
	FUNCTION_PROFILER_3DENGINE;

	const std::vector<SGeomCacheStaticMeshData>& staticMeshData = m_pGeomCache->GetStaticMeshData();
	const SGeomCacheStaticMeshData& meshData = staticMeshData[updateContext.m_meshId];

	// Create zero cleared render mesh
	const uint numMaterials = meshData.m_numIndices.size();
	uint numIndices = 0;
	for (uint i = 0; i < numMaterials; ++i)
	{
		numIndices += meshData.m_numIndices[i];
	}

	_smart_ptr<IRenderMesh> pRenderMesh = gEnv->pRenderer->CreateRenderMeshInitialized(NULL, meshData.m_numVertices,
	                                                                                   EDefaultInputLayouts::P3F_C4B_T2F, NULL, numIndices, prtTriangleList,
	                                                                                   "GeomCacheDynamicMesh", m_pGeomCache->GetFilePath(), eRMT_Dynamic);

	pRenderMesh->LockForThreadAccess();

	updateContext.m_pIndices = pRenderMesh->GetIndexPtr(FSL_VIDEO_CREATE);
	updateContext.m_pPositions.data = (Vec3*)pRenderMesh->GetPosPtrNoCache(updateContext.m_pPositions.iStride, FSL_VIDEO_CREATE);
	updateContext.m_pColors.data = (UCol*)pRenderMesh->GetColorPtr(updateContext.m_pColors.iStride, FSL_VIDEO_CREATE);
	updateContext.m_pTexcoords.data = (Vec2*)pRenderMesh->GetUVPtrNoCache(updateContext.m_pTexcoords.iStride, FSL_VIDEO_CREATE);
	updateContext.m_pTangents.data = (SPipTangents*)pRenderMesh->GetTangentPtr(updateContext.m_pTangents.iStride, FSL_VIDEO_CREATE);
	updateContext.m_pVelocities.data = (Vec3*)pRenderMesh->GetVelocityPtr(updateContext.m_pVelocities.iStride, FSL_VIDEO_CREATE);

	CRenderChunk chunk;
	chunk.nNumVerts = meshData.m_numVertices;
	uint32 currentIndexOffset = 0;

	std::vector<CRenderChunk> chunks;
	chunks.reserve(numMaterials);

	for (uint i = 0; i < numMaterials; ++i)
	{
		chunk.nFirstIndexId = currentIndexOffset;
		chunk.nNumIndices = meshData.m_numIndices[i];
		chunk.m_nMatID = meshData.m_materialIds[i];
		chunks.push_back(chunk);
		currentIndexOffset += chunk.nNumIndices;
	}

	pRenderMesh->SetRenderChunks(&chunks[0], numMaterials, false);

	updateContext.m_pUpdateState = pRenderMesh->SetAsyncUpdateState();
	pRenderMesh->UnLockForThreadAccess();

	return pRenderMesh;
}

void CGeomCacheRenderNode::SetStandIn(const char* pFilePath, const char* pMaterial)
{
	m_pStandIn = Get3DEngine()->LoadStatObj(pFilePath);

	if (m_pStandIn)
	{
		m_pStandIn->SetMaterial(GetMatMan()->LoadMaterial(pMaterial));
	}
}

void CGeomCacheRenderNode::SetFirstFrameStandIn(const char* pFilePath, const char* pMaterial)
{
	m_pFirstFrameStandIn = Get3DEngine()->LoadStatObj(pFilePath);

	if (m_pFirstFrameStandIn)
	{
		m_pFirstFrameStandIn->SetMaterial(GetMatMan()->LoadMaterial(pMaterial));
	}
}

void CGeomCacheRenderNode::SetLastFrameStandIn(const char* pFilePath, const char* pMaterial)
{
	m_pLastFrameStandIn = Get3DEngine()->LoadStatObj(pFilePath);

	if (m_pLastFrameStandIn)
	{
		m_pLastFrameStandIn->SetMaterial(GetMatMan()->LoadMaterial(pMaterial));
	}
}

void CGeomCacheRenderNode::SetStandInDistance(const float distance)
{
	m_standInDistance = distance;
}

void CGeomCacheRenderNode::SetStreamInDistance(const float distance)
{
	m_streamInDistance = distance;
}

CGeomCacheRenderNode::EStandInType CGeomCacheRenderNode::SelectStandIn() const
{
	const bool bFirstFrame = m_playbackTime == 0.0f;
	const bool bLastFrame = !m_bLooping && (m_pGeomCache ? (m_playbackTime >= m_pGeomCache->GetDuration()) : false);

	if (bFirstFrame && m_pFirstFrameStandIn && m_pFirstFrameStandIn->GetRenderMesh())
	{
		return eStandInType_FirstFrame;
	}
	else if (bLastFrame && m_pLastFrameStandIn && m_pLastFrameStandIn->GetRenderMesh())
	{
		return eStandInType_LastFrame;
	}
	else if (m_pStandIn && m_pStandIn->GetRenderMesh())
	{
		return eStandInType_Default;
	}

	return eStandInType_None;
}

IStatObj* CGeomCacheRenderNode::GetStandIn(const EStandInType type) const
{
	switch (type)
	{
	case eStandInType_Default:
		return m_pStandIn;
	case eStandInType_FirstFrame:
		return m_pFirstFrameStandIn;
	case eStandInType_LastFrame:
		return m_pLastFrameStandIn;
	}

	return NULL;
}

void CGeomCacheRenderNode::DebugDraw(const SGeometryDebugDrawInfo& info, uint nodeIndex) const
{
	CryAutoLock<CryCriticalSection> fillLock(m_fillCS);

	if (!m_bDrawing)
	{
		return;
	}

	switch (m_standInVisible)
	{
	case eStandInType_None:
		{
			if (m_pGeomCache && m_nodeMatrices.size() > 0)
			{
				const std::vector<SGeomCacheStaticNodeData>& staticNodeData = m_pGeomCache->GetStaticNodeData();
				nodeIndex = std::min(nodeIndex, (uint)(staticNodeData.size() - 1));
				DebugDrawRec(info, nodeIndex, staticNodeData);
			}
			break;
		}
	case eStandInType_Default:
		{
			m_pStandIn->DebugDraw(info);
			break;
		}
	case eStandInType_FirstFrame:
		{
			m_pFirstFrameStandIn->DebugDraw(info);
			break;
		}
	case eStandInType_LastFrame:
		{
			m_pLastFrameStandIn->DebugDraw(info);
			break;
		}
	}
}

void CGeomCacheRenderNode::DebugDrawRec(const SGeometryDebugDrawInfo& info,
                                        uint& currentNodeIndex, const std::vector<SGeomCacheStaticNodeData>& staticNodeData) const
{
	const SGeomCacheStaticNodeData& currentNodeData = staticNodeData[currentNodeIndex];

	if (currentNodeData.m_type == GeomCacheFile::eNodeType_Mesh)
	{
		for (TRenderElementMap::const_iterator iter = m_pRenderElements.begin(); iter != m_pRenderElements.end(); ++iter)
		{
			CREGeomCache* pCREGeomCache = iter->second.m_pRenderElement;
			DynArray<CREGeomCache::SMeshRenderData>* pFillData = pCREGeomCache->GetRenderDataPtr();

			if (pFillData)
			{
				CREGeomCache::SMeshRenderData& renderData = (*pFillData)[currentNodeData.m_meshOrGeometryIndex];
				IRenderMesh* pRenderMesh = renderData.m_pRenderMesh.get();

				if (renderData.m_instances.size() > 0 && pRenderMesh)
				{
					Matrix34 pieceMatrix = m_matrix * m_nodeMatrices[currentNodeIndex];

					SGeometryDebugDrawInfo subInfo = info;
					subInfo.tm = pieceMatrix;

					pRenderMesh->DebugDraw(subInfo, ~0);
					break;
				}
			}
		}
	}

	currentNodeIndex += 1;

	const uint32 numChildren = currentNodeData.m_numChildren;
	for (uint32 i = 0; i < numChildren; ++i)
	{
		DebugDrawRec(info, currentNodeIndex, staticNodeData);
	}
}

bool CGeomCacheRenderNode::RayIntersectionRec(SRayHitInfo& hitInfo, IMaterial* pCustomMtl, uint* pHitNodeIndex,
                                              uint& currentNodeIndex, const std::vector<SGeomCacheStaticNodeData>& staticNodeData, SRayHitInfo& hitOut, float& fMinDistance) const
{
	const SGeomCacheStaticNodeData& currentNodeData = staticNodeData[currentNodeIndex];

	bool bHit = false;

	if (currentNodeData.m_type == GeomCacheFile::eNodeType_Mesh)
	{
		for (TRenderElementMap::const_iterator iter = m_pRenderElements.begin(); iter != m_pRenderElements.end(); ++iter)
		{
			CREGeomCache* pCREGeomCache = iter->second.m_pRenderElement;
			DynArray<CREGeomCache::SMeshRenderData>* pFillData = pCREGeomCache->GetRenderDataPtr();

			if (pFillData)
			{
				CREGeomCache::SMeshRenderData& renderData = (*pFillData)[currentNodeData.m_meshOrGeometryIndex];
				IRenderMesh* pRenderMesh = renderData.m_pRenderMesh.get();

				if (renderData.m_instances.size() > 0 && pRenderMesh)
				{
					Matrix34 pieceMatrix = m_matrix * m_nodeMatrices[currentNodeIndex];

					AABB meshAABB = m_pGeomCache->GetStaticMeshData()[currentNodeData.m_meshOrGeometryIndex].m_aabb;

					AABB pieceWorldAABB;
					pieceWorldAABB.SetTransformedAABB(pieceMatrix, meshAABB);

					Vec3 vOut;
					if (!Intersect::Ray_AABB(hitInfo.inRay, pieceWorldAABB, vOut))
					{
						continue;
					}

					Matrix34 invPieceMatrix = pieceMatrix.GetInverted();

					// Transform ray into sub-object local space.
					SRayHitInfo subHitInfo = hitInfo;
					ZeroStruct(subHitInfo);
					subHitInfo.inReferencePoint = invPieceMatrix.TransformPoint(hitInfo.inReferencePoint);
					subHitInfo.inRay.origin = invPieceMatrix.TransformPoint(hitInfo.inRay.origin);
					subHitInfo.inRay.direction = invPieceMatrix.TransformVector(hitInfo.inRay.direction);

					if (CRenderMeshUtils::RayIntersection(pRenderMesh, subHitInfo, NULL))
					{
						const uint materialId = iter->first;
						IMaterial* pMaterial = const_cast<CGeomCacheRenderNode*>(this)->GetMaterial(NULL);
						IMaterial* pSubMaterial = pMaterial->GetSafeSubMtl(materialId);

						if (subHitInfo.nHitMatID == materialId)
						{
							subHitInfo.vHitPos = pieceMatrix.TransformPoint(subHitInfo.vHitPos);
							subHitInfo.fDistance = hitInfo.inReferencePoint.GetDistance(subHitInfo.vHitPos);

							if (subHitInfo.fDistance < fMinDistance)
							{
								bHit = true;
								fMinDistance = subHitInfo.fDistance;
								hitOut = subHitInfo;

								hitOut.nHitMatID = materialId;
								if (pSubMaterial)
								{
									hitInfo.nHitSurfaceID = pSubMaterial->GetSurfaceTypeId();
								}

								if (pHitNodeIndex)
								{
									*pHitNodeIndex = currentNodeIndex;
								}
							}
						}
					}
				}
			}
		}
	}

	currentNodeIndex += 1;

	const uint32 numChildren = currentNodeData.m_numChildren;
	for (uint32 i = 0; i < numChildren; ++i)
	{
		bHit = RayIntersectionRec(hitInfo, pCustomMtl, pHitNodeIndex, currentNodeIndex, staticNodeData, hitOut, fMinDistance) || bHit;
	}

	return bHit;
}

	#ifndef _RELEASE
void CGeomCacheRenderNode::DebugRender()
{
	if (m_pGeomCache && GetCVars()->e_GeomCacheDebugDrawMode == 3)
	{
		const std::vector<SGeomCacheStaticNodeData>& staticNodeData = m_pGeomCache->GetStaticNodeData();
		uint currentNodeIndex = 0;
		InstancingDebugDrawRec(currentNodeIndex, staticNodeData);
	}
}

void CGeomCacheRenderNode::InstancingDebugDrawRec(uint& currentNodeIndex, const std::vector<SGeomCacheStaticNodeData>& staticNodeData)
{
	CryAutoLock<CryCriticalSection> fillLock(m_fillCS);

	const SGeomCacheStaticNodeData& currentNodeData = staticNodeData[currentNodeIndex];

	ColorF colors[] = {
		Col_Aquamarine,      Col_Blue,              Col_BlueViolet,      Col_Brown,        Col_CadetBlue,      Col_Coral,           Col_CornflowerBlue,    Col_Cyan,
		Col_DimGrey,         Col_FireBrick,         Col_ForestGreen,     Col_Gold,         Col_Goldenrod,      Col_Gray,            Col_Green,             Col_GreenYellow,Col_IndianRed,         Col_Khaki,
		Col_LightBlue,       Col_LightGray,         Col_LightSteelBlue,  Col_LightWood,    Col_LimeGreen,      Col_Magenta,         Col_Maroon,            Col_MedianWood, Col_MediumAquamarine,
		Col_MediumBlue,      Col_MediumForestGreen, Col_MediumGoldenrod, Col_MediumOrchid, Col_MediumSeaGreen, Col_MediumSlateBlue, Col_MediumSpringGreen,
		Col_MediumTurquoise, Col_MediumVioletRed,   Col_MidnightBlue,    Col_Navy,         Col_NavyBlue,       Col_Orange,          Col_OrangeRed,         Col_Orchid,     Col_PaleGreen,
		Col_Pink,            Col_Plum,              Col_Red,             Col_Salmon,       Col_SeaGreen,       Col_Sienna,          Col_SkyBlue,           Col_SlateBlue,  Col_SpringGreen,       Col_SteelBlue,Col_Tan,
		Col_Thistle,         Col_Transparent,       Col_Turquoise,       Col_Violet,       Col_VioletRed,      Col_Wheat,           Col_Yellow
	};

	const uint kNumColors = sizeof(colors) / sizeof(ColorF);

	if (currentNodeData.m_type == GeomCacheFile::eNodeType_Mesh)
	{
		for (TRenderElementMap::const_iterator iter = m_pRenderElements.begin(); iter != m_pRenderElements.end(); ++iter)
		{
			CREGeomCache* pCREGeomCache = iter->second.m_pRenderElement;
			DynArray<CREGeomCache::SMeshRenderData>* pFillData = pCREGeomCache->GetRenderDataPtr();

			if (pFillData)
			{
				CREGeomCache::SMeshRenderData& renderData = (*pFillData)[currentNodeData.m_meshOrGeometryIndex];
				IRenderMesh* pRenderMesh = renderData.m_pRenderMesh.get();

				if (renderData.m_instances.size() > 0 && pRenderMesh)
				{
					Matrix34 pieceMatrix = m_matrix * m_nodeMatrices[currentNodeIndex];

					SGeometryDebugDrawInfo info;
					info.bNoLines = true;
					info.bDrawInFront = false;
					info.tm = pieceMatrix;

					const uint64 kMul = 0x9ddfea08eb382d69ULL;
					uint64 hash = (uint64)alias_cast<UINT_PTR>(pRenderMesh);
					hash ^= (hash >> 47);
					hash *= kMul;

					info.color = colors[hash % kNumColors];

					pRenderMesh->DebugDraw(info);
					break;
				}
			}
		}
	}

	currentNodeIndex += 1;

	const uint32 numChildren = currentNodeData.m_numChildren;
	for (uint32 i = 0; i < numChildren; ++i)
	{
		InstancingDebugDrawRec(currentNodeIndex, staticNodeData);
	}
}
	#endif

bool CGeomCacheRenderNode::RayIntersection(SRayHitInfo& hitInfo, IMaterial* pCustomMtl, uint* pNodeIndex) const
{
	CryAutoLock<CryCriticalSection> fillLock(m_fillCS);

	switch (m_standInVisible)
	{
	case eStandInType_None:
		{
			if (m_pGeomCache && m_nodeMatrices.size() > 0)
			{
				const std::vector<SGeomCacheStaticNodeData>& staticNodeData = m_pGeomCache->GetStaticNodeData();

				SRayHitInfo hitOut;
				float fMinDistance = std::numeric_limits<float>::max();
				uint currentNodeIndex = 0;

				if (RayIntersectionRec(hitInfo, pCustomMtl, pNodeIndex, currentNodeIndex, staticNodeData, hitOut, fMinDistance))
				{
					// Restore input ray/reference point.
					hitOut.inReferencePoint = hitInfo.inReferencePoint;
					hitOut.inRay = hitInfo.inRay;
					hitOut.fDistance = fMinDistance;

					hitInfo = hitOut;
					return true;
				}
			}
			break;
		}
	case eStandInType_Default:
		{
			return m_pStandIn->RayIntersection(hitInfo, pCustomMtl);
		}
	case eStandInType_FirstFrame:
		{
			return m_pFirstFrameStandIn->RayIntersection(hitInfo, pCustomMtl);
		}
	case eStandInType_LastFrame:
		{
			return m_pLastFrameStandIn->RayIntersection(hitInfo, pCustomMtl);
		}
	}

	return false;
}

uint CGeomCacheRenderNode::GetNodeCount() const
{
	if (!m_pGeomCache)
	{
		return 0;
	}

	const uint numNodes = m_pGeomCache->GetStaticNodeData().size();
	return numNodes;
}

Matrix34 CGeomCacheRenderNode::GetNodeTransform(const uint nodeIndex) const
{
	FUNCTION_PROFILER_3DENGINE;

	{
		CryAutoLock<CryMutex> lock(m_bTransformsReadyCS);
		while (!m_bTransformReady)
		{
			m_bTransformReadyCV.Wait(m_bTransformsReadyCS);
		}
	}

	if (nodeIndex >= m_nodeMatrices.size() || !m_pGeomCache)
	{
		return Matrix34(IDENTITY);
	}

	return m_nodeMatrices[nodeIndex];
}

const char* CGeomCacheRenderNode::GetNodeName(const uint nodeIndex) const
{
	if (!m_pGeomCache)
	{
		return "";
	}

	const std::vector<SGeomCacheStaticNodeData>& staticNodeData = m_pGeomCache->GetStaticNodeData();
	return staticNodeData[nodeIndex].m_name.c_str();
}

uint32 CGeomCacheRenderNode::GetNodeNameHash(const uint nodeIndex) const
{
	if (!m_pGeomCache)
	{
		return 0;
	}

	const std::vector<SGeomCacheStaticNodeData>& staticNodeData = m_pGeomCache->GetStaticNodeData();
	return staticNodeData[nodeIndex].m_nameHash;
}

bool CGeomCacheRenderNode::IsNodeDataValid(const uint nodeIndex) const
{
	FUNCTION_PROFILER_3DENGINE;

	{
		CryAutoLock<CryMutex> lock(m_bTransformsReadyCS);
		while (!m_bTransformReady)
		{
			m_bTransformReadyCV.Wait(m_bTransformsReadyCS);
		}
	}

	if (nodeIndex >= m_nodeMatrices.size() || !m_pGeomCache)
	{
		return false;
	}

	return true;
}

void CGeomCacheRenderNode::InitPhysicalEntity(IPhysicalEntity* pPhysicalEntity, const pe_articgeomparams& params)
{
	m_pPhysicalEntity = pPhysicalEntity;
	UpdatePhysicalEntity(&params);
}

void CGeomCacheRenderNode::UpdatePhysicalEntity(const pe_articgeomparams* pParams)
{
	if (!m_pPhysicalEntity)
	{
		return;
	}

	const std::vector<SGeomCacheStaticNodeData>& staticNodeData = m_pGeomCache->GetStaticNodeData();
	const std::vector<phys_geometry*>& physicsGeometries = m_pGeomCache->GetPhysicsGeometries();

	Matrix34 scaleMatrix = GetMatrix();
	const Vec3 scale = Vec3(scaleMatrix.GetColumn0().GetLength(), scaleMatrix.GetColumn1().GetLength(), scaleMatrix.GetColumn2().GetLength());
	scaleMatrix.SetScale(scale);

	const uint numNodes = staticNodeData.size();
	for (uint i = 0; i < numNodes; ++i)
	{
		const SGeomCacheStaticNodeData& nodeData = staticNodeData[i];
		if (nodeData.m_type == GeomCacheFile::eNodeType_PhysicsGeometry)
		{
			const Matrix34 nodeTransform = GetNodeTransform(i);
			phys_geometry* pGeometry = physicsGeometries[nodeData.m_meshOrGeometryIndex];
			if (pGeometry)
			{
				Matrix34 nodeMatrix = scaleMatrix * nodeTransform;

				if (pParams)
				{
					pe_articgeomparams params = *pParams;
					m_pPhysicalEntity->AddGeometry(pGeometry, &params, i);
				}

				pe_params_part params;
				params.pMtx3x4 = &nodeMatrix;
				params.partid = i;
				m_pPhysicalEntity->SetParams(&params);
			}
		}
	}
}

void CGeomCacheRenderNode::UpdatePhysicalMaterials()
{
	if (m_pPhysicalEntity && m_pMaterial)
	{
		int surfaceTypesId[MAX_SUB_MATERIALS] = { 0 };
		const int numIds = m_pMaterial->FillSurfaceTypeIds(surfaceTypesId);

		pe_params_part params;
		params.nMats = numIds;
		params.pMatMapping = surfaceTypesId;
		m_pPhysicalEntity->SetParams(&params);
	}
}

void CGeomCacheRenderNode::UpdateStreamableComponents(float fImportance, float fDistance, bool bFullUpdate, int nLod, const float fInvScale, bool bDrawNear)
{
	CObjManager* pObjManager = GetObjManager();
	Matrix34A matrix = GetMatrix();

	const bool bAllowStandIn = GetCVars()->e_Lods != 0;
	const bool bStreamInGeomCache = !m_pStandIn || (fDistance <= std::max(m_standInDistance, m_streamInDistance)) || !bAllowStandIn;
	if (m_pGeomCache && bStreamInGeomCache)
	{
		m_pGeomCache->UpdateStreamableComponents(fImportance, matrix, this, bFullUpdate);
	}

	static_cast<CMatInfo*>(m_pMaterial.get())->PrecacheMaterial(fDistance * fInvScale, NULL, bFullUpdate, bDrawNear);

	PrecacheStandIn(m_pStandIn, fImportance, fDistance, bFullUpdate, nLod, fInvScale, bDrawNear);
	PrecacheStandIn(m_pFirstFrameStandIn, fImportance, fDistance, bFullUpdate, nLod, fInvScale, bDrawNear);
	PrecacheStandIn(m_pLastFrameStandIn, fImportance, fDistance, bFullUpdate, nLod, fInvScale, bDrawNear);
}

void CGeomCacheRenderNode::PrecacheStandIn(IStatObj* pStandIn, float fImportance, float fDistance, bool bFullUpdate, int nLod, const float fInvScale, bool bDrawNear)
{
	if (pStandIn)
	{
		IStatObj* pLod = pStandIn->GetLodObject(nLod, true);
		if (pLod)
		{
			CObjManager* pObjManager = GetObjManager();
			Matrix34A matrix = GetMatrix();
			static_cast<CStatObj*>(pLod)->UpdateStreamableComponents(fImportance, matrix, bFullUpdate, nLod);
			pObjManager->PrecacheStatObjMaterial(pLod->GetMaterial(), fDistance * fInvScale, pLod, bFullUpdate, bDrawNear);
		}
	}
}

void CGeomCacheRenderNode::OnGeomCacheStaticDataLoaded()
{
	Initialize();
}

void CGeomCacheRenderNode::OnGeomCacheStaticDataUnloaded()
{
	Clear(false);
}

#endif
