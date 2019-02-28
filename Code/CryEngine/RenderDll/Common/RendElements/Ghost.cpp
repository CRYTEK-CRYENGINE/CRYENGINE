// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Ghost.h"
#include "MeshUtil.h"

#include "../../RenderDll/XRenderD3D9/DriverD3D.h"

#if defined(FLARES_SUPPORT_EDITING)
	#define MFPtr(FUNC_NAME) (Optics_MFPtr)(&CLensGhost::FUNC_NAME)
void CLensGhost::InitEditorParamGroups(DynArray<FuncVariableGroup>& groups)
{
	COpticsElement::InitEditorParamGroups(groups);
	FuncVariableGroup ghostGroup;
	ghostGroup.SetName("LensGhost", "Lens Ghost");
	ghostGroup.AddVariable(new OpticsMFPVariable(e_TEXTURE2D, "Texture", "The texture for lens ghosts", this, MFPtr(SetTexture), MFPtr(GetTexture)));
	groups.push_back(ghostGroup);
}
	#undef MFPtr
#endif

CLensGhost::CLensGhost(const char* name, CTexture* externalTex)
	: COpticsElement(name)
	, m_pTex(externalTex)
{
	CConstantBufferPtr pcb = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SShaderParams), true, true);

	m_primitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pcb, EShaderStage_Vertex | EShaderStage_Pixel);
}

void CLensGhost::Load(IXmlNode* pNode)
{
	COpticsElement::Load(pNode);

	XmlNodeRef pLensGhost = pNode->findChild("LensGhost");
	if (pLensGhost)
	{
		const char* textureName(NULL);
		if (pLensGhost->getAttr("Texture", &textureName))
		{
			if (textureName && textureName[0])
			{
				ITexture* pTexture = std::move(gEnv->pRenderer->EF_LoadTexture(textureName));
				SetTexture((CTexture*)pTexture);
			}
		}
	}
}

CTexture* CLensGhost::GetTexture()
{
	if (!m_pTex)
	{
		m_pTex = std::move(CTexture::ForName("%ENGINE%/EngineAssets/Textures/flares/default-flare.tif", FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown));
	}

	return m_pTex;
}

bool CLensGhost::PreparePrimitives(const SPreparePrimitivesContext& context)
{
	if (!IsVisible())
		return true;

	static CCryNameTSCRC techGhost("Ghost");

	uint64 rtFlags = 0;
	ApplyGeneralFlags(rtFlags);
	ApplyOcclusionBokehFlag(rtFlags);

	CTexture* pGhostTex = GetTexture() ? GetTexture() : CRendererResources::s_ptexBlack;
	m_primitive.SetTechnique(CShaderMan::s_ShaderLensOptics, techGhost, rtFlags);
	m_primitive.SetRenderState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);
	m_primitive.SetPrimitiveType(CRenderPrimitive::ePrim_FullscreenQuadCentered);
	m_primitive.SetTexture(0, pGhostTex);
	m_primitive.SetSampler(0, EDefaultSamplerStates::LinearBorder_Black);

	// update constants
	{
		auto constants = m_primitive.GetConstantManager().BeginTypedConstantUpdate<SShaderParams>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);

		if (m_globalOcclusionBokeh)
			ApplyOcclusionPattern(constants, m_primitive);
		else
			m_primitive.SetTexture(5, CRendererResources::s_ptexBlack);

		ColorF c = m_globalColor;
		c.NormalizeCol(c);
		c.ScaleCol(m_globalFlareBrightness);

		constants->color = c.toVec4();
		constants->ghostTileInfo = m_vTileDefinition;
		constants->baseTexSize = Vec4((float)pGhostTex->GetWidth(), (float)pGhostTex->GetHeight(), 0, 0);

		for (int i = 0; i < context.viewInfoCount; ++i)
		{
			ApplyCommonParams(constants, context.pViewInfo->viewport, context.lightScreenPos[i], Vec2(m_globalSize));

			Vec3 screenPos = computeOrbitPos(context.lightScreenPos[i], m_globalOrbitAngle);
			constants->meshCenterAndBrt.x = computeMovementLocationX(screenPos);
			constants->meshCenterAndBrt.y = computeMovementLocationY(screenPos);
			constants->meshCenterAndBrt.z = context.lightScreenPos[i].z;
			constants->meshCenterAndBrt.w = m_globalFlareBrightness;

			if (i < context.viewInfoCount - 1)
				constants.BeginStereoOverride(true);
		}

		m_primitive.GetConstantManager().EndTypedConstantUpdate(constants);
	}
	
	m_primitive.Compile(context.pass);
	context.pass.AddPrimitive(&m_primitive);

	return true;
}


#if defined(FLARES_SUPPORT_EDITING)
	#define MFPtr(FUNC_NAME) (Optics_MFPtr)(&CMultipleGhost::FUNC_NAME)
void CMultipleGhost::InitEditorParamGroups(DynArray<FuncVariableGroup>& groups)
{
	COpticsElement::InitEditorParamGroups(groups);
	FuncVariableGroup ghostGroup;
	ghostGroup.SetName("MultiGhost", "Multi Ghost");
	ghostGroup.AddVariable(new OpticsMFPVariable(e_TEXTURE2D, "Texture", "The texture for lens ghosts", this, MFPtr(SetTexture), MFPtr(GetTexture)));
	ghostGroup.AddVariable(new OpticsMFPVariable(e_INT, "Count", "The number of ghosts", this, MFPtr(SetCount), MFPtr(GetCount), 0, 1000.0f));
	ghostGroup.AddVariable(new OpticsMFPVariable(e_INT, "Random Seed", "The Seed of random generator", this, MFPtr(SetRandSeed), MFPtr(GetRandSeed), -255.0f, 255.0f));
	ghostGroup.AddVariable(new OpticsMFPVariable(e_VEC2, "Scattering range", "The scattering range for sub ghosts", this, MFPtr(SetRange), MFPtr(GetRange)));
	ghostGroup.AddVariable(new OpticsMFPVariable(e_VEC2, "position factor", "multiplier of position", this, MFPtr(SetPositionFactor), MFPtr(GetPositionFactor)));
	ghostGroup.AddVariable(new OpticsMFPVariable(e_VEC2, "position offset", "offset of position", this, MFPtr(SetPositionOffset), MFPtr(GetPositionOffset)));
	ghostGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "X-axis noise", "the noise of scattering on x-axis", this, MFPtr(SetXOffsetNoise), MFPtr(GetXOffsetNoise)));
	ghostGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Y-axis noise", "the noise of scattering on y-axis", this, MFPtr(SetYOffsetNoise), MFPtr(GetYOffsetNoise)));
	ghostGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Size Variation", "The strength for size variation", this, MFPtr(SetSizeNoise), MFPtr(GetSizeNoise)));
	ghostGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Brightness Variation", "The strength for brightness variation", this, MFPtr(SetBrightnessNoise), MFPtr(GetBrightnessNoise)));
	ghostGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Color Variation", "The strength for color variation", this, MFPtr(SetColorNoise), MFPtr(GetColorNoise)));
	groups.push_back(ghostGroup);
}
	#undef MFPtr
#endif

void CMultipleGhost::Load(IXmlNode* pNode)
{
	COpticsElement::Load(pNode);

	XmlNodeRef pMultiGhostNode = pNode->findChild("MultiGhost");

	if (pMultiGhostNode)
	{
		const char* textureName(NULL);
		if (pMultiGhostNode->getAttr("Texture", &textureName))
		{
			if (textureName && textureName[0])
			{
				ITexture* pTexture = gEnv->pRenderer->EF_LoadTexture(textureName);
				SetTexture((CTexture*)pTexture);
				if (pTexture)
					pTexture->Release();
			}
		}

		int nCount(0);
		if (pMultiGhostNode->getAttr("Count", nCount))
			SetCount(nCount);

		int nRandSeed(0);
		if (pMultiGhostNode->getAttr("RandomSeed", nRandSeed))
			SetRandSeed(nRandSeed);

		Vec2 vRange(m_vRange);
		if (pMultiGhostNode->getAttr("Scatteringrange", vRange))
			SetRange(vRange);

		Vec2 vPositionFactor(m_vPositionFactor);
		if (pMultiGhostNode->getAttr("positionfactor", vPositionFactor))
			SetPositionFactor(vPositionFactor);

		Vec2 vPositionOffset(m_vPositionOffset);
		if (pMultiGhostNode->getAttr("positionoffset", vPositionOffset))
			SetPositionOffset(vPositionOffset);

		float xOffsetNoise(m_fXOffsetNoise);
		if (pMultiGhostNode->getAttr("X-axisnoise", xOffsetNoise))
			SetXOffsetNoise(xOffsetNoise);

		float yOffsetNoise(m_fYOffsetNoise);
		if (pMultiGhostNode->getAttr("Y-axisnoise", yOffsetNoise))
			SetYOffsetNoise(yOffsetNoise);

		float fSizeNoise(m_fSizeNoise);
		if (pMultiGhostNode->getAttr("SizeVariation", fSizeNoise))
			SetSizeNoise(fSizeNoise);

		float fBrightnessNoise(m_fBrightnessNoise);
		if (pMultiGhostNode->getAttr("BrightnessVariation", fBrightnessNoise))
			SetBrightnessNoise(fBrightnessNoise);

		float fColorNoise(m_fColorNoise);
		if (pMultiGhostNode->getAttr("ColorVariation", fColorNoise))
			SetColorNoise(fColorNoise);
	}
}

void CMultipleGhost::GenGhosts(const SAuxParams& aux)
{
	stable_rand::setSeed(GetRandSeed());

	float rangeStart = m_vRange.x;
	float rangeEnd = m_vRange.y;
	float span = rangeEnd - rangeStart;
	float halfXNoiseRange = span / 2 * m_fXOffsetNoise;
	float halfYNoiseRange = span / 2 * m_fYOffsetNoise;
	if ((uint)GetCount() != children.size())
		SetCount(GetCount());
	for (uint i = 0; i < children.size(); i++)
	{
		CLensGhost* ghost = (CLensGhost*)&*(children[i]);
		ghost->SetTexture(GetTexture());
		Vec2 pos;
		float axisPos = stable_rand::randPositive() * span + rangeStart;
		pos.x = stable_rand::randUnit() * halfXNoiseRange + axisPos * m_vPositionFactor.x + m_vPositionOffset.x;
		pos.y = stable_rand::randUnit() * halfYNoiseRange + axisPos * m_vPositionFactor.y + m_vPositionOffset.y;
		ghost->SetMovement(pos);
		ghost->SetSize(stable_rand::randBias(GetSizeNoise()));
		ghost->SetBrightness(stable_rand::randBias(GetBrightnessNoise()));
		ColorF variation(stable_rand::randBias(GetColorNoise()), stable_rand::randBias(GetColorNoise()), stable_rand::randBias(GetColorNoise()));
		ghost->SetColor(variation);

		ghost->SetDynamicsEnabled(GetDynamicsEnabled());
		ghost->SetDynamicsInvert(GetDynamicsInvert());
		ghost->SetDynamicsOffset(GetDynamicsOffset());
		ghost->SetDynamicsRange(GetDynamicsRange());
	}
	validateChildrenGlobalVars(aux);
	m_bContentDirty = false;
}

bool CMultipleGhost::PreparePrimitives(const SPreparePrimitivesContext& context)
{
	if (!IsVisible())
		return true;

	if (m_bContentDirty)
		GenGhosts(context.auxParams);

	return COpticsGroup::PreparePrimitives(context);
}
