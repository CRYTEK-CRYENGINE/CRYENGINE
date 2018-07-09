// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FullscreenPass.h"
#include "DriverD3D.h"
#include "../Common/PostProcess/PostProcessUtils.h"

CFullscreenPass::CFullscreenPass(CRenderPrimitive::EPrimitiveFlags primitiveFlags)
: m_primitiveFlags(CRenderPrimitive::eFlags_None)
{
	m_bRequirePerViewCB = false;
	m_bRequireWorldPos = false;
	m_bPendingConstantUpdate = false;

	m_vertexBuffer = ~0u;

	m_clipZ = 0.0f;

	SetLabel("FULLSCREEN_PASS");
	SetPrimitiveFlags(primitiveFlags);
}

CFullscreenPass::~CFullscreenPass()
{
	if (m_vertexBuffer != ~0u)
	{
		gRenDev->m_DevBufMan.Destroy(m_vertexBuffer);
	}
}

void CFullscreenPass::BeginConstantUpdate()
{
	if (m_primitiveFlags & CRenderPrimitive::eFlags_ReflectShaderConstants)
	{
		UpdatePrimitive();
		m_primitive.GetConstantManager().BeginNamedConstantUpdate();

		m_bPendingConstantUpdate = true;
	}
}

static void GetFullScreenTriWPOS(const SRenderViewInfo& viewInfo, SVF_P3F_T2F_T3F pResult[3], int nTexWidth, int nTexHeight, float z, const D3DViewPort* pSrcRegion)
{
	Vec3 vLT = viewInfo.m_frustumCorners[SRenderViewInfo::eFrustum_LT];
	Vec3 vLB = viewInfo.m_frustumCorners[SRenderViewInfo::eFrustum_LB];
	Vec3 vRT = viewInfo.m_frustumCorners[SRenderViewInfo::eFrustum_RT];
	Vec3 vRB = viewInfo.m_frustumCorners[SRenderViewInfo::eFrustum_RB];

	z = 1.0f - z;

	pResult[0].p = Vec3(-0.0f, -0.0f, z);
	pResult[0].st0 = Vec2(0, 0);
	pResult[0].st1 = vLT;
	pResult[1].p = Vec3(-0.0f, 2.0f, z);
	pResult[1].st0 = Vec2(0, 2);
	pResult[1].st1 = vLB * 2.0f - vLT;
	pResult[2].p = Vec3(2.0f, -0.0f, z);
	pResult[2].st0 = Vec2(2, 0);
	pResult[2].st1 = vRT * 2.0f - vLT;

	if (pSrcRegion)
	{
		const Vec4 vTexCoordsRegion(
			2.0f * float(pSrcRegion->TopLeftX) / nTexWidth,
			2.0f * float(pSrcRegion->TopLeftX + pSrcRegion->Width) / nTexWidth,
			2.0f * float(pSrcRegion->TopLeftY) / nTexHeight,
			2.0f * float(pSrcRegion->TopLeftY + pSrcRegion->Height) / nTexHeight);

		pResult[0].st0 = Vec2(vTexCoordsRegion.x, vTexCoordsRegion.z);
		pResult[1].st0 = Vec2(vTexCoordsRegion.x, vTexCoordsRegion.w);
		pResult[2].st0 = Vec2(vTexCoordsRegion.y, vTexCoordsRegion.z);
	}
}

static void GetFullScreenQuadWPOS(const SRenderViewInfo& viewInfo, SVF_P3F_T2F_T3F pResult[4], int nTexWidth, int nTexHeight, float z, const D3DViewPort* pSrcRegion)
{
	Vec3 vLT = viewInfo.m_frustumCorners[SRenderViewInfo::eFrustum_LT];
	Vec3 vLB = viewInfo.m_frustumCorners[SRenderViewInfo::eFrustum_LB];
	Vec3 vRT = viewInfo.m_frustumCorners[SRenderViewInfo::eFrustum_RT];
	Vec3 vRB = viewInfo.m_frustumCorners[SRenderViewInfo::eFrustum_RB];

	z = 1.0f - z;

	pResult[0].p = Vec3(-0.0f, -0.0f, z);
	pResult[0].st0 = Vec2(0, 0);
	pResult[0].st1 = vLT;
	pResult[1].p = Vec3(-0.0f, 1.0f, z);
	pResult[1].st0 = Vec2(0, 1);
	pResult[1].st1 = vLB;
	pResult[2].p = Vec3(1.0f, -0.0f, z);
	pResult[2].st0 = Vec2(1, 0);
	pResult[2].st1 = vRT;
	pResult[3].p = Vec3(1.0f, 1.0f, z);
	pResult[3].st0 = Vec2(1, 1);
	pResult[3].st1 = vRB;

	if (pSrcRegion)
	{
		const Vec4 vTexCoordsRegion(
			float(pSrcRegion->TopLeftX) / nTexWidth,
			float(pSrcRegion->TopLeftX + pSrcRegion->Width) / nTexWidth,
			float(pSrcRegion->TopLeftY) / nTexHeight,
			float(pSrcRegion->TopLeftY + pSrcRegion->Height) / nTexHeight);

		pResult[0].st0 = Vec2(vTexCoordsRegion.x, vTexCoordsRegion.z);
		pResult[1].st0 = Vec2(vTexCoordsRegion.x, vTexCoordsRegion.w);
		pResult[2].st0 = Vec2(vTexCoordsRegion.y, vTexCoordsRegion.z);
		pResult[3].st0 = Vec2(vTexCoordsRegion.y, vTexCoordsRegion.w);
	}
}

void CFullscreenPass::UpdatePrimitive()
{
	// update viewport
	int nRenderTargetWidth  = 0;
	int nRenderTargetHeight = 0;
	{
		CRY_ASSERT(m_renderPassDesc.GetRenderTargets()[0].pTexture || m_renderPassDesc.GetDepthTarget().pTexture);

		CTexture* pTarget             = m_renderPassDesc.GetRenderTargets()[0].pTexture ? m_renderPassDesc.GetRenderTargets()[0].pTexture : m_renderPassDesc.GetDepthTarget().pTexture;
		ResourceViewHandle viewHandle = m_renderPassDesc.GetRenderTargets()[0].pTexture ? m_renderPassDesc.GetRenderTargets()[0].view     : m_renderPassDesc.GetDepthTarget().view;
		int mip = 0;

		PREFAST_SUPPRESS_WARNING(6287)
		if (viewHandle != EDefaultResourceViews::RenderTarget && viewHandle != EDefaultResourceViews::DepthStencil)
		{
			auto& viewDesc = pTarget->GetDevTexture()->LookupResourceView(viewHandle).first;
			mip = viewDesc.m_Desc.nMostDetailedMip;
		}

		nRenderTargetWidth  = pTarget->GetWidth () >> mip;
		nRenderTargetHeight = pTarget->GetHeight() >> mip;

		if (!m_bHasCustomViewport)
		{
			// Default to full-screen viewport
			D3DViewPort viewport;
			viewport.TopLeftX = viewport.TopLeftY = 0;
			viewport.Width = float(nRenderTargetWidth);
			viewport.Height = float(nRenderTargetHeight);
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;

			SetViewport(viewport);
		}
	}
	
	if (m_bRequirePerViewCB)
	{
		const SRenderViewInfo& viewInfo = gcpRendD3D.GetGraphicsPipeline().GetCurrentViewInfo(CCamera::eEye_Left);
		int viewInfoCount = gcpRendD3D.GetGraphicsPipeline().GetViewInfoCount();

		if (!m_pPerViewConstantBuffer)
			m_pPerViewConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerViewGlobalConstantBuffer));
		
		SRenderViewport customViewport(0,0,nRenderTargetWidth,nRenderTargetHeight);
		gcpRendD3D.GetGraphicsPipeline().GeneratePerViewConstantBuffer(&viewInfo, viewInfoCount, m_pPerViewConstantBuffer,&customViewport);

		m_primitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, m_pPerViewConstantBuffer, EShaderStage_Vertex | EShaderStage_Pixel);
	}

	if (m_bRequireWorldPos)
	{
		if (m_vertexBuffer == ~0u)
		{
			m_vertexBuffer = gcpRendD3D->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_DYNAMIC, 3 * sizeof(SVF_P3F_T2F_T3F));
		}

		bool bUseQuad = CVrProjectionManager::Instance()->GetProjectionType() == CVrProjectionManager::eVrProjection_LensMatched;
		const SRenderViewInfo& viewInfo = gcpRendD3D.GetGraphicsPipeline().GetCurrentViewInfo(CCamera::eEye_Left);
		
		if (bUseQuad)
		{
			SVF_P3F_T2F_T3F fullscreenQuadWPOSVertices[4];
			GetFullScreenQuadWPOS(viewInfo, fullscreenQuadWPOSVertices, nRenderTargetWidth, nRenderTargetHeight, m_clipZ, &GetViewport());
			CDeviceBufferManager::Instance()->UpdateBuffer(m_vertexBuffer, fullscreenQuadWPOSVertices, 4 * sizeof(SVF_P3F_T2F_T3F));
		}
		else
		{
			SVF_P3F_T2F_T3F fullscreenTriWPOSVertices[3];
			GetFullScreenTriWPOS(viewInfo, fullscreenTriWPOSVertices, nRenderTargetWidth, nRenderTargetHeight, m_clipZ, &GetViewport());
			CDeviceBufferManager::Instance()->UpdateBuffer(m_vertexBuffer, fullscreenTriWPOSVertices, 3 * sizeof(SVF_P3F_T2F_T3F));
		}

		m_primitive.SetCustomVertexStream(m_vertexBuffer, EDefaultInputLayouts::P3F_T2F_T3F, sizeof(SVF_P3F_T2F_T3F));
		m_primitive.SetCustomIndexStream(~0u, (RenderIndexType)0);

		if(bUseQuad)
			m_primitive.SetDrawInfo(eptTriangleStrip, 0, 0, 4);
		else
			m_primitive.SetDrawInfo(eptTriangleList, 0, 0, 3);
	}

	if (m_primitive.IsDirty())
	{
		BeginAddingPrimitives();

		m_primitive.Compile(*this);
		AddPrimitive(&m_primitive);
	}
}

bool CFullscreenPass::Execute()
{
	if (m_bPendingConstantUpdate)
	{
		// Unmap constant buffers
		m_primitive.GetConstantManager().EndNamedConstantUpdate(&GetViewport());
		m_bPendingConstantUpdate = false;
	}
	else
	{
		UpdatePrimitive();
	}

	bool success = false;
	Compile();

	if (!m_compiledPrimitives.empty())
	{
		CRY_ASSERT(!m_primitive.IsDirty()); // Primitive modified AFTER call to BeginConstantUpdate
		CPrimitiveRenderPass::Execute();

		success = true;
	}

	return success;
}

void CFullscreenPass::SetCustomViewport(const SRenderViewport& viewport)
{
	SetCustomViewport(RenderViewportToD3D11Viewport(viewport));
}
