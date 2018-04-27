// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlareSoftOcclusionQuery.h"
#include "../CryNameR.h"
#include "../../XRenderD3D9/DriverD3D.h"
#include "../Textures/Texture.h"
#include <Cry3DEngine/I3DEngine.h>

unsigned char CFlareSoftOcclusionQuery::s_paletteRawCache[s_nIDMax * 4];
char CFlareSoftOcclusionQuery::s_idHashTable[s_nIDMax];
int CFlareSoftOcclusionQuery::s_idCount = 0;
int CFlareSoftOcclusionQuery::s_ringReadIdx = 1;
int CFlareSoftOcclusionQuery::s_ringWriteIdx = 0;
int CFlareSoftOcclusionQuery::s_ringSize = MAX_OCCLUSION_READBACK_TEXTURES;

static bool g_bCreatedGlobalResources = false;

const float CFlareSoftOcclusionQuery::s_fSectorWidth = (float)s_nGatherEachSectorWidth / (float)s_nGatherTextureWidth;
const float CFlareSoftOcclusionQuery::s_fSectorHeight = (float)s_nGatherEachSectorHeight / (float)s_nGatherTextureHeight;

float CSoftOcclusionVisiblityFader::UpdateVisibility(const float newTargetVisibility, const float duration)
{
	m_TimeLine.duration = duration;

	if (fabs(newTargetVisibility - m_fTargetVisibility) > 0.1f)
	{
		m_TimeLine.startValue = m_TimeLine.GetPrevYValue();
		m_fTargetVisibility = newTargetVisibility;
		m_TimeLine.rewind();
	}
	else if (newTargetVisibility < 0.05f)
	{
		m_fTargetVisibility = newTargetVisibility;
		m_TimeLine.endValue = newTargetVisibility;
	}
	else
	{
		m_TimeLine.endValue = m_fTargetVisibility;
	}

	// Fade in fixed time (in ms), fade out user controlled
	const float fadeInTime = m_TimeLine.duration * (1.0f / 0.15f);
	const float fadeOutTime = 1e3f;
	float dt = (newTargetVisibility > m_fVisibilityFactor) ? fadeInTime : fadeOutTime;

	m_fVisibilityFactor = m_TimeLine.step(gEnv->pTimer->GetFrameTime() * dt); // interpolate

	return m_fVisibilityFactor;
}

void ReportIDPoolOverflow(int idCount, int idMax)
{
	iLog->Log("Number of soft occlusion queries [%d] exceeds current allowed range %d", idCount, idMax);
}

int CFlareSoftOcclusionQuery::GenID()
{
	int hashCode = s_idCount % s_nIDMax;

	while (hashCode < s_nIDMax && s_idHashTable[hashCode])
		hashCode++;

	if (hashCode >= s_nIDMax)
	{
		ReportIDPoolOverflow(s_idCount + 1, s_nIDMax);
		hashCode = 0;
	}

	s_idHashTable[hashCode] = (char)1;
	s_idCount++;

	return hashCode;
}

void CFlareSoftOcclusionQuery::ReleaseID(int id)
{
	if (id < s_nIDMax)
		s_idHashTable[id] = (char)0;
}

void CFlareSoftOcclusionQuery::InitGlobalResources()
{
	if (g_bCreatedGlobalResources)
		return;

	const uint32 numGPUs = gRenDev->GetActiveGPUCount();
	s_ringWriteIdx = 0;
	s_ringReadIdx  = numGPUs;
	s_ringSize     = numGPUs * MAX_FRAMES_IN_FLIGHT;

	memset(s_idHashTable, 0, sizeof(s_idHashTable));
	memset(s_paletteRawCache, 0, sizeof(s_paletteRawCache));

	g_bCreatedGlobalResources = true;
}

void CFlareSoftOcclusionQuery::GetDomainInTexture(float& out_x0, float& out_y0, float& out_x1, float& out_y1)
{
	out_x0 = (m_nID % s_nIDColMax) * s_fSectorWidth;
	out_y0 = (m_nID / s_nIDColMax) * s_fSectorHeight;
	out_x1 = out_x0 + s_fSectorWidth;
	out_y1 = out_y0 + s_fSectorHeight;
}

bool CFlareSoftOcclusionQuery::ComputeProjPos(const Vec3& vWorldPos, const Matrix44A& viewMat, const Matrix44A& projMat, Vec3& outProjPos)
{
	static int vp[] = { 0, 0, 1, 1 };
	if (mathVec3Project(&outProjPos, &vWorldPos, vp, &projMat, &viewMat, &gRenDev->m_IdentityMatrix) == 0.0f)
		return false;
	return true;
}

void CFlareSoftOcclusionQuery::GetSectorSize(float& width, float& height)
{
	width = s_fSectorWidth;
	height = s_fSectorWidth;
}

void CFlareSoftOcclusionQuery::GetOcclusionSectorInfo(SOcclusionSectorInfo& out_Info,const SRenderViewInfo& viewInfo)
{
	Vec3 vProjectedPos;
	if (ComputeProjPos(m_PosToBeChecked, viewInfo.viewMatrix, viewInfo.projMatrix, vProjectedPos) == false)
		return;

	out_Info.lineardepth = clamp_tpl(ComputeLinearDepth(m_PosToBeChecked, viewInfo.viewMatrix, viewInfo.nearClipPlane, viewInfo.farClipPlane), -1.0f, 0.99f);

	out_Info.u0 = vProjectedPos.x - m_fOccPlaneWidth / 2;
	out_Info.v0 = vProjectedPos.y - m_fOccPlaneHeight / 2;
	out_Info.u1 = vProjectedPos.x + m_fOccPlaneWidth / 2;
	out_Info.v1 = vProjectedPos.y + m_fOccPlaneHeight / 2;

	if (out_Info.u0 < 0)   out_Info.u0 = 0;
	if (out_Info.v0 < 0)   out_Info.v0 = 0;
	if (out_Info.u1 > 1)   out_Info.u1 = 1;
	if (out_Info.v1 > 1)   out_Info.v1 = 1;

	GetDomainInTexture(out_Info.x0, out_Info.y0, out_Info.x1, out_Info.y1);

	out_Info.x0 = out_Info.x0 * 2.0f - 1;
	out_Info.y0 = -(out_Info.y0 * 2.0f - 1.0f);
	out_Info.x1 = out_Info.x1 * 2.0f - 1;
	out_Info.y1 = -(out_Info.y1 * 2.0f - 1.0f);
}

float CFlareSoftOcclusionQuery::ComputeLinearDepth(const Vec3& worldPos, const Matrix44A& cameraMat, float nearDist, float farDist)
{
	Vec4 out = Vec4(worldPos, 1) * cameraMat;
	if (out.w == 0.0f)
		return 0;

	float linearDepth = (-out.z - nearDist) / (farDist - nearDist);

	return linearDepth;
}

void CFlareSoftOcclusionQuery::UpdateCachedResults()
{
	int cacheIdx = 4 * m_nID;
	m_fOccResultCache = s_paletteRawCache[cacheIdx + 0] / 255.0f;
	m_fDirResultCache = (s_paletteRawCache[cacheIdx + 3] / 255.0f) * 2.0f * PI;
	sincos_tpl(m_fDirResultCache, &m_DirVecResultCache.y, &m_DirVecResultCache.x);
}

CTexture* CFlareSoftOcclusionQuery::GetGatherTexture() const
{
	return CRendererResources::s_ptexFlaresGather;
}

void CFlareSoftOcclusionQuery::BatchReadResults()
{
	if (!g_bCreatedGlobalResources)
		return;

	CRY_PROFILE_REGION_WAITING(PROFILE_RENDERER, "CFlareSoftOcclusionQuery::BatchReadResults");

	CRendererResources::s_ptexFlaresOcclusionRing[s_ringReadIdx]->GetDevTexture()->AccessCurrStagingResource(0, false, [=](void* pData, uint32 rowPitch, uint32 slicePitch)
	{
		unsigned char* pTexBuf = reinterpret_cast<unsigned char*>(pData);
		int validLineStrideBytes = s_nIDColMax * 4;
		for (int i = 0; i < s_nIDRowMax; i++)
		{
		  memcpy(s_paletteRawCache + i * validLineStrideBytes, pTexBuf + i * rowPitch, validLineStrideBytes);
		}

		return true;
	});
}

void CFlareSoftOcclusionQuery::ReadbackSoftOcclQuery()
{
	CRendererResources::s_ptexFlaresOcclusionRing[s_ringWriteIdx]->GetDevTexture()->DownloadToStagingResource(0);

	// sync point. Move to next texture to read and write
	s_ringReadIdx  = (s_ringReadIdx  + 1) % s_ringSize;
	s_ringWriteIdx = (s_ringWriteIdx + 1) % s_ringSize;
}

CTexture* CFlareSoftOcclusionQuery::GetOcclusionTex()
{
	return CRendererResources::s_ptexFlaresOcclusionRing[s_ringWriteIdx];
}

CSoftOcclusionManager::CSoftOcclusionManager()
	: m_nPos(0)
	, m_indexBuffer(~0u)
	, m_occlusionVertexBuffer(0u)
	, m_gatherVertexBuffer(0u)
{}

CSoftOcclusionManager::~CSoftOcclusionManager()
{
	if (m_indexBuffer != ~0u)           gcpRendD3D->m_DevBufMan.Destroy(m_indexBuffer);
	if (m_occlusionVertexBuffer != ~0u) gcpRendD3D->m_DevBufMan.Destroy(m_occlusionVertexBuffer);
	if (m_gatherVertexBuffer != ~0u)    gcpRendD3D->m_DevBufMan.Destroy(m_gatherVertexBuffer);
}

bool CSoftOcclusionManager::PrepareOcclusionPrimitive(CRenderPrimitive& primitive, const CPrimitiveRenderPass& targetPass,const SRenderViewInfo& viewInfo)
{
	if (m_indexBuffer == ~0u || m_occlusionVertexBuffer == ~0u)
		return false;

	const uint32 vertexCount = GetSize() * 4;
	const uint32 indexCount = GetSize() * 3 * 2;

	// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
	CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, vertexCount, pDeviceVBAddr, CDeviceBufferManager::AlignBufferSizeForStreaming);

	for (int i(0), iSoftOcclusionListSize(GetSize()); i < iSoftOcclusionListSize; ++i)
	{
		CFlareSoftOcclusionQuery* pSoftOcclusion = GetSoftOcclusionQuery(i);
		if (pSoftOcclusion == NULL)
			continue;
		int offset = i * 4;

		CFlareSoftOcclusionQuery::SOcclusionSectorInfo sInfo;
		pSoftOcclusion->GetOcclusionSectorInfo(sInfo,viewInfo);

		for (int k = 0; k < 4; ++k)
			pDeviceVBAddr[offset + k].color.dcolor = 0xFFFFFFFF;

		pDeviceVBAddr[offset + 0].st = Vec2(sInfo.u0 * gcpRendD3D->m_CurViewportScale.x, sInfo.v0 * gcpRendD3D->m_CurViewportScale.y);
		pDeviceVBAddr[offset + 1].st = Vec2(sInfo.u1 * gcpRendD3D->m_CurViewportScale.x, sInfo.v0 * gcpRendD3D->m_CurViewportScale.y);
		pDeviceVBAddr[offset + 2].st = Vec2(sInfo.u1 * gcpRendD3D->m_CurViewportScale.x, sInfo.v1 * gcpRendD3D->m_CurViewportScale.y);
		pDeviceVBAddr[offset + 3].st = Vec2(sInfo.u0 * gcpRendD3D->m_CurViewportScale.x, sInfo.v1 * gcpRendD3D->m_CurViewportScale.y);

		pDeviceVBAddr[offset + 0].xyz = Vec3(sInfo.x0, sInfo.y1, sInfo.lineardepth);
		pDeviceVBAddr[offset + 1].xyz = Vec3(sInfo.x1, sInfo.y1, sInfo.lineardepth);
		pDeviceVBAddr[offset + 2].xyz = Vec3(sInfo.x1, sInfo.y0, sInfo.lineardepth);
		pDeviceVBAddr[offset + 3].xyz = Vec3(sInfo.x0, sInfo.y0, sInfo.lineardepth);
	}

	gcpRendD3D->m_DevBufMan.UpdateBuffer(m_occlusionVertexBuffer, pDeviceVBAddr, vertexCount * sizeof(SVF_P3F_C4B_T2F));

	static CCryNameTSCRC techRenderPlane("RenderPlane");

	primitive.SetTechnique(CShaderMan::s_ShaderSoftOcclusionQuery, techRenderPlane, 0);
	primitive.SetRenderState(GS_NODEPTHTEST);
	primitive.SetTexture(0, CRendererResources::s_ptexLinearDepthScaled[0]);
	primitive.SetSampler(0, EDefaultSamplerStates::PointBorder_Black);
	primitive.SetPrimitiveType(CRenderPrimitive::ePrim_Triangle);
	primitive.SetCustomIndexStream(m_indexBuffer, Index16);
	primitive.SetCustomVertexStream(m_occlusionVertexBuffer, EDefaultInputLayouts::P3F_C4B_T2F, sizeof(SVF_P3F_C4B_T2F));
	primitive.SetDrawInfo(eptTriangleList, 0, 0, indexCount);
	primitive.Compile(targetPass);

	return true;
}

void CSoftOcclusionManager::Init()
{
	const int maxIndexCount = CFlareSoftOcclusionQuery::s_nIDMax * 2 * 3;
	const int maxVertexCount = CFlareSoftOcclusionQuery::s_nIDMax * 4;

	m_indexBuffer           = gcpRendD3D->m_DevBufMan.Create(BBT_INDEX_BUFFER, BU_STATIC,   maxIndexCount  * sizeof(uint16));
	m_occlusionVertexBuffer = gcpRendD3D->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_DYNAMIC, maxVertexCount * sizeof(SVF_P3F_C4B_T2F));
	m_gatherVertexBuffer    = gcpRendD3D->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_DYNAMIC, maxVertexCount * sizeof(SVF_P3F_C4B_T2F));

	if (m_indexBuffer != ~0u)
	{
		CryStackAllocWithSizeVector(uint16, maxIndexCount, pDeviceIBAddr, CDeviceBufferManager::AlignBufferSizeForStreaming);

		for (int i = 0; i < CFlareSoftOcclusionQuery::s_nIDMax; ++i)
		{
			int offset0 = i * 6;
			int offset1 = i * 4;
			pDeviceIBAddr[offset0 + 0] = offset1 + 0;
			pDeviceIBAddr[offset0 + 1] = offset1 + 1;
			pDeviceIBAddr[offset0 + 2] = offset1 + 2;
			pDeviceIBAddr[offset0 + 3] = offset1 + 2;
			pDeviceIBAddr[offset0 + 4] = offset1 + 3;
			pDeviceIBAddr[offset0 + 5] = offset1 + 0;
		}

		gcpRendD3D->m_DevBufMan.UpdateBuffer(m_indexBuffer, pDeviceIBAddr, maxIndexCount * sizeof(uint16));
	}
}

bool CSoftOcclusionManager::PrepareGatherPrimitive(CRenderPrimitive& primitive, const CPrimitiveRenderPass& targetPass, SRenderViewInfo* pViewInfo, int viewInfoCount)
{
	CRY_ASSERT(viewInfoCount >= 0);

	if (m_indexBuffer == ~0u || m_gatherVertexBuffer == ~0u)
		return false;

	const uint32 vertexCount = GetSize() * 4;
	const uint32 indexCount  = GetSize() * 3 * 2;
	float x0 = 0, y0 = 0, x1 = 0, y1 = 0;

	// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
	CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, vertexCount, pDeviceVBAddr, CDeviceBufferManager::AlignBufferSizeForStreaming);

	for (int i = 0, iSoftOcclusionListSize(GetSize()); i < iSoftOcclusionListSize; ++i)
	{
		CFlareSoftOcclusionQuery* pSoftOcclusion = GetSoftOcclusionQuery(i);
		if (pSoftOcclusion == NULL)
			continue;

		int offset = i * 4;
		pSoftOcclusion->GetDomainInTexture(x0, y0, x1, y1);
		for (int k = 0; k < 4; ++k)
		{
			pDeviceVBAddr[offset + k].st = Vec2((x0 + x1) * 0.5f * gcpRendD3D->m_CurViewportScale.x, (y0 + y1) * 0.5f * gcpRendD3D->m_CurViewportScale.y);
			pDeviceVBAddr[offset + k].color.dcolor = 0xFFFFFFFF;
		}

		x0 = (x0 * 2.0f - 1.0f);
		y0 = -(y0 * 2.0f - 1.0f);
		x1 = (x1 * 2.0f - 1.0f);
		y1 = -(y1 * 2.0f - 1.0f);

		pDeviceVBAddr[offset + 0].xyz = Vec3(x0, y1, 1);
		pDeviceVBAddr[offset + 1].xyz = Vec3(x1, y1, 1);
		pDeviceVBAddr[offset + 2].xyz = Vec3(x1, y0, 1);
		pDeviceVBAddr[offset + 3].xyz = Vec3(x0, y0, 1);
	}

	gcpRendD3D->m_DevBufMan.UpdateBuffer(m_gatherVertexBuffer, pDeviceVBAddr, vertexCount * sizeof(SVF_P3F_C4B_T2F));

	static CCryNameTSCRC techGatherPlane("Gather");
	primitive.SetFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
	primitive.SetTechnique(CShaderMan::s_ShaderSoftOcclusionQuery, techGatherPlane, 0);
	primitive.SetRenderState(GS_NODEPTHTEST);
	primitive.SetTexture(0, CRendererResources::s_ptexFlaresGather);
	primitive.SetSampler(0, EDefaultSamplerStates::PointClamp);
	primitive.SetPrimitiveType(CRenderPrimitive::ePrim_Triangle);
	primitive.SetCustomIndexStream(m_indexBuffer, Index16);
	primitive.SetCustomVertexStream(m_gatherVertexBuffer, EDefaultInputLayouts::P3F_C4B_T2F, sizeof(SVF_P3F_C4B_T2F));
	primitive.SetDrawInfo(eptTriangleList, 0, 0, indexCount);
	primitive.Compile(targetPass);

	auto& constantManager = primitive.GetConstantManager();
	constantManager.BeginNamedConstantUpdate();
	{
		static CCryNameR occlusionParamsName("occlusionParams");
		const Vec4 occlusionSizeParams(
			CFlareSoftOcclusionQuery::s_fSectorWidth, 
			CFlareSoftOcclusionQuery::s_fSectorHeight,
			pViewInfo[0].downscaleFactor.x,
			pViewInfo[0].downscaleFactor.y);

		constantManager.SetNamedConstant(occlusionParamsName, occlusionSizeParams);
	}
	constantManager.EndNamedConstantUpdate(&targetPass.GetViewport());

	return true;
}

CFlareSoftOcclusionQuery* CSoftOcclusionManager::GetSoftOcclusionQuery(int nIndex) const
{
	if (nIndex >= CFlareSoftOcclusionQuery::s_nIDMax)
		return NULL;
	return m_SoftOcclusionQueries[nIndex];
}

void CSoftOcclusionManager::AddSoftOcclusionQuery(CFlareSoftOcclusionQuery* pQuery, const Vec3& vPos)
{
	if (m_nPos < CFlareSoftOcclusionQuery::s_nIDMax)
	{
		pQuery->SetPosToBeChecked(vPos);
		m_SoftOcclusionQueries[m_nPos++] = pQuery;
	}
}

void CSoftOcclusionManager::Reset()
{
	m_nPos = 0;
}

bool CSoftOcclusionManager::Update(SRenderViewInfo* pViewInfo, int viewInfoCount)
{
	if (GetSize() > 0)
	{
		CTexture* pOcclusionRT = CRendererResources::s_ptexFlaresGather;
		CTexture* pGatherRT = CFlareSoftOcclusionQuery::GetOcclusionTex();

		CClearSurfacePass::Execute(pOcclusionRT, Clr_Transparent);
		CClearSurfacePass::Execute(pGatherRT, Clr_Transparent);

		// occlusion pass
		{
			D3DViewPort viewport;
			viewport.TopLeftX = viewport.TopLeftY = 0.0f;
			viewport.Width = (float)pOcclusionRT->GetWidth();
			viewport.Height = (float)pOcclusionRT->GetHeight();
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;

			m_occlusionPass.SetRenderTarget(0, pOcclusionRT);
			m_occlusionPass.SetViewport(viewport);
			m_occlusionPass.BeginAddingPrimitives();

			if (PrepareOcclusionPrimitive(m_occlusionPrimitive, m_occlusionPass,*pViewInfo))
			{
				m_occlusionPass.AddPrimitive(&m_occlusionPrimitive);
				m_occlusionPass.Execute();
			}
		}

		// Gather pass
		{
			D3DViewPort viewport;
			viewport.TopLeftX = viewport.TopLeftY = 0.0f;
			viewport.Width = (float)pGatherRT->GetWidth();
			viewport.Height = (float)pGatherRT->GetHeight();
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;

			m_gatherPass.SetRenderTarget(0, pGatherRT);
			m_gatherPass.SetViewport(viewport);
			m_gatherPass.BeginAddingPrimitives();

			if (PrepareGatherPrimitive(m_gatherPrimitive, m_gatherPass, pViewInfo, viewInfoCount))
			{
				m_gatherPass.AddPrimitive(&m_gatherPrimitive);
				m_gatherPass.Execute();
			}
		}
	}

	return m_occlusionPass.GetPrimitiveCount() > 0 && m_gatherPass.GetPrimitiveCount() > 0;
}
