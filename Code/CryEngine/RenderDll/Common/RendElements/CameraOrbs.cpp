// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "MeshUtil.h"
#include "CameraOrbs.h"
#include "../Textures/Texture.h"
#include "../../RenderDll/XRenderD3D9/DriverD3D.h"

#if defined(FLARES_SUPPORT_EDITING)
	#define MFPtr(FUNC_NAME) (Optics_MFPtr)(&CameraOrbs::FUNC_NAME)
void CameraOrbs::InitEditorParamGroups(DynArray<FuncVariableGroup>& groups)
{
	COpticsElement::InitEditorParamGroups(groups);

	FuncVariableGroup camGroup;
	camGroup.SetName("CameraOrbs", "Camera Orbs");
	camGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Illum range", "Illum range", this, MFPtr(SetIllumRange), MFPtr(GetIllumRange)));
	camGroup.AddVariable(new OpticsMFPVariable(e_TEXTURE2D, "Orb Texture", "The texture for orbs", this, MFPtr(SetOrbTex), MFPtr(GetOrbTex)));
	camGroup.AddVariable(new OpticsMFPVariable(e_TEXTURE2D, "Lens Texture", "The texture for lens", this, MFPtr(SetLensTex), MFPtr(GetLensTex)));
	camGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Enable lens texture", "Enable lens texture", this, MFPtr(SetUseLensTex), MFPtr(GetUseLensTex)));
	camGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Enable lens detail shading", "Enable lens detail shading", this, MFPtr(SetEnableLensDetailShading), MFPtr(GetEnableLensDetailShading)));
	camGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Lens texture strength", "Lens texture strength", this, MFPtr(SetLensTexStrength), MFPtr(GetLensTexStrength)));
	camGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Lens detail shading strength", "Lens detail shading strength", this, MFPtr(SetLensDetailShadingStrength), MFPtr(GetLensDetailShadingStrength)));
	camGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Lens detail bumpiness", "Lens detail bumpiness", this, MFPtr(SetLensDetailBumpiness), MFPtr(GetLensDetailBumpiness)));
	camGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Enable orb detail shading", "Enable orb detail shading", this, MFPtr(SetEnableOrbDetailShading), MFPtr(GetEnableOrbDetailShading)));
	groups.push_back(camGroup);

	FuncVariableGroup genGroup;
	genGroup.SetName("Generator");
	genGroup.AddVariable(new OpticsMFPVariable(e_INT, "Number of orbs", "Number of orbs", this, MFPtr(SetNumOrbs), MFPtr(GetNumOrbs), 0, 1000.0f));
	genGroup.AddVariable(new OpticsMFPVariable(e_INT, "Noise seed", "Noise seed", this, MFPtr(SetNoiseSeed), MFPtr(GetNoiseSeed), -255.0f, 255.0f));
	genGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Color variation", "Color variation", this, MFPtr(SetColorNoise), MFPtr(GetColorNoise)));
	genGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Size variation", "Size variation", this, MFPtr(SetSizeNoise), MFPtr(GetSizeNoise)));
	genGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Rotation variation", "Rotation variation", this, MFPtr(SetRotationNoise), MFPtr(GetRotationNoise)));
	groups.push_back(genGroup);

	FuncVariableGroup advShadingGroup;
	advShadingGroup.SetName("AdvancedShading", "Advanced Shading");
	advShadingGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Enable adv shading", "Enable advanced shading mode", this, MFPtr(SetEnableAdvancdShading), MFPtr(GetEnableAdvancedShading)));
	advShadingGroup.AddVariable(new OpticsMFPVariable(e_COLOR, "Ambient Diffuse", "Ambient diffuse light (RGBK)", this, MFPtr(SetAmbientDiffuseRGBK), MFPtr(GetAmbientDiffuseRGBK)));
	advShadingGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Absorptance", "Absorptance of on-lens dirt", this, MFPtr(SetAbsorptance), MFPtr(GetAbsorptance)));
	advShadingGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Transparency", "Transparency of on-lens dirt", this, MFPtr(SetTransparency), MFPtr(GetTransparency)));
	advShadingGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Scattering", "Subsurface Scattering of on-lens dirt", this, MFPtr(SetScatteringStrength), MFPtr(GetScatteringStrength)));
	groups.push_back(advShadingGroup);
}
	#undef MFPtr
#endif


CameraOrbs::CameraOrbs(const char* name, const int numOrbs)
	: COpticsElement(name, 0.19f)
	, m_fSizeNoise(0.8f)
	, m_fBrightnessNoise(0.4f)
	, m_fRotNoise(0.8f)
	, m_fClrNoise(0.5f)
	, m_fIllumRadius(1.f)
	, m_bUseLensTex(0)
	, m_bOrbDetailShading(0)
	, m_bLensDetailShading(0)
	, m_fLensTexStrength(1.f)
	, m_fLensDetailShadingStrength(0.157f)
	, m_fLensDetailBumpiness(0.073f)
	, m_bAdvancedShading(false)
	, m_cAmbientDiffuse(LensOpConst::_LO_DEF_CLR_BLK)
	, m_fAbsorptance(4.0f)
	, m_fTransparency(0.37f)
	, m_fScatteringStrength(1.0f)
	, m_iNoiseSeed(0)
	, m_spriteAspectRatio(1.0f)
{
	m_Color.a = 1.f;
	SetPerspectiveFactor(0.f);
	SetRotation(0.7f);
	SetNumOrbs(numOrbs);
	m_meshDirty = true;

	// share one constant buffer between both primitives
	CConstantBufferPtr pSharedCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SShaderParams), true, true);
	m_GlowPrimitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive,       pSharedCB, EShaderStage_Vertex | EShaderStage_Pixel);
	m_CameraLensPrimitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pSharedCB, EShaderStage_Vertex | EShaderStage_Pixel);
}

void CameraOrbs::Load(IXmlNode* pNode)
{
	COpticsElement::Load(pNode);

	XmlNodeRef pCameraOrbsNode = pNode->findChild("CameraOrbs");
	if (pCameraOrbsNode)
	{
		float fIllumRadius(m_fIllumRadius);
		if (pCameraOrbsNode->getAttr("Illumrange", fIllumRadius))
			SetIllumRange(fIllumRadius);

		const char* orbTextureName(NULL);
		if (pCameraOrbsNode->getAttr("OrbTexture", &orbTextureName))
		{
			if (orbTextureName && orbTextureName[0])
			{
				ITexture* pTexture = gEnv->pRenderer->EF_LoadTexture(orbTextureName);
				SetOrbTex((CTexture*)pTexture);
				if (pTexture)
					pTexture->Release();
			}
		}

		const char* lensTextureName(NULL);
		if (pCameraOrbsNode->getAttr("LensTexture", &lensTextureName))
		{
			if (lensTextureName && lensTextureName[0])
			{
				ITexture* pTexture = gEnv->pRenderer->EF_LoadTexture(lensTextureName);
				SetLensTex((CTexture*)pTexture);
				if (pTexture)
					pTexture->Release();
			}
		}

		bool bUseLensTex(m_bUseLensTex);
		if (pCameraOrbsNode->getAttr("Enablelenstexture", bUseLensTex))
			SetUseLensTex(bUseLensTex);

		bool bLensDetailShading(m_bLensDetailShading);
		if (pCameraOrbsNode->getAttr("Enablelensdetailshading", bLensDetailShading))
			SetEnableLensDetailShading(bLensDetailShading);

		float fLensTexStrength(m_fLensTexStrength);
		if (pCameraOrbsNode->getAttr("Lenstexturestrength", fLensTexStrength))
			SetLensTexStrength(fLensTexStrength);

		float fLensDetailShadingStrength(m_fLensDetailShadingStrength);
		if (pCameraOrbsNode->getAttr("Lensdetailshadingstrength", fLensDetailShadingStrength))
			SetLensDetailShadingStrength(fLensDetailShadingStrength);

		float fLensDetailBumpiness(m_fLensDetailBumpiness);
		if (pCameraOrbsNode->getAttr("Lensdetailbumpiness", fLensDetailBumpiness))
			SetLensDetailBumpiness(fLensDetailBumpiness);

		bool bOrbDetailShading(m_bOrbDetailShading);
		if (pCameraOrbsNode->getAttr("Enableorbdetailshading", bOrbDetailShading))
			SetEnableOrbDetailShading(bOrbDetailShading);
	}

	XmlNodeRef pGeneratorNode = pNode->findChild("Generator");
	if (pGeneratorNode)
	{
		int numOfOrbs(m_nNumOrbs);
		if (pGeneratorNode->getAttr("Numberoforbs", numOfOrbs))
			SetNumOrbs(numOfOrbs);

		int nNoiseSeed(m_iNoiseSeed);
		if (pGeneratorNode->getAttr("Noiseseed", nNoiseSeed))
			SetNoiseSeed(nNoiseSeed);

		float fColorNoise(m_fClrNoise);
		if (pGeneratorNode->getAttr("Colorvariation", fColorNoise))
			SetColorNoise(fColorNoise);

		float fSizeNoise(m_fSizeNoise);
		if (pGeneratorNode->getAttr("Sizevariation", fSizeNoise))
			SetSizeNoise(fSizeNoise);

		float fRotNoise(m_fRotNoise);
		if (pGeneratorNode->getAttr("Rotationvariation", fRotNoise))
			SetRotationNoise(fRotNoise);
	}

	XmlNodeRef pAdvancedShading = pNode->findChild("AdvancedShading");
	if (pAdvancedShading)
	{
		bool bAdvancedShading(m_bAdvancedShading);
		if (pAdvancedShading->getAttr("Enableadvshading", bAdvancedShading))
			SetEnableAdvancdShading(bAdvancedShading);

		Vec3 vColor(m_cAmbientDiffuse.r, m_cAmbientDiffuse.g, m_cAmbientDiffuse.b);
		int nAlpha((int)(m_cAmbientDiffuse.a * 255.0f));
		if (pAdvancedShading->getAttr("AmbientDiffuse", vColor) && pAdvancedShading->getAttr("AmbientDiffuse.alpha", nAlpha))
			SetAmbientDiffuseRGBK(ColorF(vColor.x, vColor.y, vColor.z, (float)nAlpha / 255.0f));

		float fAbsorptance(m_fAbsorptance);
		if (pAdvancedShading->getAttr("Absorptance", fAbsorptance))
			SetAbsorptance(fAbsorptance);

		float fTransparency(m_fTransparency);
		if (pAdvancedShading->getAttr("Transparency", fTransparency))
			SetTransparency(fTransparency);

		float fScatteringStrength(m_fScatteringStrength);
		if (pAdvancedShading->getAttr("Scattering", fScatteringStrength))
			SetScatteringStrength(fScatteringStrength);
	}
}

void CameraOrbs::GenMesh()
{
	ScatterOrbs();
	MeshUtil::GenSprites(m_OrbsList, m_spriteAspectRatio, true, m_vertices, m_indices);
	MeshUtil::TrianglizeQuadIndices(m_OrbsList.size(), m_indices);
}

void CameraOrbs::ScatterOrbs()
{
	stable_rand::setSeed(m_iNoiseSeed);
	for (uint32 i = 0; i < m_OrbsList.size(); i++)
	{
		SpritePoint& sprite = m_OrbsList[i];

		Vec2& p = sprite.pos;
		p.x = stable_rand::randUnit();
		p.y = stable_rand::randUnit();

		sprite.rotation = m_fRotation * stable_rand::randBias(GetRotationNoise()) * 2 * PI;
		sprite.size = m_globalSize * stable_rand::randBias(GetSizeNoise());
		sprite.brightness = m_globalFlareBrightness * stable_rand::randBias(GetBrightnessNoise());

		ColorF& clr = sprite.color;
		ColorF variation(stable_rand::randBias(m_fClrNoise), stable_rand::randBias(m_fClrNoise), stable_rand::randBias(m_fClrNoise));
		clr = variation;

		float clrMax = clr.Max();
		clr /= clrMax;
		clr.a = m_globalColor.a;
	}
}

CTexture* CameraOrbs::GetOrbTex()
{
	if (!m_pOrbTex)
	{
		m_pOrbTex = std::move(CTexture::ForName("%ENGINE%/EngineAssets/Textures/flares/default-orb.tif", FT_DONT_STREAM, eTF_Unknown));
	}

	return m_pOrbTex;
}

CTexture* CameraOrbs::GetLensTex()
{
	if (!m_pLensTex)
	{
		m_pLensTex = std::move(CTexture::ForName("%ENGINE%/EngineAssets/Textures/flares/Smudgy.tif", FT_DONT_STREAM, eTF_Unknown));
	}

	return m_pLensTex;
}

void CameraOrbs::ApplyOrbFlags(uint64& rtFlags, bool detailShading) const
{
	if (detailShading)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE4];
}

void CameraOrbs::ApplyAdvancedShadingFlag(uint64& rtFlags) const
{
	if (m_bAdvancedShading)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE2];
}

void CameraOrbs::ApplyAdvancedShadingParams(SShaderParams& shaderParams, CRenderPrimitive& primitive, const ColorF& ambDiffuseRGBK, float absorptance, float transparency, float scattering) const
{
	CTexture* pAmbTex = CRendererResources::s_ptexSceneTarget;

	shaderParams.ambientDiffuseRGBK = Vec4(ambDiffuseRGBK.r, ambDiffuseRGBK.g, ambDiffuseRGBK.b, ambDiffuseRGBK.a);
	shaderParams.advShadingParams = Vec4(absorptance, transparency, scattering, 0);
	primitive.SetTexture(1, pAmbTex);
	primitive.SetSampler(1, EDefaultSamplerStates::PointClamp);
}

bool CameraOrbs::PreparePrimitives(const SPreparePrimitivesContext& context)
{
	if (!IsVisible())
		return true;

	static CCryNameTSCRC techCameraOrbs("CameraOrbs");
	static CCryNameTSCRC techCameraLens("CameraLens");

	uint64 rtFlags = 0;
	ApplyGeneralFlags(rtFlags);
	ApplyAdvancedShadingFlag(rtFlags);
	ApplyOcclusionBokehFlag(rtFlags);
	ApplyOrbFlags(rtFlags, m_bOrbDetailShading);

	m_GlowPrimitive.SetTechnique(CShaderMan::s_ShaderLensOptics, techCameraOrbs, rtFlags);
	m_GlowPrimitive.SetRenderState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);

	CTexture* pOrbTex = GetOrbTex();
	m_GlowPrimitive.SetTexture(0, pOrbTex ? pOrbTex : CRendererResources::s_ptexBlack);
	m_GlowPrimitive.SetSampler(0, EDefaultSamplerStates::LinearBorder_Black);

	if (!m_globalOcclusionBokeh)
		m_GlowPrimitive.SetTexture(5, CRendererResources::s_ptexBlack);

	// update constants
	{
		auto constants = m_GlowPrimitive.GetConstantManager().BeginTypedConstantUpdate<SShaderParams>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);

		ApplyCommonParams(constants, context.pViewInfo->viewport, context.lightScreenPos[0], Vec2(m_globalSize));
		constants->lensDetailParams = Vec4(1, 1, GetLensDetailBumpiness(), 0);

		if (m_bUseLensTex)
			constants->lensDetailParams2 = Vec4(GetLensTexStrength(), GetLensDetailShadingStrength(), GetLensDetailBumpiness(), 0);

		if (m_globalOcclusionBokeh)
			ApplyOcclusionPattern(constants, m_GlowPrimitive);
		if (m_bAdvancedShading)
			ApplyAdvancedShadingParams(constants, m_GlowPrimitive, GetAmbientDiffuseRGBK(), GetAbsorptance(), GetTransparency(), GetScatteringStrength());

		const ColorF lightColor = m_globalFlareBrightness * m_globalColor * m_globalColor.a;
		constants->lightColorInfo[0] = lightColor.r;
		constants->lightColorInfo[1] = lightColor.g;
		constants->lightColorInfo[2] = lightColor.b;
		constants->lightColorInfo[3] = m_fIllumRadius;

		for (int i = 0; i < context.viewInfoCount; ++i)
		{
			Vec3 screenPos = computeOrbitPos(context.lightScreenPos[i], m_globalOrbitAngle);
			const float x = computeMovementLocationX(screenPos);
			const float y = computeMovementLocationY(screenPos);

			constants->lightProjPos = Vec4(x, y, context.auxParams.linearDepth, 0);

			if (i < context.viewInfoCount - 1)
				constants.BeginStereoOverride(true);
		}

		m_GlowPrimitive.GetConstantManager().EndTypedConstantUpdate(constants);
	}

	// geometry
	{
		m_spriteAspectRatio = context.pViewInfo[0].viewport.width / float(context.pViewInfo[0].viewport.height);

		ValidateMesh();

		m_GlowPrimitive.SetCustomVertexStream(m_vertexBuffer, EDefaultInputLayouts::P3F_C4B_T2F, sizeof(SVF_P3F_C4B_T2F));
		m_GlowPrimitive.SetCustomIndexStream(m_indexBuffer, Index16);
		m_GlowPrimitive.SetDrawInfo(eptTriangleList, 0, 0, GetIndexCount());
	}

	m_GlowPrimitive.Compile(context.pass);
	context.pass.AddPrimitive(&m_GlowPrimitive);

	if (m_bUseLensTex)
	{
		uint64 rtFlags = 0;
		ApplyOrbFlags(rtFlags, m_bLensDetailShading);

		m_CameraLensPrimitive.SetTechnique(CShaderMan::s_ShaderLensOptics, techCameraLens, rtFlags);
		m_CameraLensPrimitive.SetRenderState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);
		m_CameraLensPrimitive.SetPrimitiveType(CRenderPrimitive::ePrim_FullscreenQuadTess);

		CTexture* pLensTex = GetLensTex();
		m_CameraLensPrimitive.SetTexture(0, pOrbTex ? pOrbTex : CRendererResources::s_ptexBlack);
		m_CameraLensPrimitive.SetTexture(2, pLensTex ? pLensTex : CRendererResources::s_ptexBlack);
		m_CameraLensPrimitive.SetSampler(0, EDefaultSamplerStates::LinearBorder_Black);
		m_CameraLensPrimitive.Compile(context.pass);

		context.pass.AddPrimitive(&m_CameraLensPrimitive);
	}

	return true;
}

