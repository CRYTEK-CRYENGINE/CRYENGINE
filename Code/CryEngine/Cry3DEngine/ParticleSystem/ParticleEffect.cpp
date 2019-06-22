// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleEffect.h"
#include "ParticleEmitter.h"
#include "ParticleSystem.h"
#include "Material.h"
#include <CrySerialization/STL.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/SmartPtr.h>
#include <CryParticleSystem/ParticleParams.h>

namespace pfx2
{

uint GetVersion(Serialization::IArchive& ar)
{
	SSerializationContext* pContext = ar.context<SSerializationContext>();
	if (!pContext)
		return gCurrentVersion;
	return pContext->m_documentVersion;
}

//////////////////////////////////////////////////////////////////////////
// CParticleEffect

CParticleEffect::CParticleEffect()
	: m_editVersion(0)
	, m_dirty(true)
	, m_substitutedPfx1(false)
{
	m_pAttributes = TAttributeTablePtr(new CAttributeTable);
}

cstr CParticleEffect::GetName() const
{
	return m_name.empty() ? nullptr : m_name.c_str();
}

void CParticleEffect::Compile()
{
	CRY_PFX2_PROFILE_DETAIL;

	if (!m_dirty)
		return;

	for (auto& component : m_components)
	{
		component->m_pEffect = this;
		component->SetChanged();
		component->PreCompile();
		component->ResolveDependencies();
	}

	Sort();

	uint id = 0;
	for (auto& component : m_components)
	{
		component->m_componentId = id++;
		if (!component->IsActive())
			continue;
		component->Compile();
	}

	m_topComponents.clear();
	for (auto& component : m_components)
	{
		component->FinalizeCompile();
		if (!component->GetParentComponent())
			m_topComponents.push_back(component);
	}

	m_dirty = false;
}

struct SortedComponents: TComponents
{
	SortedComponents(const TComponents& src)
	{
		for (auto pComp : src)
		{
			if (!pComp->GetParentComponent())
				AddTree(pComp);
		}
	}

	void AddTree(CParticleComponent* pComp)
	{
		push_back(pComp);
		for (auto pChild : pComp->GetChildComponents())
			AddTree(pChild);
	}
};

void CParticleEffect::Sort()
{
	SortedComponents sortedComponents(m_components);
	assert(sortedComponents.size() == m_components.size());
	m_components.swap(sortedComponents);
}

void CParticleEffect::SortFromTop()
{
	SortedComponents sortedComponents(m_topComponents);
	assert(sortedComponents.size() == m_components.size());
	m_components.swap(sortedComponents);
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

string CParticleEffect::GetShortName() const
{
	string name = m_name;
	if (name.Right(4).MakeLower() == ".pfx")
		name.resize(name.length() - 4);
	if (name.Left(10).MakeLower() == "particles/")
		name.erase(0, 10);
	return name;
}

int CParticleEffect::GetEditVersion() const
{
	uint32 version = m_editVersion;
	uint32 shift = 1;
	for (const CParticleComponent* pComponent : m_components)
	{
		const SComponentParams& params = pComponent->GetComponentParams();
		if (const CMatInfo* pMatInfo = (CMatInfo*)params.m_pMaterial.get())
			version += pMatInfo->GetModificationId() << shift;
		shift = (shift + 1) & 31;
	}
	return version;
}

void CParticleEffect::SetName(cstr name)
{
	m_name = name;
}

void CParticleEffect::Serialize(Serialization::IArchive& ar)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	uint documentVersion = 1;
	if (ar.isOutput())
		documentVersion = gCurrentVersion;
	ar(documentVersion, "Version", "");
	SSerializationContext documentContext(documentVersion);
	Serialization::SContext context(ar, &documentContext);

	if (documentVersion < gMinimumVersion || documentVersion > gCurrentVersion)
		gEnv->pLog->LogError("Particle effect %s has unsupported version %d. Valid versions are %d to %d. Some values may be incorrectly read.", 
			GetName(), documentVersion, gMinimumVersion, gCurrentVersion);

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
		Serialization::SContext effectContext(ar, this);
		ar(m_components, "Components", "+Components");
	}

	if (ar.isInput())
	{
		stl::find_and_erase_all(m_components, nullptr);
		m_components.shrink_to_fit();
		SetChanged();
		for (auto& component : m_components)
			component->SetChanged();
		Compile();
	}
}

IParticleEmitter* CParticleEffect::Spawn(const ParticleLoc& loc, const SpawnParams* pSpawnParams)
{
	CRY_PFX2_PROFILE_DETAIL;

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

void CParticleEffect::RemoveComponent(uint componentIdx, bool all)
{
	CParticleComponent* pComp = m_components[componentIdx];
	stl::find_and_erase(pComp->GetParentChildren(), pComp);
	while (all && pComp->m_children.size())
		pComp = pComp->m_children.back();
	size_t endIdx = pComp->GetComponentId() + 1;
	m_components.erase(m_components.begin() + componentIdx, m_components.begin() + endIdx);
	for (uint id = componentIdx; id < m_components.size(); ++id)
		m_components[id]->m_componentId = id;
	SetChanged();
}

void CParticleEffect::SetChanged()
{
	if (!m_dirty)
		++m_editVersion;
	m_dirty = true;
}

bool CParticleEffect::LoadResources()
{
	for (auto& comp : m_components)
	{
		if (!comp->IsActive())
			continue;
		comp->LoadResources(*comp);
		comp->FinalizeCompile();
	}
	return true;
}

void CParticleEffect::UnloadResources()
{
	for (auto& comp : m_components)
	{
		auto& params = comp->ComponentParams();
		params.m_pMaterial = nullptr;
		params.m_pMesh = nullptr;
	}
}

Serialization::SStruct CParticleEffect::GetEffectOptionsSerializer() const
{
	return Serialization::SStruct(*m_pAttributes);
}

TParticleAttributesPtr CParticleEffect::CreateAttributesInstance() const
{
	return TParticleAttributesPtr(new CAttributeInstance(m_pAttributes));
}

const ParticleParams& CParticleEffect::GetDefaultParams() const
{
	static ParticleParams paramsStandard;
	return paramsStandard;
}

}
