// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "RootOpticsElement.h"
#include "FlareSoftOcclusionQuery.h"
#include "../Textures/Texture.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "stdarg.h"

#include <Common/RenderDisplayContext.h>

enum EVisFader
{
	VISFADER_FLARE = 0,
	VISFADER_SHAFT,
	VISFADER_NUM
};

const float RootOpticsElement::kExtendedFlareRadiusRatio = 1.3f;

#if defined(FLARES_SUPPORT_EDITING)
	#define MFPtr(FUNC_NAME) (Optics_MFPtr)(&RootOpticsElement::FUNC_NAME)
void RootOpticsElement::InitEditorParamGroups(DynArray<FuncVariableGroup>& groups)
{
	COpticsGroup::InitEditorParamGroups(groups);

	FuncVariableGroup rootGroup;
	rootGroup.SetName("GlobalSettings", "Global Settings");
	rootGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Enable Occlusion", "Enable Occlusion", this, MFPtr(SetOcclusionEnabled), MFPtr(IsOcclusionEnabled)));
	rootGroup.AddVariable(new OpticsMFPVariable(e_VEC2, "Occlusion Size", "The size for occlusion plane", this, MFPtr(SetOccSize), MFPtr(GetOccSize)));
	rootGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Enable Invert Fade", "Enable Invert Fade", this, MFPtr(SetInvertFade), MFPtr(IsInvertFade)));
	rootGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Flare fade time", "The duration of flare afterimage fading in seconds", this, MFPtr(SetFlareFadingDuration), MFPtr(GetFlareFadingDuration)));
	rootGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Shaft fade time", "The duration of shaft afterimage fading in seconds", this, MFPtr(SetShaftFadingDuration), MFPtr(GetShaftFadingDuration)));
	rootGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Affected by light color", "light color can affect flare color", this, MFPtr(SetAffectedByLightColor), MFPtr(IsAffectedByLightColor)));
	rootGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Affected by light radius", "light radius can affect flare fading", this, MFPtr(SetAffectedByLightRadius), MFPtr(IsAffectedByLightRadius)));
	rootGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Affected by light FOV", "light projection FOV can affect flare fading", this, MFPtr(SetAffectedByLightFOV), MFPtr(IsAffectedByLightFOV)));
	rootGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Multiply Color", "Select one of between Multiply and Addition about color calculation. If true, Multiply will be chosen.", this, MFPtr(SetMultiplyColor), MFPtr(IsMultiplyColor)));
	groups.push_back(rootGroup);

	FuncVariableGroup sensorGroup;
	sensorGroup.SetName("Sensor");
	sensorGroup.AddVariable(new OpticsMFPVariable(e_BOOL, "Custom Sensor Variation Map", "Enable Custom Sensor Variation Map", this, MFPtr(SetCustomSensorVariationMapEnabled), MFPtr(IsCustomSensorVariationMapEnabled)));
	sensorGroup.AddVariable(new OpticsMFPVariable(e_FLOAT, "Effective Sensor Size", "The size of image-able part of the sensor", this, MFPtr(SetEffectiveSensorSize), MFPtr(GetEffectiveSensorSize)));
	groups.push_back(sensorGroup);
}
	#undef MFPtr
#endif

void RootOpticsElement::Load(IXmlNode* pNode)
{
	COpticsElement::Load(pNode);

	XmlNodeRef pGloabalSettingsNode = pNode->findChild("GlobalSettings");
	if (pGloabalSettingsNode)
	{
		bool bOcclusionEnabled(m_bOcclusionEnabled);
		if (pGloabalSettingsNode->getAttr("EnableOcclusion", bOcclusionEnabled))
			SetOcclusionEnabled(bOcclusionEnabled);

		Vec2 occlusionSize(m_OcclusionSize);
		if (pGloabalSettingsNode->getAttr("OcclusionSize", occlusionSize))
			SetOccSize(occlusionSize);

		bool bInvertFade(m_bEnableInvertFade);
		if (pGloabalSettingsNode->getAttr("EnableInvertFade", bInvertFade))
			SetInvertFade(bInvertFade);

		float fFlareTimelineDuration(m_fFlareTimelineDuration);
		if (pGloabalSettingsNode->getAttr("Flarefadetime", fFlareTimelineDuration))
			SetFlareFadingDuration(fFlareTimelineDuration);

		float fShaftTimelineDuration(m_fShaftTimelineDuration);
		if (pGloabalSettingsNode->getAttr("Flarefadetime", fShaftTimelineDuration))
			SetShaftFadingDuration(fShaftTimelineDuration);

		bool bAffectedByLightColor(m_bAffectedByLightColor);
		if (pGloabalSettingsNode->getAttr("Affectedbylightcolor", bAffectedByLightColor))
			SetAffectedByLightColor(bAffectedByLightColor);

		bool bAffectedByLightRadius(m_bAffectedByLightRadius);
		if (pGloabalSettingsNode->getAttr("Affectedbylightradius", bAffectedByLightRadius))
			SetAffectedByLightRadius(bAffectedByLightRadius);

		bool bAffectedByLightFOV(m_bAffectedByLightFOV);
		if (pGloabalSettingsNode->getAttr("AffectedbylightFOV", bAffectedByLightFOV))
			SetAffectedByLightFOV(bAffectedByLightFOV);

		bool bMultiplyColor(m_bMultiplyColor);
		if (pGloabalSettingsNode->getAttr("MultiplyColor", bMultiplyColor))
			SetMultiplyColor(bMultiplyColor);
	}

	XmlNodeRef pSensorNode = pNode->findChild("Sensor");
	if (pSensorNode)
	{
		bool bCustomSensorVariationMap(m_bCustomSensorVariationMap);
		if (pSensorNode->getAttr("CustomSensorVariationMap", bCustomSensorVariationMap))
			SetCustomSensorVariationMapEnabled(bCustomSensorVariationMap);

		float fEffectiveSensorSize(m_fEffectiveSensorSize);
		if (pSensorNode->getAttr("EffectiveSensorSize", fEffectiveSensorSize))
			SetEffectiveSensorSize(fEffectiveSensorSize);
	}
}

void RootOpticsElement::SetOcclusionQuery(CFlareSoftOcclusionQuery* query)
{
	m_pOccQuery = query;
}

float RootOpticsElement::GetFlareVisibilityFactor() const
{
	CSoftOcclusionVisiblityFader* pFader = m_pOccQuery ? m_pOccQuery->GetVisibilityFader(VISFADER_FLARE) : NULL;
	return pFader ? pFader->m_fVisibilityFactor : 0.0f;
}

float RootOpticsElement::GetShaftVisibilityFactor() const
{
	CSoftOcclusionVisiblityFader* pFader = m_pOccQuery ? m_pOccQuery->GetVisibilityFader(VISFADER_SHAFT) : NULL;
	return pFader ? pFader->m_fVisibilityFactor : 0.0f;
}

void RootOpticsElement::SetVisibilityFactor(float f)
{
	f = clamp_tpl(f, 0.f, 1.f);

	if (m_pOccQuery)
	{
		for (uint i = 0; i < VISFADER_NUM; ++i)
		{
			if (CSoftOcclusionVisiblityFader* pFader = m_pOccQuery->GetVisibilityFader(i))
			{
				pFader->m_fVisibilityFactor = f;
			}
		}
	}
}

CTexture* RootOpticsElement::GetOcclusionPattern()
{
	return m_pOccQuery->GetGatherTexture();
}

void RootOpticsElement::validateGlobalVars(const SAuxParams& aux)
{
	m_globalPerspectiveFactor = m_fPerspectiveFactor;
	m_globalDistanceFadingFactor = m_flareLight.m_bAttachToSun ? 0.0f : m_fDistanceFadingFactor;
	m_globalSensorBrightnessFactor = m_fSensorBrightnessFactor;
	m_globalSensorSizeFactor = m_fSensorSizeFactor;
	m_globalSize = m_fSize;
	m_globalFlareBrightness = GetBrightness();
	m_globalMovement = m_vMovement;

	if (m_bAffectedByLightColor)
	{
		if (aux.bMultiplyColor)
			m_globalColor = ColorF(m_Color.r * m_flareLight.m_cLdrClr.r, m_Color.g * m_flareLight.m_cLdrClr.g, m_Color.b * m_flareLight.m_cLdrClr.b, m_Color.a);
		else
			m_globalColor = ColorF(m_Color.r + m_flareLight.m_cLdrClr.r, m_Color.g + m_flareLight.m_cLdrClr.g, m_Color.b + m_flareLight.m_cLdrClr.b, m_Color.a);

		m_globalFlareBrightness *= m_flareLight.m_fClrMultiplier;
	}
	else
	{
		m_globalColor = m_Color;
	}

	if (m_bAffectedByLightRadius)
	{
		if (m_bEnableInvertFade)
		{
			const float kRedundantRadiusRatio = kExtendedFlareRadiusRatio; //1.0f;
			const float fExtendedRadius = m_flareLight.m_fRadius * kRedundantRadiusRatio;

			if (aux.distance > m_flareLight.m_fRadius && aux.distance < fExtendedRadius)
			{
				const float fDistFromRadius = aux.distance - m_flareLight.m_fRadius;
				const float fRedundantLength = fExtendedRadius - m_flareLight.m_fRadius;
				m_globalFlareBrightness *= clamp_tpl(1.0f - fDistFromRadius / fRedundantLength, 0.0f, 1.0f);
			}
			else
			{
				m_globalFlareBrightness *= clamp_tpl(aux.distance / m_flareLight.m_fRadius, 0.0f, 1.0f);
			}
		}
		else
		{
			m_globalFlareBrightness *= clamp_tpl(1.0f - aux.distance / m_flareLight.m_fRadius, 0.0f, 1.0f);
		}
	}

	if (m_bAffectedByLightFOV)
	{
		m_globalFlareBrightness *= aux.viewAngleFalloff;
	}

	float fShaftVisibilityFactor = aux.bForceRender ? 1.0f : GetShaftVisibilityFactor();
	float fFlareVisibilityFactor = aux.bForceRender ? 1.0f : GetFlareVisibilityFactor();

	m_globalShaftBrightness = m_globalFlareBrightness * fShaftVisibilityFactor;
	m_globalFlareBrightness *= fFlareVisibilityFactor;

	m_globalOcclusionBokeh = m_bOcclusionBokeh & IsOcclusionEnabled();
	m_globalOrbitAngle = m_fOrbitAngle;
	m_globalTransform = m_mxTransform;

	COpticsGroup::validateChildrenGlobalVars(aux);
}

void RootOpticsElement::RenderPreview(const SLensFlareRenderParam* pParam, const Vec3& vPos)
{
	if (pParam == NULL)
		return;
	if (!pParam->IsValid())
		return;

	if (gcpRendD3D->m_pRT->IsRenderThread())
	{
		return RT_RenderPreview(pParam, vPos);
	}
	
	SLensFlareRenderParam copyParam = *pParam;
	gcpRendD3D->m_pRT->ExecuteRenderThreadCommand([=] 
	{
		RT_RenderPreview(&copyParam, vPos);
	}, ERenderCommandFlags::None);
}

void RootOpticsElement::RT_RenderPreview(const SLensFlareRenderParam* pParam, const Vec3& vPos)
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "RootOpticsElement::RT_RenderPreview");

	CRY_ASSERT(pParam  && pParam->IsValid());

	SFlareLight light;
	light.m_vPos = vPos;
	light.m_fRadius = 10000.0f;
	light.m_bAttachToSun = false;
	light.m_cLdrClr = ColorF(1, 1, 1, 1);
	light.m_fClrMultiplier = 1;
	light.m_fViewAngleFalloff = 1;

	const bool bIgnoreOcclusionQueries = true;

	if (CTexture* pDstRT = gcpRendD3D->GetActiveDisplayContext()->GetCurrentBackBuffer())
	{
		CClearSurfacePass::Execute(pDstRT, Clr_Empty);

		D3DViewPort viewport;
		viewport.TopLeftX = viewport.TopLeftY = 0.0f;
		viewport.Width = float(pDstRT->GetWidth());
		viewport.Height = float(pDstRT->GetHeight());
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		SRenderViewInfo viewInfo[CCamera::eEye_eCount];
		size_t viewInfoCount = gcpRendD3D->GetGraphicsPipeline().GenerateViewInfo(viewInfo);

		std::vector<CPrimitiveRenderPass*> prePasses;

		CPrimitiveRenderPass previewPass;
		previewPass.SetRenderTarget(0, pDstRT);
		previewPass.SetViewport(viewport);
		previewPass.BeginAddingPrimitives();

		if (ProcessAll(previewPass, prePasses, light, viewInfo, viewInfoCount, true, bIgnoreOcclusionQueries))
			previewPass.Execute();
	}
}

bool RootOpticsElement::ProcessAll(CPrimitiveRenderPass& targetPass, std::vector<CPrimitiveRenderPass*>& prePasses, const SFlareLight& light, const SRenderViewInfo* pViewInfo, int viewInfoCount, bool bForceRender, bool bUpdateOcclusion)
{
	assert(viewInfoCount > 0 && viewInfoCount <= CCamera::eEye_eCount);

	Vec3 vSrcWorldPos = light.m_vPos;
	Vec3 vSrcProjPos;

	m_flareLight = light;
	float linearDepth = 0;
	float distance = 0;

	// Ideally we'd use the center camera here
	const SRenderViewInfo& viewInfo = pViewInfo[0];

	if (!CFlareSoftOcclusionQuery::ComputeProjPos(vSrcWorldPos, viewInfo.viewMatrix, viewInfo.projMatrix, vSrcProjPos))
		return false;

	if (viewInfo.flags & SRenderViewInfo::eFlags_ReverseDepth)
		vSrcProjPos.z = 1.0f - vSrcProjPos.z;

	linearDepth = clamp_tpl(CFlareSoftOcclusionQuery::ComputeLinearDepth(vSrcWorldPos, viewInfo.viewMatrix, viewInfo.nearClipPlane, viewInfo.farClipPlane), -1.0f, 0.99f);
	distance = viewInfo.cameraOrigin.GetDistance(vSrcWorldPos);

	if (GetElementCount() <= 0 || !IsEnabled())
		return false;

	if (!bForceRender && (linearDepth <= 0 || !IsVisibleBasedOnLight(light, distance)))
		return false;

	float fFade = 1 - 1000.f * m_fDistanceFadingFactor * linearDepth;
	if (!light.m_bAttachToSun && fFade <= 0.001f)
		return false;

	if (!bForceRender && IsOcclusionEnabled())
	{
		float curTargetVisibility = 0.0f;
		m_fFlareVisibilityFactor = m_fShaftVisibilityFactor = 0.0f;

		if (m_pOccQuery)
		{
			curTargetVisibility = m_pOccQuery->GetVisibility();

			if (CSoftOcclusionVisiblityFader* pFader = m_pOccQuery->GetVisibilityFader(VISFADER_FLARE))
				m_fFlareVisibilityFactor = bUpdateOcclusion ? pFader->UpdateVisibility(curTargetVisibility, m_fFlareTimelineDuration) : pFader->m_fVisibilityFactor;

			if (CSoftOcclusionVisiblityFader* pFader = m_pOccQuery->GetVisibilityFader(VISFADER_SHAFT))
				m_fShaftVisibilityFactor = bUpdateOcclusion ? pFader->UpdateVisibility(curTargetVisibility, m_fShaftTimelineDuration) : pFader->m_fVisibilityFactor;
		}

		if (!IsVisible())
			return true;
	}

	SPreparePrimitivesContext context(targetPass, prePasses);
	context.pViewInfo = pViewInfo;
	context.viewInfoCount = viewInfoCount;
	context.lightWorldPos = vSrcWorldPos;
	context.lightScreenPos[0] = vSrcProjPos;

	for (int i=1; i<viewInfoCount; ++i)
	{
		Vec3 projPos;
		if (CFlareSoftOcclusionQuery::ComputeProjPos(vSrcWorldPos, pViewInfo[i].viewMatrix, pViewInfo[i].projMatrix, projPos))
		{
			if (pViewInfo[i].flags & SRenderViewInfo::eFlags_ReverseDepth)
				projPos.z = 1.0f - projPos.z;

			PREFAST_ASSUME(i>0 && i<CCamera::eEye_eCount);
			context.lightScreenPos[i] = projPos;
		}
	}

	context.auxParams.linearDepth = linearDepth;
	context.auxParams.distance = distance;
	float x = vSrcProjPos.x * 2 - 1;
	float y = vSrcProjPos.y * 2 - 1;
	float unitLenSq = (x * x + y * y);
	context.auxParams.sensorVariationValue = clamp_tpl((1 - powf(unitLenSq, 0.25f)) * 2 - 1, -1.0f, 1.0f);
	context.auxParams.perspectiveShortening = clamp_tpl(10.f * (1.f - vSrcProjPos.z), 0.0f, 2.0f);
	context.auxParams.viewAngleFalloff = light.m_fViewAngleFalloff;
	context.auxParams.attachToSun = light.m_bAttachToSun;
	context.auxParams.bMultiplyColor = IsMultiplyColor();
	context.auxParams.bForceRender = bForceRender;
	context.auxParams.bIgnoreOcclusionQueries = bUpdateOcclusion;

	validateGlobalVars(context.auxParams);
	if (bForceRender || m_globalFlareBrightness > 0.001f || m_globalShaftBrightness > 0.001f)
		COpticsGroup::PreparePrimitives(context);

	return true;
}

