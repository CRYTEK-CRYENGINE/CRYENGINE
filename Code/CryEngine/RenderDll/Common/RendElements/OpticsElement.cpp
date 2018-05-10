// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OpticsElement.h"
#include "../CryNameR.h"
#include "../../XRenderD3D9/DriverD3D.h"
#include "RootOpticsElement.h"
#include "../Textures/Texture.h"
#include "FlareSoftOcclusionQuery.h"

#if defined(FLARES_SUPPORT_EDITING)
DynArray<FuncVariableGroup> COpticsElement::GetEditorParamGroups()
{
	if (paramGroups.empty())
		InitEditorParamGroups(paramGroups);

	return paramGroups;
}

	#define MFPtr(FUNC_NAME) (Optics_MFPtr)(&COpticsElement::FUNC_NAME)
void COpticsElement::InitEditorParamGroups(DynArray<FuncVariableGroup>& groups)
{
	FuncVariableGroup baseGroup;
	baseGroup.SetName("Common");
	baseGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Size", "Size", this, MFPtr(SetSize), MFPtr(GetSize)));
	baseGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Perspective factor", "Perspective factor", this, MFPtr(SetPerspectiveFactor), MFPtr(GetPerspectiveFactor)));
	baseGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Distance Fading factor", "Perspective factor", this, MFPtr(SetDistanceFadingFactor), MFPtr(GetDistanceFadingFactor)));
	baseGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Brightness", "Brightness", this, MFPtr(SetBrightness), MFPtr(GetBrightness)));
	baseGroup.AddVariable(new OpticsMFPVariable(e_VEC2, "Position", "The relative position to light", this, MFPtr(SetMovement), MFPtr(GetMovement)));
	baseGroup.AddVariable(new OpticsMFPVariable(e_COLOR, "Tint", "Basic tint", this, MFPtr(SetColor), MFPtr(GetColor)));
	baseGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Orbit angle", "The rotation angle of the virtual light source", this, MFPtr(SetOrbitAngle), MFPtr(GetOrbitAngle)));
	baseGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Occlusion Interaction", "Enable various flare interaction with the occlusion", this, MFPtr(SetOccBokehEnabled), MFPtr(IsOccBokehEnabled)));
	baseGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Auto Rotation", "Enable Auto Rotation around the pivot", this, MFPtr(SetAutoRotation), MFPtr(HasAutoRotation)));
	baseGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Correct Aspect Ratio", "Correct aspect ratio", this, MFPtr(SetAspectRatioCorrection), MFPtr(HasAspectRatioCorrection)));
	groups.push_back(baseGroup);

	FuncVariableGroup sensorGroup;
	sensorGroup.SetName("Sensor");
	sensorGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Sensor image size variation factor", "sensor image size variation factor", this, MFPtr(SetSensorSizeFactor), MFPtr(GetSensorSizeFactor)));
	sensorGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Sensor image brightness variation factor", "sensor image brightness variation factor", this, MFPtr(SetSensorBrightnessFactor), MFPtr(GetSensorBrightnessFactor)));
	groups.push_back(sensorGroup);

	FuncVariableGroup xformGroup;
	xformGroup.SetName("Transformation");
	xformGroup.AddVariable(new OpticsMFPVariable(e_VEC2, "Scale", "Scale", this, MFPtr(SetScale), MFPtr(GetScale)));
	xformGroup.AddVariable(new OpticsMFPVariable(e_VEC2, "Translation", "Translation", this, MFPtr(SetTranslation), MFPtr(GetTranslation)));
	xformGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Rotation", "Rotation", this, MFPtr(SetRotation), MFPtr(GetRotation)));
	groups.push_back(xformGroup);

	FuncVariableGroup dynamicsGroup;
	dynamicsGroup.SetName("Dynamics");
	dynamicsGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Enable", "Enable.", this, MFPtr(SetDynamicsEnabled), MFPtr(GetDynamicsEnabled)));
	dynamicsGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Trigger Invert", "Invert the trigger area.", this, MFPtr(SetDynamicsInvert), MFPtr(GetDynamicsInvert)));
	dynamicsGroup.AddVariable(new OpticsMFPVariable(e_VEC2, "Trigger Offset", "Offset from the center of the screen.", this, MFPtr(SetDynamicsOffset), MFPtr(GetDynamicsOffset)));
	dynamicsGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Trigger Range", "How much influence the trigger has.", this, MFPtr(SetDynamicsRange), MFPtr(GetDynamicsRange)));
	dynamicsGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Trigger Falloff", "Falloff strength of the trigger area.", this, MFPtr(SetDynamicsFalloff), MFPtr(GetDynamicsFalloff)));
	groups.push_back(dynamicsGroup);
}
	#undef MFPtr
#endif

COpticsElement::COpticsElement(const char* name, float size, const float brightness, const ColorF& color)
	: xform_scale(1.0f, 1.0f)
	, xform_translate(ZERO)
	, xform_rotation(0.0f)
	, m_pParent(0)
	, m_globalMovement(LensOpConst::_LO_DEF_VEC2_I)
	, m_globalTransform(IDENTITY)
	, m_globalColor(1.0f, 1.0f, 1.0f, 1.0f)
	, m_globalFlareBrightness(1.0f)
	, m_globalShaftBrightness(1.0f)
	, m_globalSize(1.0f)
	, m_globalPerspectiveFactor(1.0f)
	, m_globalDistanceFadingFactor(0.0f)
	, m_globalOrbitAngle(0.0f)
	, m_globalSensorSizeFactor(0.0f)
	, m_globalSensorBrightnessFactor(0.0f)
	, m_globalAutoRotation(false)
	, m_globalCorrectAspectRatio(false)
	, m_globalOcclusionBokeh(false)
	, m_bEnabled(true)
	, m_name(name)
	, m_mxTransform(LensOpConst::_LO_DEF_MX33)
	, m_fSize(size)
	, m_fPerspectiveFactor(1.0f)
	, m_fDistanceFadingFactor(1.0f)
	, m_Color(color)
	, m_fBrightness(brightness)
	, m_vMovement(1.0f, 1.0f)
	, m_fOrbitAngle(0.0f)
	, m_fSensorSizeFactor(0.0f)
	, m_fSensorBrightnessFactor(0.0f)
	, m_vDynamicsOffset(ZERO)
	, m_fDynamicsRange(1.0f)
	, m_fDynamicsFalloff(1.0f)
	, m_bAutoRotation(false)
	, m_bCorrectAspectRatio(true)
	, m_bOcclusionBokeh(false)
	, m_bDynamics(false)
	, m_bDynamicsInvert(false)
{
}

void COpticsElement::Load(IXmlNode* pNode)
{
	XmlNodeRef pCommonNode = pNode->findChild("Common");
	if (pCommonNode)
	{
		float fSize(m_fSize);
		if (pCommonNode->getAttr("Size", fSize))
			SetSize(fSize);

		float fPerspectiveFactor(m_fPerspectiveFactor);
		if (pCommonNode->getAttr("Perspectivefactor", fPerspectiveFactor))
			SetPerspectiveFactor(fPerspectiveFactor);

		float fDistanceFadingFactor = m_fDistanceFadingFactor;
		if (pCommonNode->getAttr("DistanceFadingfactor", fDistanceFadingFactor))
			SetDistanceFadingFactor(fDistanceFadingFactor);

		float fBrightness = m_fBrightness;
		if (pCommonNode->getAttr("Brightness", fBrightness))
			SetBrightness(fBrightness);

		Vec2 vPos(m_vMovement);
		if (pCommonNode->getAttr("Position", vPos))
			SetMovement(vPos);

		Vec3 color(m_Color.r, m_Color.g, m_Color.b);
		int nAlpha((int)(m_Color.a * 255.0f));
		if (pCommonNode->getAttr("Tint", color) && pCommonNode->getAttr("Tint.alpha", nAlpha))
			SetColor(ColorF(color.x, color.y, color.z, (float)nAlpha / 255.0f));

		float fOrbitAngle(m_fOrbitAngle);
		if (pCommonNode->getAttr("Orbitangle", fOrbitAngle))
			SetOrbitAngle(fOrbitAngle);

		bool bOcclusionBokeh(m_bOcclusionBokeh);
		if (pCommonNode->getAttr("OcclusionInteraction", bOcclusionBokeh))
			SetOccBokehEnabled(bOcclusionBokeh);

		bool bAutoRotation(m_bAutoRotation);
		if (pCommonNode->getAttr("AutoRotation", bAutoRotation))
			SetAutoRotation(bAutoRotation);

		bool bCorrectAspectRatio(m_bCorrectAspectRatio);
		if (pCommonNode->getAttr("CorrectAspectRatio", bCorrectAspectRatio))
			SetAspectRatioCorrection(bCorrectAspectRatio);
	}

	XmlNodeRef pSensorNode = pNode->findChild("Sensor");
	if (pSensorNode)
	{
		float fSensorSizeFactor(m_fSensorSizeFactor);
		if (pSensorNode->getAttr("Sensorimagesizevariationfactor", fSensorSizeFactor))
			SetSensorSizeFactor(fSensorSizeFactor);

		float fSensorBrightnessFactor = m_fSensorBrightnessFactor;
		if (pSensorNode->getAttr("Sensorimagebrightnessvariationfactor", fSensorBrightnessFactor))
			SetSensorBrightnessFactor(fSensorBrightnessFactor);
	}

	XmlNodeRef pTransformationNode = pNode->findChild("Transformation");
	if (pTransformationNode)
	{
		Vec2 vScale(xform_scale);
		if (pTransformationNode->getAttr("Scale", vScale))
			SetScale(vScale);

		Vec2 vTranslation(xform_translate);
		if (pTransformationNode->getAttr("Translation", vTranslation))
			SetTranslation(vTranslation);

		float fRotation(xform_rotation);
		if (pTransformationNode->getAttr("Rotation", fRotation))
			SetRotation(fRotation);
	}

	XmlNodeRef pDynamicsNode = pNode->findChild("Dynamics");
	if (pDynamicsNode)
	{
		bool bEnable(m_bEnabled);
		if (pDynamicsNode->getAttr("Enable", bEnable))
			SetDynamicsEnabled(bEnable);

		bool bDynamicInvert(m_bDynamicsInvert);
		if (pDynamicsNode->getAttr("TriggerInvert", bDynamicInvert))
			SetDynamicsInvert(bDynamicInvert);

		Vec2 vDynamicsOffset(m_vDynamicsOffset);
		if (pDynamicsNode->getAttr("TriggerOffset", vDynamicsOffset))
			SetDynamicsOffset(vDynamicsOffset);

		float fDynamicsRange(m_fDynamicsRange);
		if (pDynamicsNode->getAttr("TriggerRange", fDynamicsRange))
			SetDynamicsRange(fDynamicsRange);

		float fDynamicsFalloff(m_fDynamicsFalloff);
		if (pDynamicsNode->getAttr("TriggerFalloff", fDynamicsFalloff))
			SetDynamicsFalloff(fDynamicsFalloff);
	}
}

RootOpticsElement* COpticsElement::GetRoot()
{
	IOpticsElementBase* parent = this;
	do
	{
		parent = parent->GetParent();
		if (parent == NULL)
			return NULL;
	}
	while (parent->GetType() != eFT_Root);
	return (RootOpticsElement*)parent;
}

void COpticsElement::validateGlobalVars(const SAuxParams& aux)
{
	if (m_pParent)
	{
		m_globalPerspectiveFactor = m_fPerspectiveFactor * m_pParent->m_globalPerspectiveFactor;
		m_globalDistanceFadingFactor = m_fDistanceFadingFactor * m_pParent->m_globalDistanceFadingFactor;
		m_globalSensorSizeFactor = m_fSensorSizeFactor * m_pParent->m_globalSensorSizeFactor;
		float sensorSizeFactor = 1 + m_globalSensorSizeFactor * aux.sensorVariationValue;

		m_globalSensorBrightnessFactor = m_fSensorBrightnessFactor * m_pParent->m_globalSensorBrightnessFactor;
		float sensorBrightnessFactor = fabs(1 + m_globalSensorBrightnessFactor * aux.sensorVariationValue);

		float perspectiveShortening = 1 + m_globalPerspectiveFactor * (aux.perspectiveShortening - 1);
		m_globalSize = m_fSize * m_pParent->m_globalSize * perspectiveShortening * sensorSizeFactor;  // remember to multiply the additional perspective factor

		if (aux.bMultiplyColor)
		{
			m_globalColor = m_Color * m_pParent->m_globalColor;
		}
		else
		{
			m_globalColor = ColorF(m_Color.r + m_pParent->m_globalColor.r,
			                       m_Color.g + m_pParent->m_globalColor.g,
			                       m_Color.b + m_pParent->m_globalColor.b,
			                       m_Color.a * m_pParent->m_globalColor.a);
		}

		float fading = clamp_tpl(1 - 1000.f * m_globalDistanceFadingFactor * aux.linearDepth, 0.0f, 1.f);
		float brightness = m_fBrightness * sensorBrightnessFactor * fading;
		m_globalFlareBrightness = brightness * m_pParent->m_globalFlareBrightness;
		m_globalShaftBrightness = brightness * m_pParent->m_globalShaftBrightness;

		m_globalMovement.x = m_vMovement.x * m_pParent->m_globalMovement.x;
		m_globalMovement.y = m_vMovement.y * m_pParent->m_globalMovement.y;

		if (!m_pParent->m_globalTransform.IsIdentity())
			m_globalTransform = m_mxTransform * m_pParent->m_globalTransform;
		else
			m_globalTransform = m_mxTransform;

		m_globalOrbitAngle = m_fOrbitAngle + m_pParent->m_globalOrbitAngle;
		m_globalOcclusionBokeh = m_bOcclusionBokeh & m_pParent->m_globalOcclusionBokeh;
		m_globalAutoRotation = m_bAutoRotation & m_pParent->m_bAutoRotation;
		m_globalCorrectAspectRatio = m_bCorrectAspectRatio & m_pParent->m_bCorrectAspectRatio;
	}
}

void COpticsElement::updateXformMatrix()
{
	Matrix33 scaleMx;
	scaleMx.SetScale(Vec3(xform_scale.x, xform_scale.y, 1));
	Matrix33 rotMx;
	rotMx.SetRotationAA(xform_rotation * 2.0f * PI, Vec3(0, 0, 1));
	Matrix33 combine = rotMx * scaleMx;
	combine.SetColumn(2, Vec3(xform_translate.x, xform_translate.y, 1));
	SetTransform(combine);
}

//////////////////////////////////////////////////////////////////////////
void COpticsElement::DeleteThis()
{
	gRenDev->ExecuteRenderThreadCommand( [=] { delete this; },ERenderCommandFlags::LevelLoadingThread_defer );
}

const Vec3 COpticsElement::computeOrbitPos(const Vec3& vSrcProjPos, float orbitAngle)
{
	if (orbitAngle < 0.01f && orbitAngle > -0.01f)
		return vSrcProjPos;

	const Vec2 oriVec(vSrcProjPos.x - 0.5f, vSrcProjPos.y - 0.5f);
	float orbitSin = 0.0f, orbitCos = 0.0f;
	sincos_tpl(orbitAngle, &orbitSin, &orbitCos);
	const Vec2 resultVec = Vec2(oriVec.x * orbitCos - oriVec.y * orbitSin, oriVec.y * orbitCos + oriVec.x * orbitSin);
	return Vec3(resultVec.x + 0.5f, resultVec.y + 0.5f, vSrcProjPos.z);
}

void COpticsElement::ApplyOcclusionPattern(SShaderParamsBase& shaderParams, CRenderPrimitive& primitive)
{
	if (GetRoot() == NULL)
		return;

	CFlareSoftOcclusionQuery* pSoftOcclusionQuery = GetRoot()->GetOcclusionQuery();
	if (pSoftOcclusionQuery && pSoftOcclusionQuery->GetGatherTexture())
	{
		CTexture* pGatherTex = pSoftOcclusionQuery->GetGatherTexture();

		float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
		pSoftOcclusionQuery->GetDomainInTexture(x0, y0, x1, y1);
		float width, height;
		pSoftOcclusionQuery->GetSectorSize(width, height);

		shaderParams.occPatternInfo = Vec4((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, width, height);
		primitive.SetTexture(5, pGatherTex);
		primitive.SetSampler(5, EDefaultSamplerStates::LinearClamp);
	}
	else
	{
		shaderParams.occPatternInfo = Vec4(ZERO);
		primitive.SetTexture(5, CRendererResources::s_ptexBlack);
		primitive.SetSampler(5, EDefaultSamplerStates::LinearClamp);
	}
}

void COpticsElement::ApplyOcclusionBokehFlag(uint64& rtFlags)
{
	if (m_globalOcclusionBokeh)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE3];
}

void COpticsElement::ApplySpectrumTexFlag(uint64& rtFlags, bool enabled)
{
	if (enabled)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE2];
}

void COpticsElement::ApplyGeneralFlags(uint64& rtFlags)
{
	if (m_globalAutoRotation)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE0];
	if (m_globalCorrectAspectRatio)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE5];
}

void COpticsElement::ApplyCommonParams(SShaderParamsBase& shaderParams, const SRenderViewport& viewport, const Vec3& lightProjPos, const Vec2& size)
{
	shaderParams.outputDimAndSize = Vec4(float(viewport.width), float(viewport.height), size.x, size.y);
	shaderParams.xform = Matrix34(m_globalTransform.GetTransposed());

	// dynamics
	float fTriggerArea = 1.0f;
	if (m_bDynamics)
	{
		float fRange = 1.0f / max(0.01f, m_fDynamicsRange);
		float fFalloff = m_fDynamicsFalloff;

		Vec2 vProjPos;
		vProjPos.x = (lightProjPos.x * 2.f - 1.f) + m_vDynamicsOffset.x;
		vProjPos.y = (lightProjPos.y * 2.f - 1.f) + m_vDynamicsOffset.y;

		fTriggerArea = vProjPos.GetLength();
		fTriggerArea = m_bDynamicsInvert ? 1.0f - fTriggerArea : fTriggerArea;
		fTriggerArea = powf(clamp_tpl(1.f - fTriggerArea * fRange, 0.f, 1.f), fFalloff);
	}

	shaderParams.dynamics = Vec4(fTriggerArea, 1, 1, 1);

	
	Vec4 vHDRSetupParams[5];
	gEnv->p3DEngine->GetHDRSetupParams(vHDRSetupParams);

	shaderParams.hdrParams[0] = vHDRSetupParams[0].x * 6.2f;
	shaderParams.hdrParams[1] = vHDRSetupParams[0].y * 0.5f;
	shaderParams.hdrParams[2] = vHDRSetupParams[0].z * 0.06f;
	shaderParams.hdrParams[3] = 1.0f;

	ColorF c = m_globalColor;
	c.ScaleCol(m_globalFlareBrightness);
	shaderParams.externTint = c.toVec4();
}


void COpticsElement::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
}
