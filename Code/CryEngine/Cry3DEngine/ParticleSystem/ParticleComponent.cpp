// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleComponent.h"
#include "ParticleEffect.h"
#include "ParticleSystem.h"
#include <CrySerialization/STL.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/SmartPtr.h>
#include <CrySerialization/DynArray.h>
#include <CrySerialization/Math.h>

namespace pfx2
{

MakeDataType(EPDT_SpawnId,          TParticleId);
MakeDataType(EPDT_ParentId,         TParticleId);
MakeDataType(EPDT_SpawnFraction,    float);
MakeDataType(EPDT_NormalAge,        float);
MakeDataType(EPDT_LifeTime,         float);
MakeDataType(EPDT_InvLifeTime,      float);
MakeDataType(EPDT_Random,           float);

MakeDataType(EPVF_Position,         Vec3);
MakeDataType(EPVF_Velocity,         Vec3);
MakeDataType(EPQF_Orientation,      Quat);
MakeDataType(EPVF_AngularVelocity,  Vec3);

void SVisibilityParams::Combine(const SVisibilityParams& o) // Combination from multiple features chooses most restrictive values
{
	m_viewDistanceMultiple = m_viewDistanceMultiple * o.m_viewDistanceMultiple;
	SetMin(m_maxScreenSize, o.m_maxScreenSize);
	SetMax(m_minCameraDistance, o.m_minCameraDistance);
	SetMin(m_maxCameraDistance, o.m_maxCameraDistance);
	if (m_indoorVisibility == EIndoorVisibility::Both)
		m_indoorVisibility = o.m_indoorVisibility;
	if (m_waterVisibility == EWaterVisibility::Both)
		m_waterVisibility = o.m_waterVisibility;
}

void STextureAnimation::Serialize(Serialization::IArchive& ar)
{
	ar(m_frameCount, "FrameCount", "Frame Count");
	ar(m_frameRate, "FrameRate", "Frame Rate");
	ar(m_cycleMode, "CycleMode", "Cycle Mode");
	ar(m_frameBlending, "FrameBlending", "Frame Blending");

	if (ar.isInput())
		Update();
}

void STextureAnimation::Update()
{
	m_ageScale = m_frameRate / m_frameCount;

	// Scale animation position to frame number
	if (m_cycleMode != EAnimationCycle::Once)
		m_animPosScale = float(m_frameCount);
	else if (m_frameBlending)
		m_animPosScale = float(m_frameCount - 1);
	else
		// If not cycling, reducing slightly to avoid hitting 1
		m_animPosScale = float(m_frameCount) - 0.001f;
}

//////////////////////////////////////////////////////////////////////////
// SComponentParams

void SComponentParams::Serialize(Serialization::IArchive& ar)
{
	auto* pComponent = static_cast<CParticleComponent*>(ar.context<IParticleComponent>());
	if (!pComponent || !ar.isEdit() || !ar.isOutput())
		return;
	char buffer[1024];

	bool first = true;
	buffer[0] = 0;
	for (auto type : EParticleDataType::values())
	{
		if (pComponent->UseParticleData(type))
		{
			if (first)
				cry_sprintf(buffer, "%s", type.name());
			else
				cry_sprintf(buffer, "%s, %s", buffer, type.name());
			first = false;
		}
	}
	ar(string(buffer), "", "!Fields used:");

	cry_sprintf(buffer, "%d", pComponent->GetUseData()->totalSizes[EDD_Particle]);
	ar(string(buffer), "", "!Bytes per Particle:");
}

//////////////////////////////////////////////////////////////////////////
// CParticleComponent

CParticleComponent::CParticleComponent()
	: m_dirty(true)
	, m_pEffect(nullptr)
	, m_parent(nullptr)
	, m_componentId(gInvalidId)
	, m_nodePosition(-1.0f, -1.0f)
{
}

void CParticleComponent::SetChanged()
{
	m_dirty = true;
	if (m_pEffect)
		m_pEffect->SetChanged();
}

IParticleFeature* CParticleComponent::GetFeature(uint featureIdx) const
{
	return m_features[featureIdx].get();
}

IParticleFeature* CParticleComponent::AddFeature(uint placeIdx, const SParticleFeatureParams& featureParams)
{
	IParticleFeature* pNewFeature = (featureParams.m_pFactory)();
	m_features.insert(placeIdx, static_cast<CParticleFeature*>(pNewFeature));
	SetChanged();
	return pNewFeature;
}

void CParticleComponent::AddFeature(uint placeIdx, CParticleFeature* pFeature)
{
	m_features.insert(placeIdx, pFeature);
	SetChanged();
}

void CParticleComponent::AddFeature(CParticleFeature* pFeature)
{
	m_features.push_back(pFeature);
	SetChanged();
}

void CParticleComponent::RemoveFeature(uint featureIdx)
{
	m_features.erase(m_features.begin() + featureIdx);
	SetChanged();
}

void CParticleComponent::SwapFeatures(const uint* swapIds, uint numSwapIds)
{
	CRY_PFX2_ASSERT(numSwapIds == m_features.size());
	decltype(m_features)newFeatures;
	newFeatures.resize(m_features.size());
	for (uint i = 0; i < numSwapIds; ++i)
		newFeatures[i] = m_features[swapIds[i]];
	std::swap(m_features, newFeatures);
	SetChanged();
}

Vec2 CParticleComponent::GetNodePosition() const
{
	return m_nodePosition;
}

void CParticleComponent::SetNodePosition(Vec2 position)
{
	m_nodePosition = position;
}

void CParticleComponent::AddParticleData(EParticleDataType type)
{
	SetChanged();
	m_pUseData->AddData(type);
}

bool CParticleComponent::SetParent(IParticleComponent* pParent, int position)
{
	auto newParent = static_cast<CParticleComponent*>(pParent);
	if (m_parent == newParent)
	{
		if (position < 0)
			return true;
		int index = GetIndex(true);
		if (position == index)
			return true;
		if (position > index)
			position--;
	}

	if (newParent && !newParent->CanBeParent(this))
		return false;

	stl::find_and_erase(GetParentChildren(), this);
	m_parent = newParent;
	auto& children = GetParentChildren();

	position = min((uint)position, (uint)children.size());
	children.insert(children.begin() + position, this);
	if (!m_parent)
		m_pEffect->SortFromTop();
	SetChanged();
	assert(GetIndex(true) == position);
	return true;
}

bool CParticleComponent::CanBeParent(IParticleComponent* child) const
{
	if (m_params.m_usesGPU)
		return false;
	if (child)
	{
		// Prevent cycles
		for (const IParticleComponent* parent = this; parent; parent = parent->GetParent())
			if (parent == child)
				return false;
	}
	return true;
}

uint CParticleComponent::GetIndex(bool fromParent /*= false*/)
{
	if (!fromParent)
		return m_componentId;

	const auto& children = GetParentChildren();
	for (uint index = 0; index < children.size(); ++index)
		if (children[index] == this)
			return index;
	return children.size();
}

const pfx2::CParticleComponent::TComponents& CParticleComponent::GetParentChildren() const
{
	return m_parent ? m_parent->m_children : m_pEffect->GetTopComponents();
}

pfx2::CParticleComponent::TComponents& CParticleComponent::GetParentChildren()
{
	return m_parent ? m_parent->m_children : m_pEffect->GetTopComponents();
}

bool CParticleComponent::CanMakeRuntime(CParticleEmitter* pEmitter) const
{
	if (!IsEnabled())
		return false;
	for (auto& pFeature : m_features)
	{
		if (pFeature->IsEnabled() && !pFeature->CanMakeRuntime(pEmitter))
			return false;
	}
	if (m_parent)
		return m_parent->CanMakeRuntime(pEmitter);
	return true;
}

void CParticleComponent::PreCompile()
{
	CRY_PFX2_PROFILE_DETAIL;

	if (!m_dirty)
		return;

	m_params = {};
	m_GPUParams = {};

	static_cast<SFeatureDispatchers&>(*this) = {};
	m_gpuFeatures.clear();
}

void CParticleComponent::ResolveDependencies()
{
	if (!m_dirty)
		return;

	for (auto& it : m_features)
	{
		if (it && it->IsEnabled())
		{
			// potentially replace feature with new one or null
			CParticleFeature* pFeature = it->ResolveDependency(this);
			it = pFeature;
		}
	}

	// remove null features
	stl::find_and_erase_all(m_features, nullptr);
	m_features.shrink_to_fit();
}

void CParticleComponent::Compile()
{
	if (!m_dirty)
		return;

	CRY_PFX2_PROFILE_DETAIL;

	// Create new use data array, existing containers reference old array
	SUseData usePrev;
	if (m_pUseData)
		usePrev = *m_pUseData;

	m_pUseData = NewUseData();

	// Add default particle data
	AddParticleData(EPDT_ParentId);
	AddParticleData(EPVF_Position);
	AddParticleData(EPVF_Velocity);
	AddParticleData(EPDT_NormalAge);
	AddParticleData(EPDT_InvLifeTime);
	AddParticleData(EPDT_LifeTime);

	// Validate feature requirements and exclusivity.
	uint featureMask = 0;
	int maxPriority = 0;
	for (auto& it : m_features)
	{
		if (it->IsEnabled())
		{
			EFeatureType type = it->GetFeatureType();
			if (type & (EFT_Life | EFT_Motion | EFT_Render))
				if (featureMask & type)
				{
					it->SetEnabled(false);
					continue;
				}
			featureMask |= type;
			SetMax(maxPriority, it->Priority());
		}
	}

	// Init features in priority order
	for (int pass = 0; pass <= maxPriority; pass++)
	{
		for (auto& it : m_features)
		{
			if (it->IsEnabled())
			{
				int priority = it->Priority();
				if (priority == pass)
					it->AddToComponent(this, &m_params);
			}
		}
	}

	// add default features
	m_defaultFeatures.clear();
	for (uint b = 1; b < EFT_END; b <<= 1)
	{
		if (!(featureMask & b))
		{
			if (EFeatureType(b) == EFT_Child && !m_parent)
				continue;
			if (EFeatureType(b) == EFT_Render)
			{
				if (featureMask & EFT_Effect)
					continue;
				if (m_children.size())
					continue;
			}
			if (auto* params = GetPSystem()->GetDefaultFeatureParam(EFeatureType(b)))
			{
				if (auto* feature = static_cast<CParticleFeature*>(params->m_pFactory()))
				{
					m_defaultFeatures.push_back(feature);
					feature->AddToComponent(this, &m_params);
				}
			}
		}
	}
}

void CParticleComponent::FinalizeCompile()
{
	if (IsActive() && !m_params.m_pMaterial)
	{
		static cstr s_defaultDiffuseMap = "%ENGINE%/EngineAssets/Textures/white.dds";
		m_params.m_pMaterial = GetPSystem()->GetTextureMaterial(s_defaultDiffuseMap, 
			m_params.m_usesGPU, m_GPUParams.facingMode);
	}
	m_dirty = false;
}

void CParticleComponent::Serialize(Serialization::IArchive& ar)
{
	CRY_PFX2_PROFILE_DETAIL;

	if (!m_pEffect)
		m_pEffect = ar.context<CParticleEffect>();

	ar(m_enabled);
	ar(m_visible, "Visible");
	if (ar.isOutput())
		ar(m_name, "Name", "^");
	else if (ar.isInput())
	{
		string inputName;
		ar(inputName, "Name", "^");
		SetName(inputName.c_str());
	}

	if (!ar.isEdit())
	{
		string parentName;
		if (ar.isOutput())
		{
			if (m_parent)
			{
				parentName = m_parent->GetName();
				ar(parentName, "Parent");
			}
		}
		else if (ar.isInput())
		{
			CParticleComponent* pParent = nullptr;
			if (ar(parentName, "Parent"))
			{
				assert(m_pEffect);
				pParent = m_pEffect->FindComponentByName(parentName);
			}
			SetParent(pParent);
		}
	}

	Serialization::SContext context(ar, static_cast<IParticleComponent*>(this));
	ar(m_params, "Stats", "Component Statistics");
	ar(m_nodePosition, "nodePos", "Node Position");
	ar(m_features, "Features", "^");
	if (ar.isInput())
		m_features.shrink_to_fit();
	if (ar.isInput())
		SetChanged();
}

void CParticleComponent::SetName(const char* name)
{
	if (!m_pEffect)
		m_name = name;
	else
		m_name = m_pEffect->MakeUniqueName(this, name);
}

string CParticleComponent::GetFullName() const
{
	return m_pEffect->GetShortName() + "." + m_name;
}

pfx2::CParticleFeature* CParticleComponent::FindFeature(const SParticleFeatureParams& params, const CParticleFeature* pSkip /*= nullptr*/) const
{
	for (const auto& pFeature : m_features)
	{
		if (pFeature && pFeature != pSkip && &pFeature->GetFeatureParams() == &params)
			return pFeature;
	}
	return nullptr;
}

SERIALIZATION_CLASS_NAME(CParticleComponent, CParticleComponent, "Component", "Component");

}
