// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImageSpaceShafts.h"
#include "FlareSoftOcclusionQuery.h"
#include "../CryNameR.h"
#include "../../RenderDll/XRenderD3D9/DriverD3D.h"

CTexture* ImageSpaceShafts::m_pOccBuffer = NULL;
CTexture* ImageSpaceShafts::m_pDraftBuffer = NULL;

#if defined(FLARES_SUPPORT_EDITING)
	#define MFPtr(FUNC_NAME) (Optics_MFPtr)(&ImageSpaceShafts::FUNC_NAME)
void ImageSpaceShafts::InitEditorParamGroups(DynArray<FuncVariableGroup>& groups)
{
	COpticsElement::InitEditorParamGroups(groups);
	FuncVariableGroup isShaftsGroup;
	isShaftsGroup.SetName("ImageSpaceShafts", "Image Space Shafts");
	isShaftsGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "High Quality", "Enable High quality mode", this, MFPtr(SetHighQualityMode), MFPtr(IsHighQualityMode)));
	isShaftsGroup.AddVariable(new OpticsMFPVariable(e_TEXTURE2D, "Gobo Tex", "Gobo texture", this, MFPtr(SetGoboTex), MFPtr(GetGoboTex)));
	groups.push_back(isShaftsGroup);
}
	#undef MFPtr
#endif

ImageSpaceShafts::ImageSpaceShafts(const char* name)
	: COpticsElement(name)
	, m_bTexDirty(true)
	, m_bHighQualityMode(false)
{
	m_Color.a = 1.f;
	SetSize(0.7f);

	// share one constant buffer between both primitives
	CConstantBufferPtr pSharedCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SShaderParams));

	m_occlusionPrimitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pSharedCB, EShaderStage_Vertex | EShaderStage_Pixel);
	m_shaftGenPrimitive .SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pSharedCB, EShaderStage_Vertex | EShaderStage_Pixel);
	m_blendPrimitive    .SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pSharedCB, EShaderStage_Vertex | EShaderStage_Pixel);
}

void ImageSpaceShafts::Load(IXmlNode* pNode)
{
	COpticsElement::Load(pNode);

	XmlNodeRef pImageSpaceNode = pNode->findChild("ImageSpaceShafts");
	if (pImageSpaceNode)
	{
		bool bHighQualityMode(m_bHighQualityMode);
		if (pImageSpaceNode->getAttr("HighQuality", bHighQualityMode))
			SetHighQualityMode(bHighQualityMode);
		else
		{
			assert(0);
		}

		const char* pGoboTexName = NULL;
		if (pImageSpaceNode->getAttr("GoboTex", &pGoboTexName))
		{
			if (pGoboTexName && pGoboTexName[0])
			{
				ITexture* pTexture = std::move(gEnv->pRenderer->EF_LoadTexture(pGoboTexName));
				SetGoboTex((CTexture*)pTexture);
			}
		}
		else
		{
			assert(0);
		}
	}
	else
	{
		assert(0);
	}
}

void ImageSpaceShafts::InitTextures()
{
	float occBufRatio, occDraftRatio, occFinalRatio;
	if (m_bHighQualityMode && CRenderer::CV_r_flareHqShafts)
	{
		occBufRatio   = 0.5f;
		occDraftRatio = 0.5f;
		occFinalRatio = 0.5f;
	}
	else
	{
		occBufRatio   = 0.25f;
		occDraftRatio = 0.4f;
		occFinalRatio = 0.45f;
	}

	// TODO: this can impossibly be correct when the resolution changes (over time)
	const int w = CRendererResources::s_renderWidth;
	const int h = CRendererResources::s_renderHeight;
	const int flag = FT_DONT_RELEASE | FT_DONT_STREAM;
	const ETEX_Format draftTexFormat(eTF_R16G16B16A16);

	m_pOccBuffer   = CTexture::GetOrCreateRenderTarget("$ImageSpaceShaftsOccBuffer"  , (int)(w * occBufRatio  ), (int)(h * occBufRatio  ), ColorF(0.0f, 0.0f, 0.0f, 1.0f), eTT_2D, flag, eTF_R8G8B8A8);
	m_pDraftBuffer = CTexture::GetOrCreateRenderTarget("$ImageSpaceShaftsDraftBuffer", (int)(w * occDraftRatio), (int)(h * occDraftRatio), ColorF(0.0f, 0.0f, 0.0f, 1.0f), eTT_2D, flag, draftTexFormat);

	m_bTexDirty = false;
}

bool ImageSpaceShafts::PrepareOcclusion(CTexture* pDestRT, CTexture* pGoboTex, SamplerStateHandle samplerState)
{
	// prepare pass
	D3DViewPort viewport;

	viewport.TopLeftX =
	viewport.TopLeftY = 0.0f;
	viewport.Width  = (float)pDestRT->GetWidth();
	viewport.Height = (float)pDestRT->GetHeight();
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	CRY_ASSERT(CRendererResources::s_renderWidth  == viewport.Width);
	CRY_ASSERT(CRendererResources::s_renderHeight == viewport.Height);

	CClearSurfacePass::Execute(pDestRT, Clr_Empty);

	m_occlusionPass.SetRenderTarget(0, pDestRT);
	m_occlusionPass.SetViewport(viewport);
	m_occlusionPass.BeginAddingPrimitives();

	// prepare primitive
	static CCryNameTSCRC occlusionTech("ImageSpaceShaftsOcclusion");
	uint64 rtFlags = 0;
	ApplyGeneralFlags(rtFlags);

	m_occlusionPrimitive.SetTechnique(CShaderMan::s_ShaderLensOptics, occlusionTech, rtFlags);
	m_occlusionPrimitive.SetRenderState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);
	m_occlusionPrimitive.SetPrimitiveType(CRenderPrimitive::ePrim_FullscreenQuadCentered);
	m_occlusionPrimitive.SetTexture(0, pGoboTex ? pGoboTex : CRendererResources::s_ptexBlack);
	m_occlusionPrimitive.SetTexture(1, CRendererResources::s_ptexLinearDepthScaled[0]);
	m_occlusionPrimitive.SetSampler(0, samplerState);
	m_occlusionPrimitive.Compile(m_occlusionPass);

	m_occlusionPass.AddPrimitive(&m_occlusionPrimitive);

	return true;
}

bool ImageSpaceShafts::PrepareShaftGen(CTexture* pDestRT, CTexture* pOcclTex, SamplerStateHandle samplerState)
{
	// prepare pass
	D3DViewPort viewport;

	viewport.TopLeftX =
	viewport.TopLeftY = 0.0f;
	viewport.Width  = (float)pDestRT->GetWidth();
	viewport.Height = (float)pDestRT->GetHeight();
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	CRY_ASSERT(CRendererResources::s_renderWidth  == viewport.Width);
	CRY_ASSERT(CRendererResources::s_renderHeight == viewport.Height);

	CClearSurfacePass::Execute(pDestRT, Clr_Empty);

	m_shaftGenPass.SetRenderTarget(0, pDestRT);
	m_shaftGenPass.SetViewport(viewport);
	m_shaftGenPass.BeginAddingPrimitives();

	// prepare primitive
	static CCryNameTSCRC occlusionTech("ImageSpaceShaftsGen");

	uint64 rtFlags = 0;
	ApplyGeneralFlags(rtFlags);

	m_shaftGenPrimitive.SetTechnique(CShaderMan::s_ShaderLensOptics, occlusionTech, rtFlags);
	m_shaftGenPrimitive.SetRenderState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE_A_ZERO);
	m_shaftGenPrimitive.SetPrimitiveType(CRenderPrimitive::ePrim_FullscreenQuadCentered);
	m_shaftGenPrimitive.SetTexture(0, pOcclTex);
	m_shaftGenPrimitive.SetSampler(0, samplerState);
	m_shaftGenPrimitive.Compile(m_shaftGenPass);

	m_shaftGenPass.AddPrimitive(&m_shaftGenPrimitive);

	return true;
}

bool ImageSpaceShafts::PreparePrimitives(const SPreparePrimitivesContext& context)
{
	if (!IsVisible())
		return true;

	if (!m_pOccBuffer || m_bTexDirty)
	{
		InitTextures();
	}
	if (!m_pOccBuffer)
		return false;

	// update constants first
	{
		auto constants = m_occlusionPrimitive.GetConstantManager().BeginTypedConstantUpdate<SShaderParams>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);

		ColorF c = m_globalColor;
		c.NormalizeCol(c);
		c.ScaleCol(m_globalFlareBrightness);

		constants->color = c.toVec4();
		constants->screenSize.x = float(m_pOccBuffer->GetWidth());
		constants->screenSize.y = float(m_pOccBuffer->GetHeight());
		constants->screenSize.z = float(m_pDraftBuffer->GetWidth());
		constants->screenSize.w = float(m_pDraftBuffer->GetHeight());
		constants->sceneDepth.x = context.auxParams.linearDepth;

		for (int i = 0; i < context.viewInfoCount; ++i)
		{
			Vec3 screenPos = computeOrbitPos(context.lightScreenPos[i], m_globalOrbitAngle);
			ApplyCommonParams(constants, context.pViewInfo->viewport, screenPos, Vec2(m_globalSize));

			constants->meshCenterAndBrt = Vec4(screenPos, 1.0f);

			if (i < context.viewInfoCount - 1)
				constants.BeginStereoOverride(true);
		}

		m_occlusionPrimitive.GetConstantManager().EndTypedConstantUpdate(constants);
	}

	// prepare occlusion and shaft gen prepasses first
	if (!context.auxParams.bIgnoreOcclusionQueries)
	{
		if (PrepareOcclusion(m_pOccBuffer, m_pGoboTex.get(), EDefaultSamplerStates::BilinearClamp))
			context.prePasses.push_back(&m_occlusionPass);

		if (PrepareShaftGen(m_pDraftBuffer, m_pOccBuffer, EDefaultSamplerStates::BilinearClamp))
			context.prePasses.push_back(&m_shaftGenPass);
	}

	// regular pass primitive
	{
		static CCryNameTSCRC blendTech("ImageSpaceShaftsBlend");

		m_blendPrimitive.SetTechnique(CShaderMan::s_ShaderLensOptics, blendTech, 0);
		m_blendPrimitive.SetRenderState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);
		m_blendPrimitive.SetPrimitiveType(CRenderPrimitive::ePrim_FullscreenQuadCentered);
		m_blendPrimitive.SetTexture(0, m_pDraftBuffer);
		m_blendPrimitive.SetSampler(0, EDefaultSamplerStates::BilinearClamp);
		m_blendPrimitive.Compile(context.pass);

		context.pass.AddPrimitive(&m_blendPrimitive);
	}

	return true;
}
