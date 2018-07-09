// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ChromaticRing.h"
#include "../Textures/Texture.h"
#include "../CryNameR.h"
#include "../../RenderDll/XRenderD3D9/DriverD3D.h"

#if defined(FLARES_SUPPORT_EDITING)
	#define MFPtr(FUNC_NAME) (Optics_MFPtr)(&ChromaticRing::FUNC_NAME)
void ChromaticRing::InitEditorParamGroups(DynArray<FuncVariableGroup>& groups)
{
	COpticsElement::InitEditorParamGroups(groups);

	FuncVariableGroup crGroup;
	crGroup.SetName("ChromaticRing", "Chromatic Ring");
	crGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Lock to light", "Lock to light", this, MFPtr(SetLockMovement), MFPtr(IsLockMovement)));
	crGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Thickness", "Thickness", this, MFPtr(SetWidth), MFPtr(GetWidth)));

	crGroup.AddVariable(new OpticsMFPVariable(e_INT, "Polygon complexity", "Polygon complexity", this, MFPtr(SetPolyComplexity), MFPtr(GetPolyComplexity), 0, 1024.0f));
	crGroup.AddVariable(new OpticsMFPVariable(e_TEXTURE2D, "Gradient Texture", "Gradient Texture", this, MFPtr(SetSpectrumTex), MFPtr(GetSpectrumTex)));
	crGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Enable Gradient Texture", "Enable Gradient Texture", this, MFPtr(SetUsingSpectrumTex), MFPtr(IsUsingSpectrumTex)));

	crGroup.AddVariable(new OpticsMFPVariable(e_INT, "Noise seed", "Noise seed", this, MFPtr(SetNoiseSeed), MFPtr(GetNoiseSeed), -255.0f, 255.0f));
	crGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Noise strength", "Noise strength", this, MFPtr(SetNoiseStrength), MFPtr(GetNoiseStrength)));
	crGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Completion fading", "the fading ratio at the ends of this arc", this, MFPtr(SetCompletionFading), MFPtr(GetCompletionFading)));
	crGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Completion span angle", "The span of this arc in degree", this, MFPtr(SetCompletionSpanAngle), MFPtr(GetCompletionSpanAngle)));
	crGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Completion rotation", "The rotation of this arc", this, MFPtr(SetCompletionRotation), MFPtr(GetCompletionRotation)));

	groups.push_back(crGroup);
}
	#undef MFPtr
#endif

ChromaticRing::ChromaticRing(const char* name)
	: COpticsElement(name)
	, m_bUseSpectrumTex(false)
	, m_fWidth(0.5f)
	, m_nPolyComplexity(160)
	, m_nColorComplexity(2)
	, m_fNoiseStrength(0.0f)
	, m_fCompletionStart(90.f)
	, m_fCompletionEnd(270.f)
	, m_fCompletionFading(45.f)
{
	SetSize(0.9f);
	SetAutoRotation(true);
	SetAspectRatioCorrection(false);

	m_meshDirty = true;

	CConstantBufferPtr pSharedCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SShaderParams), true, true);
	m_primitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pSharedCB, EShaderStage_Vertex | EShaderStage_Pixel);
	m_wireframePrimitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pSharedCB, EShaderStage_Vertex | EShaderStage_Pixel);
}

void ChromaticRing::Load(IXmlNode* pNode)
{
	COpticsElement::Load(pNode);

	XmlNodeRef pCameraOrbsNode = pNode->findChild("ChromaticRing");
	if (pCameraOrbsNode)
	{
		bool bLockMovement(m_bLockMovement);
		if (pCameraOrbsNode->getAttr("Locktolight", bLockMovement))
			SetLockMovement(bLockMovement);

		float fWidth(m_fWidth);
		if (pCameraOrbsNode->getAttr("Thickness", fWidth))
			SetWidth(fWidth);

		int nPolyComplexity(m_nPolyComplexity);
		if (pCameraOrbsNode->getAttr("Polygoncomplexity", nPolyComplexity))
			SetPolyComplexity(nPolyComplexity);

		const char* gradientTexName = NULL;
		if (pCameraOrbsNode->getAttr("GradientTexture", &gradientTexName))
		{
			if (gradientTexName && gradientTexName[0])
			{
				ITexture* pTexture = gEnv->pRenderer->EF_LoadTexture(gradientTexName);
				SetSpectrumTex((CTexture*)pTexture);
				if (pTexture)
					pTexture->Release();
			}
		}

		bool bUseSpectrumTex(m_bUseSpectrumTex);
		if (pCameraOrbsNode->getAttr("EnableGradientTexture", bUseSpectrumTex))
			SetUsingSpectrumTex(bUseSpectrumTex);

		int nNoiseSeed(m_nNoiseSeed);
		if (pCameraOrbsNode->getAttr("Noiseseed", nNoiseSeed))
			SetNoiseSeed(nNoiseSeed);

		float fNoiseStrength(m_fNoiseStrength);
		if (pCameraOrbsNode->getAttr("Noisestrength", fNoiseStrength))
			SetNoiseStrength(fNoiseStrength);

		float fCompletionFading(m_fCompletionFading);
		if (pCameraOrbsNode->getAttr("Completionfading", fCompletionFading))
			SetCompletionFading(fCompletionFading);

		float fTotalAngle(0);
		if (pCameraOrbsNode->getAttr("Completionspanangle", fTotalAngle))
			SetCompletionSpanAngle(fTotalAngle);

		float fCompletionRotation(0);
		if (pCameraOrbsNode->getAttr("Completionrotation", fCompletionRotation))
			SetCompletionRotation(fCompletionRotation);
	}
}

float ChromaticRing::computeDynamicSize(const Vec3& vSrcProjPos, const float maxSize)
{
	Vec2 dir(vSrcProjPos.x - 0.5f, vSrcProjPos.y - 0.5f);
	float len = dir.GetLength();
	const float hoopDistFactor = 2.3f;
	return len * hoopDistFactor * maxSize;
}

CTexture* ChromaticRing::GetOrLoadSpectrumTex()
{
	if (m_pSpectrumTex == nullptr)
	{
		m_pSpectrumTex = std::move(CTexture::ForName("%ENGINE%/EngineAssets/Textures/flares/spectrum_full.tif", FT_DONT_STREAM, eTF_Unknown));
	}

	return m_pSpectrumTex;
}

bool ChromaticRing::PreparePrimitives(const SPreparePrimitivesContext& context)
{
	if (!IsVisible())
		return true;

	// get rt flags
	uint64 rtFlags = 0;
	ApplyGeneralFlags(rtFlags);
	ApplySpectrumTexFlag(rtFlags, m_bUseSpectrumTex);

	// update constants (constant buffer is shared by both primitives, so we only have to update it once)
	{
		auto constants = m_primitive.GetConstantManager().BeginTypedConstantUpdate<SShaderParams>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);

		for (int i = 0; i < context.viewInfoCount; ++i)
		{
			Vec3 screenPos = computeOrbitPos(context.lightScreenPos[i], m_globalOrbitAngle);
			float newSize  = computeDynamicSize(screenPos, m_globalSize);

			ApplyCommonParams(constants, context.pViewInfo->viewport, context.lightScreenPos[0], Vec2(newSize));

			constants->meshCenterAndBrt[0] = m_bLockMovement ? screenPos.x : computeMovementLocationX(screenPos);
			constants->meshCenterAndBrt[1] = m_bLockMovement ? screenPos.y : computeMovementLocationY(screenPos);
			constants->meshCenterAndBrt[2] = screenPos.z;
			constants->meshCenterAndBrt[3] = m_globalFlareBrightness;

			if (i < context.viewInfoCount - 1)
				constants.BeginStereoOverride(false);
		}

		m_primitive.GetConstantManager().EndTypedConstantUpdate(constants);
	}

	// update mesh
	ValidateMesh();

	// now prepare primitives
	CRenderPrimitive* pPrimitives[] = { &m_primitive, &m_wireframePrimitive };

	for (int i = 0; i < CRY_ARRAY_COUNT(pPrimitives); ++i)
	{
		static CCryNameTSCRC techChromaticRing("ChromaticRing");

		auto& prim = *pPrimitives[i];
		prim.SetTechnique(CShaderMan::s_ShaderLensOptics, techChromaticRing, rtFlags);
		prim.SetRenderState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE | (i==0 ? 0 : GS_WIREFRAME));
		prim.SetTexture(0, GetOrLoadSpectrumTex());
		prim.SetSampler(0, EDefaultSamplerStates::LinearBorder_Black);
		prim.SetCustomVertexStream(m_vertexBuffer, EDefaultInputLayouts::P3F_C4B_T2F, sizeof(SVF_P3F_C4B_T2F));
		prim.SetCustomIndexStream(m_indexBuffer, Index16);
		prim.SetDrawInfo(eptTriangleList, 0, 0, GetIndexCount());
		prim.Compile(context.pass);

		context.pass.AddPrimitive(&prim);
	}

	return true;
}
