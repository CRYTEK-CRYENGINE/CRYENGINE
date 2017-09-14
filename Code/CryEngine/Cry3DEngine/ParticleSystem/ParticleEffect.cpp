// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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
	, m_substitutedPfx1(false)
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
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (!m_dirty)
		return;

	m_numRenderObjects = 0;
	m_attributeInstance.Reset(m_pAttributes, EAttributeScope::PerEffect);
	for (auto& component : m_components)
	{
		component->m_pEffect = this;
		component->SetChanged();
		component->m_componentParams.Reset();
		component->PreCompile();
	}
	for (auto& component : m_components)
		component->ResolveDependencies();

	Sort();

	uint id = 0;
	for (auto& component : m_components)
	{
		component->Compile();
		component->m_componentId = id++;
	}
	for (auto& component : m_components)
		component->FinalizeCompile();

	m_dirty = false;
}

void CParticleEffect::Sort()
{
	struct SortedComponents: TComponents
	{
		SortedComponents(const TComponents& src)
		{
			for (auto pComp : src)
			{
				if (!pComp->GetParentComponent())
					AddTree(pComp);
			}
			assert(size() == src.size());
		}

		void AddTree(CParticleComponent* pComp)
		{
			push_back(pComp);
			for (auto pChild : pComp->GetChildComponents())
				AddTree(pChild);
		}
	};

	SortedComponents sortedComponents(m_components);
	std::swap(m_components, sortedComponents);
}

CParticleComponent* CParticleEffect::FindComponentByName(const char* name) const
{
	for (const auto& pComponent : m_components)
	{
		if (pComponent->m_name == name)
			return pComponent;
	}
	return nullptr;
}

string CParticleEffect::MakeUniqueName(const CParticleComponent* forComponent, const char* name)
{
	CParticleComponent* found = FindComponentByName(name);
	if (!found || found == forComponent)
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
	while (FindComponentByName(newName));

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
	for (const auto& pComponent : m_components)
	{
		// Iterate top-level components
		auto const& params = pComponent->GetComponentParams();
		if (pComponent->IsEnabled() && !pComponent->GetParentComponent() && params.IsImmortal())
		{
			float eqTime = pComponent->GetEquilibriumTime(Range(params.m_emitterLifeTime.start));
			maxEqTime = max(maxEqTime, eqTime);
		}
	}
	return maxEqTime;
}

int CParticleEffect::GetEditVersion() const
{
	int version = m_editVersion + m_components.size();
	for (const auto& pComponent : m_components)
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
	{
		if (ar.isInput() && !ar.isEdit())
			m_components.clear();
		ar(m_components, "Components", "+Components");
	}

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
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	PParticleEmitter pEmitter = GetPSystem()->CreateEmitter(this);
	CParticleEmitter* pCEmitter = static_cast<CParticleEmitter*>(pEmitter.get());
	if (pSpawnParams)
		pCEmitter->SetSpawnParams(*pSpawnParams);
	pEmitter->Activate(true);
	pCEmitter->SetLocation(loc);
	return pEmitter;
}

IParticleComponent* CParticleEffect::AddComponent()
{
	CParticleComponent* pNewComponent = new CParticleComponent();
	pNewComponent->m_pEffect = this;
	pNewComponent->m_componentId = m_components.size();
	pNewComponent->SetName("Component01");
	m_components.push_back(pNewComponent);
	SetChanged();
	return pNewComponent;
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
