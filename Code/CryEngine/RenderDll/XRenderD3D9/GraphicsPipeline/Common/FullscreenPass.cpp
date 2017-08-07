#include "StdAfx.h"
#include "FullscreenPass.h"
#include "DriverD3D.h"
#include "../Common/PostProcess/PostProcessUtils.h"

CFullscreenPass::CFullscreenPass(CRenderPrimitive::EPrimitiveFlags primitiveFlags)
: m_primitiveFlags(CRenderPrimitive::eFlags_None)
{
	m_inputVars[0] = m_inputVars[1] = m_inputVars[2] = m_inputVars[3] = 0;

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

void CFullscreenPass::UpdatePrimitive()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	// update viewport
	{
		CRY_ASSERT(m_renderPassDesc.GetRenderTargets()[0].pTexture || m_renderPassDesc.GetDepthTarget().pTexture);

		CTexture* pTarget             = m_renderPassDesc.GetRenderTargets()[0].pTexture ? m_renderPassDesc.GetRenderTargets()[0].pTexture   : m_renderPassDesc.GetDepthTarget().pTexture;
		ResourceViewHandle viewHandle = m_renderPassDesc.GetRenderTargets()[0].pTexture ? m_renderPassDesc.GetRenderTargets()[0].view : m_renderPassDesc.GetDepthTarget().view;
		int mip = 0;

		if (viewHandle != EDefaultResourceViews::RenderTarget && viewHandle != EDefaultResourceViews::DepthStencil)
		{
			auto& viewDesc = pTarget->GetDevTexture()->LookupResourceView(viewHandle).first;
			mip = viewDesc.m_Desc.nMostDetailedMip;
		}

		D3DViewPort viewport;

		viewport.TopLeftX = viewport.TopLeftY = 0;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		viewport.Width  = float(pTarget->GetWidthNonVirtual() >> mip);
		viewport.Height = float(pTarget->GetHeightNonVirtual() >> mip);

		SetViewport(viewport);

		// TODO: remove this call - Engine viewport needs to be set so that data is available when filling reflected PB constants
		rd->RT_SetViewport((int)viewport.TopLeftX, (int)viewport.TopLeftY, (int)viewport.Width, (int)viewport.Height);
	}
	
	if (m_bRequirePerViewCB)
	{
		if (!m_pPerViewConstantBuffer)
			m_pPerViewConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerViewGlobalConstantBuffer));
		
		CStandardGraphicsPipeline::SViewInfo viewInfo[2];
		int viewInfoCount = rd->GetGraphicsPipeline().GetViewInfo(viewInfo, &m_viewport);
		rd->GetGraphicsPipeline().UpdatePerViewConstantBuffer(viewInfo, viewInfoCount, m_pPerViewConstantBuffer);

		m_primitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, m_pPerViewConstantBuffer, EShaderStage_Vertex | EShaderStage_Pixel);
	}

	if (m_bRequireWorldPos)
	{
		if (m_vertexBuffer == ~0u)
		{
			m_vertexBuffer = gcpRendD3D->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_DYNAMIC, 3 * sizeof(SVF_P3F_T2F_T3F));
		}

		bool bUseQuad = CVrProjectionManager::Instance()->GetProjectionType() == CVrProjectionManager::eVrProjection_LensMatched;
		
		if (bUseQuad)
		{
			SVF_P3F_T2F_T3F fullscreenQuadWPOSVertices[4];
			SPostEffectsUtils::GetFullScreenQuadWPOS(fullscreenQuadWPOSVertices, 0, 0, m_clipZ);
			rd->m_DevBufMan.UpdateBuffer(m_vertexBuffer, fullscreenQuadWPOSVertices, 4 * sizeof(SVF_P3F_T2F_T3F));
		}
		else
		{
			SVF_P3F_T2F_T3F fullscreenTriWPOSVertices[3];
			SPostEffectsUtils::GetFullScreenTriWPOS(fullscreenTriWPOSVertices, 0, 0, m_clipZ);
			rd->m_DevBufMan.UpdateBuffer(m_vertexBuffer, fullscreenTriWPOSVertices, 3 * sizeof(SVF_P3F_T2F_T3F));
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
		m_primitive.GetConstantManager().EndNamedConstantUpdate();
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
