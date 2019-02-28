// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MFXForceFeedbackEffect.h
//  Version:     v1.00
//  Created:     8/4/2010 by Benito Gangoso Rodriguez
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MFXForceFeedbackFX.h"
#include "CryAction.h"
#include "IForceFeedbackSystem.h"

CMFXForceFeedbackEffect::CMFXForceFeedbackEffect()
	: CMFXEffectBase(eMFXPF_ForceFeedback)
{
}

CMFXForceFeedbackEffect::~CMFXForceFeedbackEffect()
{

}

void CMFXForceFeedbackEffect::LoadParamsFromXml(const XmlNodeRef& paramsNode)
{
	// Xml data format
	/*
	   <ForceFeedback name="..." minFallOffDistance="..." maxFallOffDistance="..." />
	 */
	m_forceFeedbackParams.forceFeedbackEventName = paramsNode->getAttr("name");

	float minFallOffDistance = 0.0f;
	paramsNode->getAttr("minFallOffDistance", minFallOffDistance);
	float maxFallOffDistance = 5.0f;
	paramsNode->getAttr("maxFallOffDistance", maxFallOffDistance);

	m_forceFeedbackParams.intensityFallOffMinDistanceSqr = minFallOffDistance * minFallOffDistance;
	m_forceFeedbackParams.intensityFallOffMaxDistanceSqr = maxFallOffDistance * maxFallOffDistance;
}

void CMFXForceFeedbackEffect::Execute(const SMFXRunTimeEffectParams& params)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	float distanceToPlayerSqr = FLT_MAX;
	IActor* pClientActor = gEnv->pGameFramework->GetClientActor();
	if (pClientActor)
	{
		distanceToPlayerSqr = (pClientActor->GetEntity()->GetWorldPos() - params.pos).GetLengthSquared();
	}

	const float testDistanceSqr = clamp_tpl(distanceToPlayerSqr, m_forceFeedbackParams.intensityFallOffMinDistanceSqr, m_forceFeedbackParams.intensityFallOffMaxDistanceSqr);
	const float minMaxDiffSqr = m_forceFeedbackParams.intensityFallOffMaxDistanceSqr - m_forceFeedbackParams.intensityFallOffMinDistanceSqr;

	float effectIntensity = (float)__fsel(-minMaxDiffSqr, 0.0f, 1.0f - (testDistanceSqr - m_forceFeedbackParams.intensityFallOffMinDistanceSqr) / (minMaxDiffSqr + FLT_EPSILON));
	effectIntensity *= effectIntensity;
	if (effectIntensity > 0.01f)
	{
		IForceFeedbackSystem* pForceFeedback = CCryAction::GetCryAction()->GetIForceFeedbackSystem();
		assert(pForceFeedback);
		ForceFeedbackFxId fxId = pForceFeedback->GetEffectIdByName(m_forceFeedbackParams.forceFeedbackEventName.c_str());
		pForceFeedback->PlayForceFeedbackEffect(fxId, SForceFeedbackRuntimeParams(effectIntensity, 0.0f));
	}
}

void CMFXForceFeedbackEffect::GetResources(SMFXResourceList& resourceList) const
{
	SMFXForceFeedbackListNode* listNode = SMFXForceFeedbackListNode::Create();
	listNode->m_forceFeedbackParams.forceFeedbackEventName = m_forceFeedbackParams.forceFeedbackEventName.c_str();
	listNode->m_forceFeedbackParams.intensityFallOffMinDistanceSqr = m_forceFeedbackParams.intensityFallOffMinDistanceSqr;
	listNode->m_forceFeedbackParams.intensityFallOffMaxDistanceSqr = m_forceFeedbackParams.intensityFallOffMaxDistanceSqr;

	SMFXForceFeedbackListNode* next = resourceList.m_forceFeedbackList;

	if (!next)
		resourceList.m_forceFeedbackList = listNode;
	else
	{
		while (next->pNext)
			next = next->pNext;

		next->pNext = listNode;
	}
}

void CMFXForceFeedbackEffect::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	pSizer->Add(m_forceFeedbackParams.forceFeedbackEventName);
}
