// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Glow.h"
#include "../Textures/Texture.h"
#include "../CryNameR.h"
#include "../../RenderDll/XRenderD3D9/DriverD3D.h"

#if defined(FLARES_SUPPORT_EDITING)
	#define MFPtr(FUNC_NAME) (Optics_MFPtr)(&Glow::FUNC_NAME)
void Glow::InitEditorParamGroups(DynArray<FuncVariableGroup>& groups)
{
	COpticsElement::InitEditorParamGroups(groups);

	FuncVariableGroup glowGroup;
	glowGroup.SetName("Glow");
	glowGroup.AddVariable(new OpticsMFPVariable(e_INT, "Polygon factor", "Polygons factor", this, MFPtr(SetPolygonFactor), MFPtr(GetPolygonFactor), 0, 128.0f));
	glowGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Focus factor", "Focus factor", this, MFPtr(SetFocusFactor), MFPtr(GetFocusFactor)));
	glowGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Gamma", "Gamma", this, MFPtr(SetGamma), MFPtr(GetGamma)));
	groups.push_back(glowGroup);
}
	#undef MFPtr
#endif

Glow::Glow(const char* name)
	: COpticsElement(name)
	, m_fFocusFactor(0.3f)
	, m_fPolyonFactor(32.f)
	, m_fGamma(1)
{
	CConstantBufferPtr pcb = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SShaderParams), true, true);

	m_primitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pcb, EShaderStage_Vertex | EShaderStage_Pixel);
}

void Glow::Load(IXmlNode* pNode)
{
	COpticsElement::Load(pNode);

	XmlNodeRef pGlowNode = pNode->findChild("Glow");
	if (pGlowNode)
	{
		int nPolygonFactor(0);
		if (pGlowNode->getAttr("Polygonfactor", nPolygonFactor))
			SetPolygonFactor(nPolygonFactor);

		float fFocusFactor(m_fFocusFactor);
		if (pGlowNode->getAttr("Focusfactor", fFocusFactor))
			SetFocusFactor(fFocusFactor);

		float fGamma(m_fGamma);
		if (pGlowNode->getAttr("Gamma", fGamma))
			SetGamma(fGamma);
	}
}

void Glow::GenMesh()
{
	float ringPos = 1;
	MeshUtil::GenDisk(m_fSize, (int)m_fPolyonFactor, 1, true, m_globalColor, &ringPos, m_vertices, m_indices);
}

bool Glow::PreparePrimitives(const SPreparePrimitivesContext& context)
{
	if (!IsVisible())
		return true;

	static CCryNameTSCRC techName("Glow");

	uint64 rtFlags = 0;
	ApplyGeneralFlags(rtFlags);

	m_primitive.SetTechnique(CShaderMan::s_ShaderLensOptics, techName, rtFlags);
	m_primitive.SetRenderState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);

	// update constants
	{
		auto constants = m_primitive.GetConstantManager().BeginTypedConstantUpdate<SShaderParams>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);

		for (int i = 0; i < context.viewInfoCount; ++i)
		{
			ApplyCommonParams(constants, context.pViewInfo->viewport, context.lightScreenPos[i], Vec2(m_globalSize));

			constants->lumaParams = Vec4(m_fFocusFactor, m_fGamma, 0, 0);
			constants->meshCenterAndBrt[0] = computeMovementLocationX(context.lightScreenPos[i]);
			constants->meshCenterAndBrt[1] = computeMovementLocationY(context.lightScreenPos[i]);
			constants->meshCenterAndBrt[2] = context.lightScreenPos[i].z;
			constants->meshCenterAndBrt[3] = 1.0f;

			if (i < context.viewInfoCount - 1)
				constants.BeginStereoOverride(false);
		}

		m_primitive.GetConstantManager().EndTypedConstantUpdate(constants);
	}

	ValidateMesh();

	m_primitive.SetCustomVertexStream(m_vertexBuffer, EDefaultInputLayouts::P3F_C4B_T2F, sizeof(SVF_P3F_C4B_T2F));
	m_primitive.SetCustomIndexStream(m_indexBuffer, Index16);
	m_primitive.SetDrawInfo(eptTriangleList, 0, 0, GetIndexCount());
	m_primitive.Compile(context.pass);

	context.pass.AddPrimitive(&m_primitive);

	return true;
}
