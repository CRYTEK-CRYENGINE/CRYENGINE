// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Streaks.h"
#include "RootOpticsElement.h"
#include "../CryNameR.h"
#include "../../RenderDll/XRenderD3D9/DriverD3D.h"

#if defined(FLARES_SUPPORT_EDITING)
	#define MFPtr(FUNC_NAME) (Optics_MFPtr)(&Streaks::FUNC_NAME)
void Streaks::InitEditorParamGroups(DynArray<FuncVariableGroup>& groups)
{
	COpticsElement::InitEditorParamGroups(groups);

	FuncVariableGroup streakGroup;
	streakGroup.SetName("Streaks", "Streaks");
	streakGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Enable gradient tex", "Enable gradient texture", this, MFPtr(SetEnableSpectrumTex), MFPtr(GetEnableSpectrumTex)));
	streakGroup.AddVariable(new OpticsMFPVariable(e_TEXTURE2D, "Gradient Tex", "Gradient Texture", this, MFPtr(SetSpectrumTex), MFPtr(GetSpectrumTex)));
	streakGroup.AddVariable(new OpticsMFPVariable(e_INT, "Noise seed", "Noise seed", this, MFPtr(SetNoiseSeed), MFPtr(GetNoiseSeed), -255.0f, 255.0f));
	streakGroup.AddVariable(new OpticsMFPVariable(e_INT, "Streak count", "Number of streaks to generate", this, MFPtr(SetStreakCount), MFPtr(GetStreakCount), 0, 1000.0f));
	streakGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Thickness", "Thickness of the shafts", this, MFPtr(SetThickness), MFPtr(GetThickness)));
	streakGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Thickness noise", "Noise strength of thickness variation", this, MFPtr(SetThicknessNoise), MFPtr(GetThicknessNoise)));
	streakGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Size noise", "Noise strength of shafts' sizes", this, MFPtr(SetSizeNoise), MFPtr(GetSizeNoise)));
	streakGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Spacing noise", "Noise strength of shafts' spacing", this, MFPtr(SetSpacingNoise), MFPtr(GetSpacingNoise)));
	groups.push_back(streakGroup);
}
	#undef MFPtr
#endif

Streaks::Streaks(const char* name)
	: COpticsElement(name, 0.5f)
	, m_fThickness(0.3f)
	, m_nNoiseSeed(81)
	, m_fSizeNoiseStrength(0.8f)
	, m_fThicknessNoiseStrength(0.6f)
	, m_fSpacingNoiseStrength(0.2f)
	, m_bUseSpectrumTex(false)
{
	m_vMovement.x = 1.f;
	m_vMovement.y = 1.f;
	m_Color.a = 1.f;

	SetAutoRotation(false);
	SetAspectRatioCorrection(true);
	SetColorComplexity(2);
	SetStreakCount(1);

	m_meshDirty = true;

	m_constantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SShaderParams), true, true);
	m_indexBuffer = ~0u;
}

Streaks::~Streaks()
{
	if (m_indexBuffer != ~0u)
		gcpRendD3D->m_DevBufMan.Destroy(m_indexBuffer);
}

void Streaks::Load(IXmlNode* pNode)
{
	COpticsElement::Load(pNode);

	XmlNodeRef pStreakNode = pNode->findChild("Streaks");

	if (pStreakNode)
	{
		bool bUseSpectrumTex(m_bUseSpectrumTex);
		if (pStreakNode->getAttr("Enablegradienttex", bUseSpectrumTex))
			SetEnableSpectrumTex(bUseSpectrumTex);

		const char* gradientTexName = NULL;
		if (pStreakNode->getAttr("GradientTex", &gradientTexName))
		{
			if (gradientTexName && gradientTexName[0])
			{
				ITexture* pTexture = std::move(gEnv->pRenderer->EF_LoadTexture(gradientTexName));
				SetSpectrumTex((CTexture*)pTexture);
			}
		}

		int nNoiseSeed(m_nNoiseSeed);
		if (pStreakNode->getAttr("Noiseseed", nNoiseSeed))
			SetNoiseSeed(nNoiseSeed);

		int nStreakCount(m_nStreakCount);
		if (pStreakNode->getAttr("Streakcount", nStreakCount))
			SetStreakCount(nStreakCount);

		float fThickness(m_fThickness);
		if (pStreakNode->getAttr("Thickness", fThickness))
			SetThickness(fThickness);

		float fThicknessNoiseStrength(m_fThicknessNoiseStrength);
		if (pStreakNode->getAttr("Thicknessnoise", fThicknessNoiseStrength))
			SetThicknessNoise(fThicknessNoiseStrength);

		float fSizeNoiseStrength(m_fSizeNoiseStrength);
		if (pStreakNode->getAttr("Sizenoise", fSizeNoiseStrength))
			SetSizeNoise(fSizeNoiseStrength);

		float fSpacingNoiseStrength(m_fSpacingNoiseStrength);
		if (pStreakNode->getAttr("Spacingnoise", fSpacingNoiseStrength))
			SetSpacingNoise(fSpacingNoiseStrength);
	}
}

CTexture* Streaks::GetTexture()
{
	if (m_bUseSpectrumTex)
	{
		if (m_pSpectrumTex == nullptr)
		{
			m_pSpectrumTex = std::move(CTexture::ForName("%ENGINE%/EngineAssets/Textures/flares/spectrum_full.tif", FT_DONT_STREAM, eTF_Unknown));
		}

		return m_pSpectrumTex;
	}

	return CRendererResources::s_ptexBlack;
}

void Streaks::UpdateMeshes()
{
	stable_rand::setSeed((int)m_nNoiseSeed);
	float dirDelta = 1.0f / (float)(m_separatedMeshList.size());

	// each sub mesh has its own vertex buffer
	for (uint32 i = 0; i < m_separatedMeshList.size(); i++)
	{
		float spacingNoise = 1 + stable_rand::randUnit() * m_fSpacingNoiseStrength;

		float dirUnit = (i * dirDelta + spacingNoise);
		float dir = dirUnit;

		float randRadius = (1 + stable_rand::randUnit() * m_fSizeNoiseStrength);
		float thickness = m_fThickness * (1 + stable_rand::randUnit() * m_fThicknessNoiseStrength);

		auto& mesh = m_separatedMeshList[i];
		MeshUtil::GenStreak(dir, randRadius, thickness, Clr_Neutral, mesh.GetVertices(), m_meshIndices);

		mesh.MarkDirty();
		mesh.ValidateMesh();
		mesh.primitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, m_constantBuffer, EShaderStage_Vertex | EShaderStage_Pixel);
	}

	// index buffer is shared
	if (m_indexBuffer != ~0u)
		gcpRendD3D->m_DevBufMan.Destroy(m_indexBuffer);

	m_indexBuffer = gcpRendD3D->m_DevBufMan.Create(BBT_INDEX_BUFFER, BU_STATIC, m_meshIndices.size() * sizeof(uint16));
	gcpRendD3D->m_DevBufMan.UpdateBuffer(m_indexBuffer, &m_meshIndices[0], m_meshIndices.size() * sizeof(uint16));

	m_meshDirty = false;
}

bool Streaks::PreparePrimitives(const SPreparePrimitivesContext& context)
{
	if (!IsVisible() || m_separatedMeshList.empty())
		return true;

	// update meshes if necessary
	if (m_meshDirty)
		UpdateMeshes();

	// update shared constant buffer
	{
		auto& firstPrimitive = m_separatedMeshList.front().primitive;
		auto constants = firstPrimitive.GetConstantManager().BeginTypedConstantUpdate<SShaderParams>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);

		for (int i = 0; i < context.viewInfoCount; ++i)
		{
			ApplyCommonParams(constants, context.pViewInfo->viewport, context.lightScreenPos[i], Vec2(m_globalSize));

			Vec3 screenPos = computeOrbitPos(context.lightScreenPos[i], m_globalOrbitAngle);
			constants->meshCenterAndBrt[0] = computeMovementLocationX(screenPos);
			constants->meshCenterAndBrt[1] = computeMovementLocationY(screenPos);
			constants->meshCenterAndBrt[2] = context.lightScreenPos[i].z;
			constants->meshCenterAndBrt[3] = 1.0f;

			if (i < context.viewInfoCount - 1)
				constants.BeginStereoOverride(false);
		}

		firstPrimitive.GetConstantManager().EndTypedConstantUpdate(constants);
	}

	uint64 rtFlags = 0;
	ApplyGeneralFlags(rtFlags);
	ApplySpectrumTexFlag(rtFlags, m_bUseSpectrumTex);

	for (auto& mesh : m_separatedMeshList)
	{
		static CCryNameTSCRC techName("Streaks");

		auto& prim = mesh.primitive;
		prim.SetTechnique(CShaderMan::s_ShaderLensOptics, techName, rtFlags);
		prim.SetRenderState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);
		prim.SetTexture(0, GetTexture());
		prim.SetSampler(0, EDefaultSamplerStates::LinearBorder_Black);

		prim.SetCustomVertexStream(mesh.GetVertexBuffer(), EDefaultInputLayouts::P3F_C4B_T2F, sizeof(SVF_P3F_C4B_T2F));
		prim.SetCustomIndexStream(m_indexBuffer, Index16);
		prim.SetDrawInfo(eptTriangleList, 0, 0, m_meshIndices.size());
		prim.Compile(context.pass);

		context.pass.AddPrimitive(&prim);
	}

	return true;
}
