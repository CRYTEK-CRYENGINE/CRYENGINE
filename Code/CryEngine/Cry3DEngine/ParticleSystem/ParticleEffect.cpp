// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     23/09/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CrySerialization/STL.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/SmartPtr.h>
#include <CryParticleSystem/ParticleParams.h>
#include "ParticleEffect.h"
#include "ParticleEmitter.h"
#include "ParticleFeature.h"

CRY_PFX2_DBG

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// CParticleEffect

CParticleEffect::CParticleEffect()
	: m_editVersion(0)
	, m_dirty(true)
	, m_numRenderObjects(0)
{
	m_pAttributes = TAttributeTablePtr(new CAttributeTable);
}

cstr CParticleEffect::GetName() const
{
	return m_name.empty() ? nullptr : m_name.c_str();
}

void CParticleEffect::Compile()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PARTICLE);

	if (!m_dirty)
		return;

	m_numRenderObjects = 0;
	m_attributeInstance.Reset(m_pAttributes, EAttributeScope::PerEffect);
	for (size_t i = 0; i < m_components.size(); ++i)
	{
		m_components[i]->m_pEffect = this;
		m_components[i]->m_componentId = i;
		m_components[i]->SetChanged();
		m_components[i]->m_componentParams.Reset();
		m_components[i]->PreCompile();
	}
	for (auto& component : m_components)
		component->ResolveDependencies();
	for (auto& component : m_components)
		component->Compile();
	for (auto& component : m_components)
		component->FinalizeCompile();

	m_dirty = false;
}

TComponentId CParticleEffect::FindComponentIdByName(const char* name) const
{
	const auto it = std::find_if(m_components.begin(), m_components.end(), [name](TComponentPtr pComponent)
	{
		if (!pComponent)
			return false;
		return strcmp(pComponent->GetName(), name) == 0;
	});
	if (it == m_components.end())
		return gInvalidId;
	return TComponentId(it - m_components.begin());
}

string CParticleEffect::MakeUniqueName(TComponentId forComponentId, const char* name)
{
	TComponentId foundId = FindComponentIdByName(name);
	if (foundId == forComponentId || foundId == gInvalidId)
		return string(name);

	string newName = name;
	int pos = newName.length() - 1;

	do
	{
		while (pos >= 0 && newName[pos] == '9')
		{
			newName.replace(pos, 1, 1, '0');
			pos--;
		}
		if (pos < 0 || !isdigit(newName[pos]))
			newName.insert(++pos, '1');
		else
			newName.replace(pos, 1, 1, newName[pos] + 1);
	}
	while (FindComponentIdByName(newName) != gInvalidId);

	return newName;
}

uint CParticleEffect::AddRenderObjectId()
{
	return m_numRenderObjects++;
}

uint CParticleEffect::GetNumRenderObjectIds() const
{
	return m_numRenderObjects;
}

float CParticleEffect::GetEquilibriumTime() const
{
	float maxEqTime = 0.0f;
	for (auto comp : m_components)
	{
		// Iterate top-level components
		auto const& params = comp->GetComponentParams();
		if (comp->IsEnabled() && !params.IsSecondGen() && params.IsImmortal())
		{
			float eqTime = comp->GetEquilibriumTime(Range(params.m_emitterLifeTime.start));
			maxEqTime = max(maxEqTime, eqTime);
		}
	}
	return maxEqTime;
}

int CParticleEffect::GetEditVersion() const
{
	int version = m_editVersion + m_components.size();
	for (auto pComponent : m_components)
	{
		const SComponentParams& params = pComponent->GetComponentParams();
		const CMatInfo* pMatInfo = (CMatInfo*)params.m_pMaterial.get();
		if (pMatInfo)
			version += pMatInfo->GetModificationId();
	}
	return version;
}

void CParticleEffect::SetName(cstr name)
{
	m_name = name;
}

void CParticleEffect::Serialize(Serialization::IArchive& ar)
{
	uint documentVersion = 1;
	if (ar.isOutput())
		documentVersion = gCurrentVersion;
	ar(documentVersion, "Version", "");
	SSerializationContext documentContext(documentVersion);
	Serialization::SContext context(ar, &documentContext);

	ar(*m_pAttributes, "Attributes");

	if (ar.isInput() && documentVersion < 3)
	{
		std::vector<CParticleComponent> oldComponents;
		ar(oldComponents, "Components", "+Components");
		m_components.reserve(oldComponents.size());
		for (auto& oldComponent : oldComponents)
			m_components.push_back(new CParticleComponent(oldComponent));
	}
	else
		ar(m_components, "Components", "+Components");

	if (ar.isInput())
	{
		auto it = std::remove_if(m_components.begin(), m_components.end(), [](TComponentPtr ptr){ return !ptr; });
		m_components.erase(it, m_components.end());
		SetChanged();
		for (auto& component : m_components)
			component->SetChanged();
		Compile();
	}
}

IParticleEmitter* CParticleEffect::Spawn(const ParticleLoc& loc, const SpawnParams* pSpawnParams)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_PARTICLE);

	PParticleEmitter pEmitter = GetPSystem()->CreateEmitter(this);
	CParticleEmitter* pCEmitter = static_cast<CParticleEmitter*>(pEmitter.get());
	if (pSpawnParams)
		pCEmitter->SetSpawnParams(*pSpawnParams);
	pEmitter->Activate(true);
	pCEmitter->SetLocation(loc);
	return pEmitter;
}

void CParticleEffect::AddComponent(uint componentIdx)
{
	uint idx = m_components.size();
	CParticleComponent* pNewComponent = new CParticleComponent();
	pNewComponent->m_pEffect = this;
	pNewComponent->m_componentId = componentIdx;
	pNewComponent->SetName("Component01");
	m_components.insert(m_components.begin() + componentIdx, pNewComponent);
	SetChanged();
}

void CParticleEffect::RemoveComponent(uint componentIdx)
{
	m_components.erase(m_components.begin() + componentIdx);
	SetChanged();
}

void CParticleEffect::SetChanged()
{
	if (!m_dirty)
		++m_editVersion;
	m_dirty = true;
}

Serialization::SStruct CParticleEffect::GetEffectOptionsSerializer() const
{
	return Serialization::SStruct(*m_pAttributes);
}

const ParticleParams& CParticleEffect::GetDefaultParams() const
{
	static ParticleParams paramsStandard;
	return paramsStandard;
}

}
