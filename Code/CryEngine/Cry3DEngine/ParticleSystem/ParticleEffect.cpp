// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleEffect.h"
#include "ParticleSystem.h"
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
	, m_numRenderObjects(0)
{
	m_pAttributes = TAttributeTablePtr(new CAttributeTable);
}

cstr CParticleEffect::GetName() const
{
	return m_name.empty() ? nullptr : m_name.c_str();
}

void CParticleEffect::Update()
{
	CRY_PFX2_PROFILE_DETAIL;

	if (!m_dirty)
		return;

	m_numRenderObjects = 0;
	m_environFlags = 0;
	for (auto& component : m_components)
	{
		component->m_pEffect = this;
		component->SetChanged();
		component->PreCompile();
	}
	for (auto& component : m_components)
		component->ResolveDependencies();

	Sort();

	uint id = 0;
	MainPreUpdate.clear();
	RenderDeferred.clear();
	for (auto& component : m_components)
	{
		component->m_componentId = id++;
		component->Compile();
		if (component->MainPreUpdate.size())
			MainPreUpdate.push_back(component);
		if (component->RenderDeferred.size())
			RenderDeferred.push_back(component);
	}

	m_timings = {};
	for (auto& component : m_components)
	{
		component->FinalizeCompile();
		if (!component->GetParentComponent())
		{
			component->UpdateTimings();
			const STimingParams& timings = component->ComponentParams();
			SetMax(m_timings.m_maxParticleLife, timings.m_maxParticleLife);
			SetMax(m_timings.m_stableTime, timings.m_stableTime);
			SetMax(m_timings.m_equilibriumTime, timings.m_equilibriumTime);
			SetMax(m_timings.m_maxTotalLIfe, timings.m_maxTotalLIfe);
		}
	}

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

	do
	{
		int pos = newName.length() - 1;
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
	while ((found = FindComponentByName(newName)) && found != forComponent);

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
		auto it = std::remove_if(m_components.begin(), m_components.end(), [](TComponentPtr ptr){ return !ptr; });
		m_components.erase(it, m_components.end());
		SetChanged();
		for (auto& component : m_components)
			component->SetChanged();
		Update();
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
	auto pComp = m_components[componentIdx];
	pComp->SetParent(nullptr);
	while (all && pComp->m_children.size())
		pComp = pComp->m_children.back();
	size_t endIdx = pComp->GetComponentId() + 1;
	m_components.erase(m_components.begin() + componentIdx, m_components.begin() + endIdx);
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
