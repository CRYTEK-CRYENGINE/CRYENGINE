// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MFXRandomEffect.h"

void CMFXRandomizerContainer::ExecuteRandomizedEffects(const SMFXRunTimeEffectParams& params)
{
	TMFXEffectBasePtr pEffect = ChooseCandidate(params);
	if (pEffect)
	{
		pEffect->Execute(params);
	}
}

TMFXEffectBasePtr CMFXRandomizerContainer::ChooseCandidate(const SMFXRunTimeEffectParams& params) const
{
	const size_t nEffects = m_effects.size();
	if (nEffects == 0)
		return TMFXEffectBasePtr(NULL);

	CryFixedArray<TMFXEffectBasePtr, 16> candidatesArray;

	TMFXEffects::const_iterator it = m_effects.begin();
	TMFXEffects::const_iterator itEnd = m_effects.end();
	while ((it != itEnd) && !candidatesArray.isfull())
	{
		const TMFXEffectBasePtr& pEffect = *it;
		if (pEffect->CanExecute(params))
		{
			candidatesArray.push_back(pEffect);
		}
		++it;
	}

	TMFXEffectBasePtr pChosenEffect = NULL;
	if (!candidatesArray.empty())
	{
		const uint32 randChoice = cry_random(0U, candidatesArray.size() - 1);
		pChosenEffect = candidatesArray[randChoice];
	}

	return pChosenEffect;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CMFXRandomEffect::CMFXRandomEffect()
	: CMFXEffectBase(eMFXPF_All)
{
}

CMFXRandomEffect::~CMFXRandomEffect()
{
}

void CMFXRandomEffect::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
}

void CMFXRandomEffect::Execute(const SMFXRunTimeEffectParams& params)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	m_container.ExecuteRandomizedEffects(params);
}

void CMFXRandomEffect::LoadParamsFromXml(const XmlNodeRef& paramsNode)
{
	// Xml data format
	/*
	   <RandEffect>
	   <Particle />
	   <Audio />
	   <ForceFeedback />
	   ...
	   </Particle>
	 */

	m_container.BuildFromXML(paramsNode);
}

void CMFXRandomEffect::GetResources(SMFXResourceList& resourceList) const
{
	m_container.GetResources(resourceList);
}

void CMFXRandomEffect::PreLoadAssets()
{
	m_container.PreLoadAssets();
}

void CMFXRandomEffect::ReleasePreLoadAssets()
{
	m_container.ReleasePreLoadAssets();
}

void CMFXRandomEffect::SetCustomParameter(const char* customParameter, const SMFXCustomParamValue& customParameterValue)
{
	m_container.SetCustomParameter(customParameter, customParameterValue);
}
