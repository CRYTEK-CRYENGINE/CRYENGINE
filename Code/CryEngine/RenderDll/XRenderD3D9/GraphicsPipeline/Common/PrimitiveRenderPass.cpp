// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryCore/CryCustomTypes.h>
#include "DriverD3D.h"
#include "../Common/PostProcess/PostProcessUtils.h"
#include <CryRenderer/RenderElements/RendElement.h>
#include <Common/RendElements/MeshUtil.h>

CRenderPrimitive::SPrimitiveGeometry CRenderPrimitive::s_primitiveGeometryCache[CRenderPrimitive::ePrim_Count];
int CRenderPrimitive::s_nPrimitiveGeometryCacheUsers = 0;

CGraphicsPipeline& CRenderPassBase::GetGraphicsPipeline() const
{
	return gRenDev->GetGraphicsPipeline();
}

SCompiledRenderPrimitive::SCompiledRenderPrimitive(SCompiledRenderPrimitive&& other)
	: m_pPipelineState(std::move(other.m_pPipelineState))
	, m_pResourceLayout(std::move(other.m_pResourceLayout))
	, m_pResources(std::move(other.m_pResources))
	, m_pVertexInputSet(std::move(other.m_pVertexInputSet))
	, m_pIndexInputSet(std::move(other.m_pIndexInputSet))
	, m_inlineConstantBuffers(std::move(other.m_inlineConstantBuffers))
	, m_stencilRef(std::move(other.m_stencilRef))
	, m_drawInfo(std::move(other.m_drawInfo))
{
}

void SCompiledRenderPrimitive::Reset()
{
	m_stencilRef = 0;
	m_pVertexInputSet = nullptr;
	m_pIndexInputSet = nullptr;

	m_pResources.reset();
	m_drawInfo = SDrawInfo();

	m_pPipelineState.reset();
	m_pResourceLayout.reset();

	m_inlineConstantBuffers.fill(SDeviceObjectHelpers::SConstantBufferBindInfo());
}

CRenderPrimitive::SPrimitiveGeometry::SPrimitiveGeometry()
	: primType(eptTriangleList)
	, vertexFormat(InputLayoutHandle::Unspecified)
	, vertexBaseOffset(0)
	, vertexOrIndexCount(0)
	, vertexOrIndexOffset(0)
	, indexStream()
	, vertexStream()
{
}

CRenderPrimitive::CRenderPrimitive(CRenderPrimitive&& other)
	: SCompiledRenderPrimitive(std::move(other))
	, m_flags(std::move(other.m_flags))
	, m_dirtyMask(other.m_dirtyMask)
	, m_renderState(std::move(other.m_renderState))
	, m_stencilState(std::move(other.m_stencilState))
	, m_stencilReadMask(std::move(other.m_stencilReadMask))
	, m_stencilWriteMask(std::move(other.m_stencilWriteMask))
	, m_cullMode(std::move(other.m_cullMode))
	, m_pShader(std::move(other.m_pShader))
	, m_techniqueName(std::move(other.m_techniqueName))
	, m_rtMask(std::move(other.m_rtMask))
	, m_primitiveType(std::move(other.m_primitiveType))
	, m_primitiveGeometry(std::move(other.m_primitiveGeometry))
	, m_resourceDesc(other.m_resourceDesc)
	, m_constantManager(std::move(other.m_constantManager))
	, m_currentPsoUpdateCount(std::move(other.m_currentPsoUpdateCount))
	, m_renderPassHash(std::move(other.m_renderPassHash))
	, m_bDepthClip(std::move(other.m_bDepthClip))
{}

CRenderPrimitive::CRenderPrimitive(EPrimitiveFlags flags)
	: SCompiledRenderPrimitive(SCompiledRenderPrimitive::eType_RenderPrimitive)
	, m_flags(flags)
	, m_dirtyMask(eDirty_All)
	, m_renderState(0)
	, m_stencilState(STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_KEEP))
	, m_stencilReadMask(0xFF)
	, m_stencilWriteMask(0xFF)
	, m_cullMode(eCULL_Back)
	, m_pShader(nullptr)
	, m_rtMask(0)
	, m_primitiveType(ePrim_Triangle)
	, m_currentPsoUpdateCount(0)
	, m_renderPassHash(0)
	, m_bDepthClip(true)
	, m_resourceDesc()
{
}

void CRenderPrimitive::Reset(EPrimitiveFlags flags)
{
	SCompiledRenderPrimitive::Reset();

	m_flags = flags;
	m_dirtyMask = eDirty_All;
	m_renderState = 0;
	m_stencilState = STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_KEEP);
	m_stencilReadMask = 0xFF;
	m_stencilWriteMask = 0xFF;
	m_cullMode = eCULL_Back;
	m_bDepthClip = true;
	m_pShader = nullptr;
	m_techniqueName.reset();
	m_rtMask = 0;
	m_primitiveType = ePrim_Triangle;
	m_primitiveGeometry = SPrimitiveGeometry();
	m_resourceDesc.ClearResources();
	m_constantManager.Reset();
	m_currentPsoUpdateCount = 0;
	m_renderPassHash = 0;
}

void CRenderPrimitive::AllocateTypedConstantBuffer(EConstantBufferShaderSlot shaderSlot, int size, EShaderStage shaderStages)
{
	SetInlineConstantBuffer(shaderSlot, gcpRendD3D->m_DevBufMan.CreateConstantBuffer(size), shaderStages);
}

bool CRenderPrimitive::IsDirty() const
{
	// Merge local and remote dirty flags to test for changes
	EDirtyFlags dirtyMask = m_dirtyMask | (EDirtyFlags)m_resourceDesc.GetDirtyFlags();

	if (dirtyMask != eDirty_None)
		return true;

	if (m_currentPsoUpdateCount != m_pPipelineState->GetUpdateCount())
		return true;

	return false;
}

CRenderPrimitive::EDirtyFlags CRenderPrimitive::Compile(const CPrimitiveRenderPass& targetPass)
{
	CRY_ASSERT_MESSAGE(targetPass.GetRenderPass() && targetPass.GetRenderPass()->IsValid(), 
		"Target pass needs to have a valid renderpass for compilation. Call CPrimitiveRenderPass::BeginAddingPrimitives first");

	// Merge local and remote dirty flags to test for changes
	EDirtyFlags dirtyMask = m_dirtyMask | (EDirtyFlags)m_resourceDesc.GetDirtyFlags();

	// TODO: This comparison detects a change even when CRenderPrimitivePass's format doesn't change. It's excessive for dx12.
	const uint64 targetPassHash = targetPass.GetRenderPass()->GetHash();
	if (targetPassHash != m_renderPassHash)
	{
		dirtyMask |=  eDirty_RenderPass;
	}

	if ((dirtyMask != eDirty_None) || (m_currentPsoUpdateCount != m_pPipelineState->GetUpdateCount()))
	{
		EDirtyFlags revertMask = dirtyMask;

		CD3D9Renderer* const __restrict rd = gcpRendD3D;

		if (dirtyMask & eDirty_Geometry)
		{
			dirtyMask |= eDirty_InstanceData;

			if (m_primitiveType != ePrim_Custom)
			{
				CRenderPrimitive::EPrimitiveType primitiveType = m_primitiveType;
				if (CVrProjectionManager::Instance()->GetProjectionType() == CVrProjectionManager::eVrProjection_LensMatched && primitiveType == ePrim_Triangle)
					primitiveType = ePrim_FullscreenQuad;

				m_primitiveGeometry = s_primitiveGeometryCache[primitiveType];
			}

			m_pIndexInputSet = GetDeviceObjectFactory().CreateIndexStreamSet(&m_primitiveGeometry.indexStream);
			m_pVertexInputSet = GetDeviceObjectFactory().CreateVertexStreamSet(1, &m_primitiveGeometry.vertexStream);

			dirtyMask &= ~eDirty_Geometry;
		}

		if (dirtyMask & eDirty_InstanceData)
		{
			m_drawInfo.vertexBaseOffset = m_primitiveGeometry.vertexBaseOffset;
			m_drawInfo.vertexOrIndexCount = m_primitiveGeometry.vertexOrIndexCount;
			m_drawInfo.vertexOrIndexOffset = m_primitiveGeometry.vertexOrIndexOffset;
		}

		if (dirtyMask & eDirty_Resources)
		{
			if (!CDeviceResourceSet::UpdateWithReevaluation(m_pResources, m_resourceDesc))
				return (EDirtyFlags)(m_dirtyMask |= revertMask);
		}

		if (dirtyMask & (eDirty_Technique | eDirty_ResourceLayout))
		{
			m_constantManager.ReleaseShaderReflection();

			if (m_flags & eFlags_ReflectShaderConstants_Mask)
			{
				EShaderStage stages = EShaderStage_None;
				stages |= (m_flags & eFlags_ReflectShaderConstants_VS) ? EShaderStage_Vertex   : EShaderStage_None;
				stages |= (m_flags & eFlags_ReflectShaderConstants_PS) ? EShaderStage_Pixel    : EShaderStage_None;
				stages |= (m_flags & eFlags_ReflectShaderConstants_GS) ? EShaderStage_Geometry : EShaderStage_None;
				m_constantManager.AllocateShaderReflection(m_pShader, m_techniqueName, m_rtMask, stages);
			}

			int bindSlot = 0;
			SDeviceResourceLayoutDesc resourceLayoutDesc;

			if (!targetPass.GetOutputResources().IsEmpty())
			{
				resourceLayoutDesc.SetResourceSet(bindSlot++, targetPass.GetOutputResources());
			}

			if (!m_resourceDesc.IsEmpty())
			{
				resourceLayoutDesc.SetResourceSet(bindSlot++, m_resourceDesc);
			}

			for (auto& cb : m_constantManager.GetBuffers())
				resourceLayoutDesc.SetConstantBuffer(bindSlot++, cb.shaderSlot, cb.shaderStages);

			m_pResourceLayout = GetDeviceObjectFactory().CreateResourceLayout(resourceLayoutDesc);

			if (!m_pResourceLayout)
				return (EDirtyFlags)(m_dirtyMask |= revertMask);
		}

		if (dirtyMask & (eDirty_Technique | eDirty_RenderState | eDirty_ResourceLayout | eDirty_RenderPass | eDirty_Topology))
		{
			CDeviceGraphicsPSODesc psoDesc;

			psoDesc.m_pResourceLayout = m_pResourceLayout;
			psoDesc.m_pShader = m_pShader;
			psoDesc.m_technique = m_techniqueName;
			psoDesc.m_ShaderFlags_RT = m_rtMask;
			psoDesc.m_ShaderFlags_MD = 0;
			psoDesc.m_ShaderFlags_MDV = 0;
			psoDesc.m_PrimitiveType = m_primitiveGeometry.primType;
			psoDesc.m_VertexFormat = m_primitiveGeometry.vertexFormat;
			psoDesc.m_RenderState = m_renderState;
			psoDesc.m_StencilState = m_stencilState;
			psoDesc.m_StencilReadMask = m_stencilReadMask;
			psoDesc.m_StencilWriteMask = m_stencilWriteMask;
			psoDesc.m_CullMode = m_cullMode;
			psoDesc.m_bDepthClip = m_bDepthClip;
			psoDesc.m_pRenderPass = targetPass.GetRenderPass();
			
			m_pPipelineState = GetDeviceObjectFactory().CreateGraphicsPSO(psoDesc);
			if (!m_pPipelineState || !m_pPipelineState->IsValid())
				return (EDirtyFlags)(m_dirtyMask |= revertMask);

			m_currentPsoUpdateCount = m_pPipelineState->GetUpdateCount();

			m_renderPassHash = targetPass.GetRenderPass()->GetHash();

			if (m_flags & eFlags_ReflectShaderConstants_Mask)
				m_constantManager.InitShaderReflection(*m_pPipelineState);

			const auto& cBuffers = m_constantManager.GetBuffers();
			assert(cBuffers.size() <= m_inlineConstantBuffers.max_size());
			size_t sz = std::min(m_inlineConstantBuffers.max_size(),cBuffers.size());
			std::copy_n(cBuffers.begin(), sz, m_inlineConstantBuffers.begin());
			std::fill(m_inlineConstantBuffers.begin() + sz, m_inlineConstantBuffers.end(), SDeviceObjectHelpers::SConstantBufferBindInfo());
		}

		m_dirtyMask = dirtyMask = eDirty_None;
	}

	return dirtyMask;
}

void CRenderPrimitive::AddPrimitiveGeometryCacheUser()
{
	if (s_nPrimitiveGeometryCacheUsers == 0)
	{
		CD3D9Renderer* const __restrict rd = gcpRendD3D;

		// ePrim_Triangle
		{
			SPrimitiveGeometry& primitiveGeometry = s_primitiveGeometryCache[ePrim_Triangle];

			// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
			CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 3, fullscreenTriVertices, CDeviceBufferManager::AlignBufferSizeForStreaming);

			SPostEffectsUtils::GetFullScreenTri(fullscreenTriVertices, 0, 0, 1.0f);
			primitiveGeometry.vertexStream.hStream = gcpRendD3D->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_DYNAMIC, 3 * sizeof(SVF_P3F_C4B_T2F));
			primitiveGeometry.vertexStream.nStride = sizeof(SVF_P3F_C4B_T2F);
			primitiveGeometry.primType = eptTriangleList;
			primitiveGeometry.vertexFormat = EDefaultInputLayouts::P3F_C4B_T2F;
			primitiveGeometry.vertexOrIndexOffset = 0;
			primitiveGeometry.vertexOrIndexCount = 3;

			gcpRendD3D->m_DevBufMan.UpdateBuffer(primitiveGeometry.vertexStream.hStream, fullscreenTriVertices, fullscreenTriVerticesSize);
		}

		// ePrim_ProceduralTriangle
		{
			SPrimitiveGeometry& primitiveGeometry = s_primitiveGeometryCache[ePrim_ProceduralTriangle];

			primitiveGeometry.vertexStream.hStream = ~0u;
			primitiveGeometry.vertexStream.nStride = 0;
			primitiveGeometry.primType = eptTriangleList;
			primitiveGeometry.vertexFormat = EDefaultInputLayouts::Empty;
			primitiveGeometry.vertexOrIndexOffset = 0;
			primitiveGeometry.vertexOrIndexCount = 3;
		}

		// ePrim_ProceduralQuad
		{
			SPrimitiveGeometry& primitiveGeometry = s_primitiveGeometryCache[ePrim_ProceduralQuad];

			primitiveGeometry.vertexStream.hStream = ~0u;
			primitiveGeometry.vertexStream.nStride = 0;
			primitiveGeometry.primType = eptTriangleStrip;
			primitiveGeometry.vertexFormat = EDefaultInputLayouts::Empty;
			primitiveGeometry.vertexOrIndexOffset = 0;
			primitiveGeometry.vertexOrIndexCount = 4;
		}

		// ePrim_FullscreenQuad
		{
			SPrimitiveGeometry& primitiveGeometry = s_primitiveGeometryCache[ePrim_FullscreenQuad];

			// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
			CryStackAllocWithSizeVectorCleared(SVF_P3F_C4B_T2F, 4, fullscreenQuadVertices, CDeviceBufferManager::AlignBufferSizeForStreaming);

			SPostEffectsUtils::GetFullScreenQuad(fullscreenQuadVertices, 0, 0, 1.0f);
			primitiveGeometry.vertexStream.hStream = gcpRendD3D->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_DYNAMIC, 4 * sizeof(SVF_P3F_C4B_T2F));
			primitiveGeometry.vertexStream.nStride = sizeof(SVF_P3F_C4B_T2F);
			primitiveGeometry.primType = eptTriangleStrip;
			primitiveGeometry.vertexFormat = EDefaultInputLayouts::P3F_C4B_T2F;
			primitiveGeometry.vertexOrIndexOffset = 0;
			primitiveGeometry.vertexOrIndexCount = 4;

			gcpRendD3D->m_DevBufMan.UpdateBuffer(primitiveGeometry.vertexStream.hStream, fullscreenQuadVertices, fullscreenQuadVerticesSize);
		}

		t_arrDeferredMeshVertBuff vertices;
		t_arrDeferredMeshIndBuff indices;

		auto initPrimitiveGeometryStreams = [](SPrimitiveGeometry& primitiveGeometry, t_arrDeferredMeshVertBuff boxVertices, t_arrDeferredMeshIndBuff boxIndices)
		{
			primitiveGeometry.vertexStream.hStream = gcpRendD3D->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_STATIC, boxVertices.size() * sizeof(boxVertices[0]));
			primitiveGeometry.vertexStream.nStride = sizeof(boxVertices[0]);
			primitiveGeometry.indexStream.hStream = gcpRendD3D->m_DevBufMan.Create(BBT_INDEX_BUFFER, BU_STATIC, boxIndices.size() * sizeof(boxIndices[0]));
			primitiveGeometry.indexStream.nStride = Index16;
			primitiveGeometry.primType = eptTriangleList;
			primitiveGeometry.vertexFormat = EDefaultInputLayouts::P3F_C4B_T2F;
			primitiveGeometry.vertexOrIndexOffset = 0;
			primitiveGeometry.vertexOrIndexCount = boxIndices.size();

			gcpRendD3D->m_DevBufMan.UpdateBuffer(primitiveGeometry.vertexStream.hStream, boxVertices);
			gcpRendD3D->m_DevBufMan.UpdateBuffer(primitiveGeometry.indexStream.hStream, boxIndices);
		};

		// Box Primitives
		{
			CDeferredRenderUtils::CreateUnitBox(indices, vertices);
			initPrimitiveGeometryStreams(s_primitiveGeometryCache[ePrim_UnitBox], vertices, indices);

			CDeferredRenderUtils::CreateSimpleLightFrustumMesh(indices, vertices);
			initPrimitiveGeometryStreams(s_primitiveGeometryCache[ePrim_CenteredBox], vertices, indices);
		}

		// Projector Primitives
		{
			const uint nProjectorMeshStep = 10;

			for (int i = ePrim_Projector; i <= ePrim_Projector2; ++i)
			{
				const uint nFrustTess = 11 + nProjectorMeshStep * (i - ePrim_Projector);

				CDeferredRenderUtils::CreateUnitFrustumMesh(nFrustTess, nFrustTess, indices, vertices);
				initPrimitiveGeometryStreams(s_primitiveGeometryCache[i], vertices, indices);
			}

			for (int i = ePrim_ClipProjector; i <= ePrim_ClipProjector2; ++i)
			{
				uint nFrustTess = 41 + nProjectorMeshStep * (i - ePrim_ClipProjector);

				CDeferredRenderUtils::CreateUnitFrustumMesh(nFrustTess, nFrustTess, indices, vertices);
				initPrimitiveGeometryStreams(s_primitiveGeometryCache[i], vertices, indices);
			}
		}

		// fullscreen quad
		{
			MeshUtil::GenScreenTile(0, 0, 1, 1, ColorF(1, 1, 1, 1), 1, 1, vertices, indices);
			initPrimitiveGeometryStreams(s_primitiveGeometryCache[ePrim_FullscreenQuad], vertices, indices);
		}

		// tessellated fullscreen centered
		{
			MeshUtil::GenScreenTile(-1, -1, 1, 1, ColorF(1, 1, 1, 1), 1, 1, vertices, indices);
			initPrimitiveGeometryStreams(s_primitiveGeometryCache[ePrim_FullscreenQuadCentered], vertices, indices);
		}

		// tessellated fullscreen quad
		{
			const int rowCount = 15;
			const int colCount = 25;

			MeshUtil::GenScreenTile(-1, -1, 1, 1, ColorF(1, 1, 1, 1), rowCount, colCount, vertices, indices);
			initPrimitiveGeometryStreams(s_primitiveGeometryCache[ePrim_FullscreenQuadTess], vertices, indices);
		}
	}

	++s_nPrimitiveGeometryCacheUsers;
}

void CRenderPrimitive::RemovePrimitiveGeometryCacheUser()
{
	if (--s_nPrimitiveGeometryCacheUsers == 0)
	{
		for (EPrimitiveType primType = ePrim_First; primType != ePrim_Count; primType = EPrimitiveType(primType + 1))
		{
			if (s_primitiveGeometryCache[primType].vertexStream.hStream != ~0u) gRenDev->m_DevBufMan.Destroy(s_primitiveGeometryCache[primType].vertexStream.hStream);
			if (s_primitiveGeometryCache[primType].indexStream.hStream != ~0u) gRenDev->m_DevBufMan.Destroy(s_primitiveGeometryCache[primType].indexStream.hStream);

			s_primitiveGeometryCache[primType] = SPrimitiveGeometry();
		}
	}
}

CPrimitiveRenderPass::CPrimitiveRenderPass(bool createGeometryCache)
	: m_scissorEnabled(false)
	, m_bAddingPrimitives(false)
	, m_passFlags(ePassFlags_None)
	, m_clearMask(0)
	, m_outputResources()
	, m_outputNULLResources()
	, m_renderPassDesc()
{
	ZeroStruct(m_viewport);
	ZeroStruct(m_scissor);

	if( createGeometryCache )
	{
		CRenderPrimitive::AddPrimitiveGeometryCacheUser();
	}

	SetLabel("PRIMITIVE_PASS");
}

void CPrimitiveRenderPass::Reset()
{
	m_passFlags = ePassFlags_None;

	m_pRenderPass.reset();
	m_pOutputResourceSet.reset();
	m_pOutputNULLResourceSet.reset();

	m_renderPassDesc.ClearResources();
	m_outputResources.ClearResources();
	m_outputNULLResources.ClearResources();
	m_compiledPrimitives.clear();

	ZeroStruct(m_viewport);
	ZeroStruct(m_scissor);

	m_scissorEnabled = false;
	m_bAddingPrimitives = false;
	m_clearMask = 0;
}

CPrimitiveRenderPass::~CPrimitiveRenderPass()
{
	CRenderPrimitive::RemovePrimitiveGeometryCacheUser();
}

CPrimitiveRenderPass::CPrimitiveRenderPass(CPrimitiveRenderPass&& other)
	: CRenderPassBase(std::move(other))
	, m_scissorEnabled(std::move(other.m_scissorEnabled))
	, m_bAddingPrimitives(std::move(other.m_bAddingPrimitives))
	, m_passFlags(ePassFlags_None)
	, m_pRenderPass(std::move(other.m_pRenderPass))
	, m_renderPassDesc(std::move(other.m_renderPassDesc))
	, m_outputResources(std::move(other.m_outputResources))
	, m_outputNULLResources(std::move(other.m_outputNULLResources))
	, m_pOutputResourceSet(std::move(other.m_pOutputResourceSet))
	, m_pOutputNULLResourceSet(std::move(other.m_pOutputNULLResourceSet))
	, m_viewport(std::move(other.m_viewport))
	, m_scissor(std::move(other.m_scissor))
	, m_compiledPrimitives(std::move(other.m_compiledPrimitives))
{}

void CPrimitiveRenderPass::SetViewport(const D3DViewPort& viewport)
{
	if (!m_scissorEnabled)
	{
		m_scissor.left   = LONG(viewport.TopLeftX);
		m_scissor.top    = LONG(viewport.TopLeftY);
		m_scissor.right  = LONG(viewport.TopLeftX + viewport.Width);
		m_scissor.bottom = LONG(viewport.TopLeftY + viewport.Height);
	}

	m_viewport = viewport;
}

void CPrimitiveRenderPass::SetViewport(const SRenderViewport& viewport)
{
	SetViewport(RenderViewportToD3D11Viewport(viewport));
}

void CPrimitiveRenderPass::SetScissor(bool bEnable, const D3DRectangle& scissor)
{
	if (m_scissorEnabled = bEnable)
	{
		m_scissor = scissor;
	}
	else
	{
		m_scissor.left   = LONG(m_viewport.TopLeftX);
		m_scissor.top    = LONG(m_viewport.TopLeftY);
		m_scissor.right  = LONG(m_viewport.TopLeftX + m_viewport.Width);
		m_scissor.bottom = LONG(m_viewport.TopLeftY + m_viewport.Height);
	}
}

void CPrimitiveRenderPass::SetTargetClearMask(uint32 clearMask)
{
	m_clearMask = clearMask;
}

void CPrimitiveRenderPass::ClearPrimitives()
{
	m_compiledPrimitives.clear();
}

void CPrimitiveRenderPass::BeginAddingPrimitives(bool bClearPrimitiveList)
{
	Compile();

	if (bClearPrimitiveList)
	{
		ClearPrimitives();
	}

	m_bAddingPrimitives = true;
}


bool CPrimitiveRenderPass::AddPrimitive(CRenderPrimitive* pPrimitive)
{
	CRY_ASSERT(m_bAddingPrimitives); // Need to call 'BeginAddingPrimitives' first

	if (!pPrimitive->IsDirty())
	{
		m_compiledPrimitives.push_back(pPrimitive);
		return true;
	}

	return false;
}

bool CPrimitiveRenderPass::AddPrimitive(SCompiledRenderPrimitive* pPrimitive)
{
	CRY_ASSERT(m_bAddingPrimitives); // Need to call 'BeginAddingPrimitives' first
	CRY_ASSERT(pPrimitive->m_pPipelineState->IsValid());
	CRY_ASSERT(pPrimitive->m_pResourceLayout);
	CRY_ASSERT(!pPrimitive->m_pResources || pPrimitive->m_pResources->IsValid());

	m_compiledPrimitives.push_back(pPrimitive);
	return true;
}

void CPrimitiveRenderPass::Compile()
{
	bool bOutputsDirty = CDeviceRenderPass::UpdateWithReevaluation(m_pRenderPass, m_renderPassDesc);

	// update output resource set whenever something changed
	if (bOutputsDirty)
	{
		const auto& outputUAVs = m_renderPassDesc.GetOutputUAVs();
		if (outputUAVs[0].IsValid())
		{
			// TODO: respect dirty flags, currently a RT-change invalidates the UAV resource sets
			for (int i = 0; i < outputUAVs.size(); ++i)
			{
				if (!outputUAVs[i].IsValid())
					break;

				m_outputResources.SetBuffer(i, outputUAVs[i].pBuffer, EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
				m_outputNULLResources.SetBuffer(i, gcpRendD3D->m_DevBufMan.GetNullBufferTyped(), EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
			}

			if (m_outputResources.HasChanged())
			{
				CDeviceResourceSet::UpdateWithReevaluation(m_pOutputResourceSet, m_outputResources);
				CRY_ASSERT(m_pOutputResourceSet->IsValid());
			}

			if (m_outputNULLResources.HasChanged())
			{
				CDeviceResourceSet::UpdateWithReevaluation(m_pOutputNULLResourceSet, m_outputNULLResources);
				CRY_ASSERT(m_pOutputNULLResourceSet->IsValid());
			}
		}
	}
}

void CPrimitiveRenderPass::Prepare(CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	uint32 firstBindSlot = m_pOutputResourceSet ? 1 : 0;

	CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();

	pCommandInterface->PrepareRenderPassForUse(*m_pRenderPass.get());

	for (auto pPrimitive : m_compiledPrimitives)
	{
		uint32 bindSlot = firstBindSlot;

		CRY_ASSERT(pPrimitive->m_pPipelineState->IsValid());
		CRY_ASSERT(pPrimitive->m_pResourceLayout);
		CRY_ASSERT_MESSAGE(pPrimitive->m_type == SCompiledRenderPrimitive::eType_RenderPrimitive ? !static_cast<CRenderPrimitive*>(pPrimitive)->IsDirty() : true, "Render primitive of the current render pass is dirty!");

		if (pPrimitive->m_pResources)
		{
			CRY_ASSERT(pPrimitive->m_pResources->IsValid());
			pCommandInterface->PrepareResourcesForUse(bindSlot++, pPrimitive->m_pResources.get());
		}

		uint32 cbBindSlot = bindSlot;
		for (auto& cb : pPrimitive->m_inlineConstantBuffers)
		{
			if (cb.pBuffer)
				pCommandInterface->PrepareInlineConstantBufferForUse(cbBindSlot++, cb.pBuffer, cb.shaderSlot, cb.shaderStages);
		}

		if (pPrimitive->m_pVertexInputSet)
		{
			pCommandInterface->PrepareVertexBuffersForUse(1, 0, pPrimitive->m_pVertexInputSet);
		}

		if (pPrimitive->m_pIndexInputSet)
		{
			pCommandInterface->PrepareIndexBufferForUse(pPrimitive->m_pIndexInputSet);
		}
	}
}

void CPrimitiveRenderPass::Execute()
{
	m_bAddingPrimitives = false;

	if (m_compiledPrimitives.empty() || !m_pRenderPass || !m_pRenderPass->IsValid())
		return;

	if (gcpRendD3D->GetGraphicsPipeline().GetRenderPassScheduler().IsActive())
	{
		gcpRendD3D->GetGraphicsPipeline().GetRenderPassScheduler().AddPass(this);
		return;
	}

	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();
	CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();

#ifdef ENABLE_PROFILING_CODE
	commandList.BeginProfilingSection();
#endif

	if (m_clearMask & CPrimitiveRenderPass::eClear_Stencil)
	{
		CClearSurfacePass::Execute(GetDepthTarget(), CLEAR_STENCIL, Clr_Unused.r, Val_Stencil);
	}
	if (m_clearMask & CPrimitiveRenderPass::eClear_Color0)
	{
		CClearSurfacePass::Execute(GetRenderTarget(0), Clr_Transparent);
	}

	// prepare primitives first
	Prepare(commandList);

	pCommandInterface->BeginRenderPass(*m_pRenderPass, m_scissor);

	if (!CVrProjectionManager::Instance()->SetRenderingState(
		commandList,
		m_viewport,
		(m_passFlags & ePassFlags_UseVrProjectionState) != 0,
		(m_passFlags & ePassFlags_RequireVrProjectionConstants) != 0))
	{
		pCommandInterface->SetViewports(1, &m_viewport);
		pCommandInterface->SetScissorRects(1, &m_scissor);
	}
	
	for (auto pPrimitive : m_compiledPrimitives)
	{
		uint32 bindSlot = 0;
		pCommandInterface->SetResourceLayout(pPrimitive->m_pResourceLayout.get());
		pCommandInterface->SetPipelineState(pPrimitive->m_pPipelineState.get());
		pCommandInterface->SetStencilRef(pPrimitive->m_stencilRef);

		if (pPrimitive->m_pVertexInputSet)
		{
			pCommandInterface->SetVertexBuffers(1, 0, pPrimitive->m_pVertexInputSet);
		}

		if (pPrimitive->m_pIndexInputSet)
		{
			pCommandInterface->SetIndexBuffer(pPrimitive->m_pIndexInputSet);
		}

		if (m_pOutputResourceSet)
		{
			pCommandInterface->SetResources(bindSlot++, m_pOutputResourceSet.get());
		}

		if (pPrimitive->m_pResources)
		{
			pCommandInterface->SetResources(bindSlot++, pPrimitive->m_pResources.get());
		}

		for (auto& cb : pPrimitive->m_inlineConstantBuffers)
		{
			if (cb.pBuffer)
			{
				pCommandInterface->SetInlineConstantBuffer(bindSlot++, cb.pBuffer, cb.shaderSlot, cb.shaderStages);
			}
		}

		if (pPrimitive->m_pIndexInputSet)
		{
			pCommandInterface->DrawIndexed(pPrimitive->m_drawInfo.vertexOrIndexCount, 1, pPrimitive->m_drawInfo.vertexOrIndexOffset, pPrimitive->m_drawInfo.vertexBaseOffset, 0);
		}
		else
		{
			pCommandInterface->Draw(pPrimitive->m_drawInfo.vertexOrIndexCount, 1, pPrimitive->m_drawInfo.vertexOrIndexOffset, 0);
		}
	}

	if (m_passFlags & ePassFlags_UseVrProjectionState)
	{
		CVrProjectionManager::Instance()->RestoreState(commandList);
	}

	// unbind output resources
	if (m_pOutputNULLResourceSet)
	{
		pCommandInterface->SetResources(0, m_pOutputNULLResourceSet.get());
		gcpRendD3D->m_DevMan.CommitDeviceStates();
	}

	pCommandInterface->EndRenderPass(*m_pRenderPass);

#ifdef ENABLE_PROFILING_CODE
	rd->AddRecordedProfilingStats(commandList.EndProfilingSection(), EFSLIST_GENERAL, false);
#endif
}
