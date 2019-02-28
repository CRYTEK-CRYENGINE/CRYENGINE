// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IrisShafts.h"
#include "RootOpticsElement.h"
#include "FlareSoftOcclusionQuery.h"
#include "../CryNameR.h"
#include "../../RenderDll/XRenderD3D9/DriverD3D.h"

#if defined(FLARES_SUPPORT_EDITING)
	#define MFPtr(FUNC_NAME) (Optics_MFPtr)(&IrisShafts::FUNC_NAME)
void IrisShafts::InitEditorParamGroups(DynArray<FuncVariableGroup>& groups)
{
	COpticsElement::InitEditorParamGroups(groups);
	FuncVariableGroup irisGroup;
	irisGroup.SetName("IrisShafts", "Iris Shafts");
	irisGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Enable gradient tex", "Enable gradient texture", this, MFPtr(SetEnableSpectrumTex), MFPtr(GetEnableSpectrumTex)));
	irisGroup.AddVariable(new OpticsMFPVariable(e_TEXTURE2D, "Base Tex", "Basic Texture", this, MFPtr(SetBaseTex), MFPtr(GetBaseTex)));
	irisGroup.AddVariable(new OpticsMFPVariable(e_TEXTURE2D, "Gradient Tex", "Gradient Texture", this, MFPtr(SetSpectrumTex), MFPtr(GetSpectrumTex)));
	irisGroup.AddVariable(new OpticsMFPVariable(e_INT, "Noise seed", "Noise seed", this, MFPtr(SetNoiseSeed), MFPtr(GetNoiseSeed), -255.0f, 255.0f));
	irisGroup.AddVariable(new OpticsMFPVariable(e_INT, "Complexity", "Complexity of shafts", this, MFPtr(SetComplexity), MFPtr(GetComplexity), 0, 1000.0f));
	irisGroup.AddVariable(new OpticsMFPVariable(e_INT, "Smoothness", "The level of smoothness", this, MFPtr(SetSmoothLevel), MFPtr(GetSmoothLevel), 0, 255.0f));
	irisGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Thickness", "Thickness of the shafts", this, MFPtr(SetThickness), MFPtr(GetThickness), 0, 255.0f));
	irisGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Thickness noise", "Noise strength of thickness variation", this, MFPtr(SetThicknessNoise), MFPtr(GetThicknessNoise)));
	irisGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Spread", "Spread of the shafts", this, MFPtr(SetSpread), MFPtr(GetSpread)));
	irisGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Spread noise", "Noise strength of spread variation", this, MFPtr(SetSpreadNoise), MFPtr(GetSpreadNoise)));
	irisGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Size noise", "Noise strength of shafts' sizes", this, MFPtr(SetSizeNoise), MFPtr(GetSizeNoise)));
	irisGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Spacing noise", "Noise strength of shafts' spacing", this, MFPtr(SetSpacingNoise), MFPtr(GetSpacingNoise)));
	groups.push_back(irisGroup);
}
	#undef MFPtr
#endif

IrisShafts::IrisShafts(const char* name)
	: COpticsElement(name, 0.5f)
	, m_fThickness(0.3f)
	, m_fSpread(0.2f)
	, m_nSmoothLevel(2)
	, m_nNoiseSeed(81)
	, m_pBaseTex(0)
	, m_fSizeNoiseStrength(0.8f)
	, m_fThicknessNoiseStrength(0.6f)
	, m_fSpacingNoiseStrength(0.2f)
	, m_fSpreadNoiseStrength(0.0f)
	, m_bUseSpectrumTex(false)
	, m_fPrimaryDir(0)
	, m_fAngleRange(1)
	, m_fConcentrationBoost(0)
	, m_fPrevOcc(-1.f)
	, m_fBrightnessBoost(0)
	, m_MaxNumberOfPolygon(0)
{
	m_vMovement.x = 1.f;
	m_vMovement.y = 1.f;
	m_Color.a = 1.f;

	SetAutoRotation(false);
	SetAspectRatioCorrection(true);
	SetColorComplexity(2);
	SetComplexity(32);

	m_meshDirty = true;

	m_primitive.AllocateTypedConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, sizeof(SShaderParams), EShaderStage_Vertex | EShaderStage_Pixel);
}

void IrisShafts::Load(IXmlNode* pNode)
{
	COpticsElement::Load(pNode);

	XmlNodeRef pIrisShaftsNode = pNode->findChild("IrisShafts");

	if (pIrisShaftsNode)
	{
		bool bUseSpectrumTex(m_bUseSpectrumTex);
		if (pIrisShaftsNode->getAttr("Enablegradienttex", bUseSpectrumTex))
			SetEnableSpectrumTex(bUseSpectrumTex);

		const char* baseTextureName = NULL;
		if (pIrisShaftsNode->getAttr("BaseTex", &baseTextureName))
		{
			if (baseTextureName && baseTextureName[0])
			{
				ITexture* pTexture = std::move(gEnv->pRenderer->EF_LoadTexture(baseTextureName));
				SetBaseTex((CTexture*)pTexture);
			}
		}

		const char* gradientTexName = NULL;
		if (pIrisShaftsNode->getAttr("GradientTex", &gradientTexName))
		{
			if (gradientTexName && gradientTexName[0])
			{
				ITexture* pTexture = std::move(gEnv->pRenderer->EF_LoadTexture(gradientTexName));
				SetSpectrumTex((CTexture*)pTexture);
			}
		}

		int nNoiseSeed(m_nNoiseSeed);
		if (pIrisShaftsNode->getAttr("Noiseseed", nNoiseSeed))
			SetNoiseSeed(nNoiseSeed);

		int nComplexity(m_nComplexity);
		if (pIrisShaftsNode->getAttr("Complexity", nComplexity))
			SetComplexity(nComplexity);

		int nSmoothLevel(m_nSmoothLevel);
		if (pIrisShaftsNode->getAttr("Smoothness", nSmoothLevel))
			SetSmoothLevel(nSmoothLevel);

		float fThickness(m_fThickness);
		if (pIrisShaftsNode->getAttr("Thickness", fThickness))
			SetThickness(fThickness);

		float fThicknessNoise(m_fThicknessNoiseStrength);
		if (pIrisShaftsNode->getAttr("Thicknessnoise", fThicknessNoise))
			SetThicknessNoise(fThicknessNoise);

		float fSpread(m_fSpread);
		if (pIrisShaftsNode->getAttr("Spread", fSpread))
			SetSpread(fSpread);

		float fThicknessNoiseStrength(m_fThicknessNoiseStrength);
		if (pIrisShaftsNode->getAttr("Spreadnoise", fThicknessNoiseStrength))
			SetSpreadNoise(fThicknessNoiseStrength);

		float fSizeNoiseStrength(m_fSizeNoiseStrength);
		if (pIrisShaftsNode->getAttr("Sizenoise", fSizeNoiseStrength))
			SetSizeNoise(fSizeNoiseStrength);

		float fSpacingNoiseStrength(m_fSpacingNoiseStrength);
		if (pIrisShaftsNode->getAttr("Spacingnoise", fSpacingNoiseStrength))
			SetSpacingNoise(fSpacingNoiseStrength);
	}
}

float IrisShafts::ComputeSpreadParameters(const float spread)
{
	return spread * 75;
}

int IrisShafts::ComputeDynamicSmoothLevel(int maxLevel, float spanAngle, float threshold)
{
	return min(maxLevel, (int)(spanAngle / threshold));
}

void IrisShafts::GenMesh()
{
	stable_rand::setSeed((int)(m_nNoiseSeed * m_fConcentrationBoost));

	float dirDelta = 1.0f / (float)m_nComplexity;
	float halfAngleRange = m_fAngleRange / 2;
	ColorF color(1, 1, 1, 1);

	m_vertices.clear();
	m_indices.clear();

	std::vector<int> randomTable;
	randomTable.resize(m_nComplexity);
	for (int i = 0; i < m_nComplexity; ++i)
		randomTable[i] = i;
	for (int i = 0; i < m_nComplexity; ++i)
		std::swap(randomTable[(int)(stable_rand::randPositive() * m_nComplexity)], randomTable[(int)(stable_rand::randPositive() * m_nComplexity)]);

	for (int i = 0; i < m_nComplexity; i++)
	{
		float spacingNoise = 1 + stable_rand::randUnit() * m_fSpacingNoiseStrength;

		float dirDiff;
		if (m_fConcentrationBoost > 1.f && randomTable[i] == m_nComplexity / 2)
			dirDiff = dirDelta * spacingNoise * 0.05f;
		else
			dirDiff = m_fAngleRange * fmod((randomTable[i] * dirDelta + spacingNoise), 1.0f) - halfAngleRange;
		float dirUnit = m_fPrimaryDir + dirDiff;
		float dir = 360 * dirUnit;

		float dirDiffRatio = fabs(dirDiff) / m_fAngleRange;
		float sizeBoost = 1 + (m_fConcentrationBoost - 1) * (-1.75f * dirDiffRatio + 2.f);                 // From center to edge: 2->0.25
		float brightnessBoost = 1 + (m_fConcentrationBoost - 1) * (1 / (15 * (dirDiffRatio + 0.02f)) - 1); // from center to edge: 2.333->-0.994

		color.a = brightnessBoost;
		float size = sizeBoost * (1 + stable_rand::randUnit() * m_fSizeNoiseStrength);
		float thickness = m_fThickness * (1 + stable_rand::randUnit() * m_fThicknessNoiseStrength);
		float spread = m_fSpread * (1 + stable_rand::randUnit() * m_fSpreadNoiseStrength);
		float halfAngle = ComputeSpreadParameters(spread);
		int dynSmoothLevel = ComputeDynamicSmoothLevel(m_nSmoothLevel, halfAngle * 2, 1);
		if (dynSmoothLevel <= 0)
			continue;

		std::vector<SVF_P3F_C4B_T2F> vertices;
		std::vector<uint16> indices;
		MeshUtil::GenShaft(size, thickness, dynSmoothLevel, dir - halfAngle, dir + halfAngle, color, vertices, indices);

		int generatedPolyonNum = (int)(indices.size() + m_indices.size()) / 3;
		if (CRenderer::CV_r_FlaresIrisShaftMaxPolyNum != 0 && generatedPolyonNum > CRenderer::CV_r_FlaresIrisShaftMaxPolyNum)
			break;

		int indexOffset = m_vertices.size();
		m_vertices.insert(m_vertices.end(), vertices.begin(), vertices.end());

		for (int k = 0, iIndexSize(indices.size()); k < iIndexSize; ++k)
			m_indices.push_back(indices[k] + indexOffset);
	}
}

bool IrisShafts::PreparePrimitives(const SPreparePrimitivesContext& context)
{
	if (!IsVisible())
		return true;

	static CCryNameTSCRC techName("IrisShafts");

	// update occlusion dependent parameters
	if (m_globalOcclusionBokeh)
	{
		RootOpticsElement* root = GetRoot();
		CFlareSoftOcclusionQuery* occQuery = root->GetOcclusionQuery();
		float occ = occQuery->GetOccResult();
		float interpOcc = root->GetShaftVisibilityFactor();

		if (occ >= 0.05f && fabs(m_fPrevOcc - occ) > 0.01f)
		{
			m_fAngleRange = 0.65f * occ * occ + 0.35f;
			m_fPrimaryDir = occQuery->GetDirResult() / (2 * PI);
			m_fConcentrationBoost = 2.0f - occ;
			m_fBrightnessBoost = 1 + 2 * pow(occ - 1, 6);

			m_meshDirty = true;
			m_fPrevOcc = occ;
		}
		else if (occ < 0.05f)
		{
			m_fAngleRange = 0.33f;
			m_fBrightnessBoost = 1 + 2 * pow(interpOcc - 1, 6);
			m_fPrevOcc = 0;
			m_meshDirty = true;
		}
	}
	else
	{
		if (m_fAngleRange < 0.999f)
			m_meshDirty = true;
		m_fPrimaryDir = 0;
		m_fAngleRange = 1;
		m_fConcentrationBoost = 1;
		m_fBrightnessBoost = 1;
	}

	uint64 rtFlags = 0;
	ApplyGeneralFlags(rtFlags);
	ApplySpectrumTexFlag(rtFlags, m_bUseSpectrumTex);

	m_primitive.SetTechnique(CShaderMan::s_ShaderLensOptics, techName, rtFlags);
	m_primitive.SetRenderState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);
	m_primitive.SetTexture(0, (m_bUseSpectrumTex && m_pSpectrumTex) ? m_pSpectrumTex.get() : CRendererResources::s_ptexBlack);
	m_primitive.SetTexture(1, m_pBaseTex ? m_pBaseTex.get() : CRendererResources::s_ptexBlack);
	m_primitive.SetSampler(0, EDefaultSamplerStates::LinearBorder_Black);

	// update constants
	{
		auto constants = m_primitive.GetConstantManager().BeginTypedConstantUpdate<SShaderParams>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);

		for (int i = 0; i < context.viewInfoCount; ++i)
		{
			ApplyCommonParams(constants, context.pViewInfo->viewport, context.lightScreenPos[0], Vec2(m_globalSize));

			Vec3 screenPos = computeOrbitPos(context.lightScreenPos[i], m_globalOrbitAngle);
			constants->meshCenterAndBrt[0] = computeMovementLocationX(screenPos);
			constants->meshCenterAndBrt[1] = computeMovementLocationY(screenPos);
			constants->meshCenterAndBrt[2] = context.lightScreenPos[i].z;
			constants->meshCenterAndBrt[3] = 1.0f;

			if (i < context.viewInfoCount - 1)
				constants.BeginStereoOverride(false);
		}
		m_primitive.GetConstantManager().EndTypedConstantUpdate(constants);
	}


	// update mesh
	{
		if (m_MaxNumberOfPolygon != CRenderer::CV_r_FlaresIrisShaftMaxPolyNum)
		{
			m_meshDirty = true;
			m_MaxNumberOfPolygon = CRenderer::CV_r_FlaresIrisShaftMaxPolyNum;
		}

		ValidateMesh();

		m_primitive.SetCustomVertexStream(m_vertexBuffer, EDefaultInputLayouts::P3F_C4B_T2F, sizeof(SVF_P3F_C4B_T2F));
		m_primitive.SetCustomIndexStream(m_indexBuffer, Index16);
		m_primitive.SetDrawInfo(eptTriangleList, 0, 0, GetIndexCount());
	}

	m_primitive.Compile(context.pass);
	context.pass.AddPrimitive(&m_primitive);

	return true;
}
