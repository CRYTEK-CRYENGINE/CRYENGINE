// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     29/01/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CrySerialization/STL.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/SmartPtr.h>
#include <CrySerialization/Math.h>
#include "ParticleComponent.h"
#include "ParticleEffect.h"
#include "ParticleFeature.h"
#include "Features/FeatureMotion.h"

CRY_PFX2_DBG

namespace pfx2
{

EParticleDataType PDT(EPDT_SpawnId, TParticleId);
EParticleDataType PDT(EPDT_ParentId, TParticleId);
EParticleDataType PDT(EPDT_State, uint8);
EParticleDataType PDT(EPDT_SpawnFraction, float);
EParticleDataType PDT(EPDT_NormalAge, float);
EParticleDataType PDT(EPDT_LifeTime, float);
EParticleDataType PDT(EPDT_InvLifeTime, float);
EParticleDataType PDT(EPDT_Random, float);

EParticleDataType PDT(EPVF_Position, float, 3);
EParticleDataType PDT(EPVF_Velocity, float, 3);
EParticleDataType PDT(EPQF_Orientation, float, 4);
EParticleDataType PDT(EPVF_AngularVelocity, float, 3);
EParticleDataType PDT(EPVF_LocalPosition, float, 3);
EParticleDataType PDT(EPVF_LocalVelocity, float, 3);
EParticleDataType PDT(EPQF_LocalOrientation, float, 4);



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
// SModuleParams

SComponentParams::SComponentParams()
	: m_pComponent(0)
{
	Reset();
}

SComponentParams::SComponentParams(const CParticleComponent& component)
	: m_pComponent(&component)
{
	Reset();
}

void SComponentParams::Serialize(Serialization::IArchive& ar)
{
	if (!m_pComponent || !ar.isEdit() || !ar.isOutput())
		return;
	char buffer[1024];

	bool first = true;
	buffer[0] = 0;
	uint32 bytesPerParticle = 0;
	for (auto type : EParticleDataType::values())
	{
		if (m_pComponent->UseParticleData(type))
		{
			if (first)
				cry_sprintf(buffer, "%s", type.name());
			else
				cry_sprintf(buffer, "%s, %s", buffer, type.name());
			first = false;
			bytesPerParticle += type.info().typeSize() * type.info().step();
		}
	}
	ar(string(buffer), "", "!Fields used:");

	cry_sprintf(buffer, "%d", bytesPerParticle);
	ar(string(buffer), "", "!Bytes per Particle:");
}

void SComponentParams::Reset()
{
	m_shaderData = SParticleShaderData();
	m_visibility = SVisibilityParams();
	m_renderObjectFlags = 0;
	m_instanceDataStride = 0;
	m_scaleParticleCount = 1.0f;
	m_emitterLifeTime = {};
	m_maxParticleLifeTime = 0.0f;
	m_particleRange = 0;
	m_renderStateFlags = OS_ALPHA_BLEND;
	m_maxParticleSize = 0.0f;
	m_meshCentered = false;
	m_diffuseMap = "%ENGINE%/EngineAssets/Textures/white.dds";
	m_pMaterial = 0;
	m_particleObjFlags = 0;
	m_renderObjectSortBias = 0.0f;
}

void SComponentParams::MakeMaterial(CParticleComponent* pComponent)
{
	enum EGpuParticlesVertexShaderFlags
	{
		eGpuParticlesVertexShaderFlags_None           = 0x0,
		eGpuParticlesVertexShaderFlags_FacingVelocity = 0x2000
	};

	if (m_pMaterial)
		return;
		
	const gpu_pfx2::SComponentParams& params = pComponent->GetGPUComponentParams();
	const char* shaderName = params.usesGpuImplementation ? "Particles.ParticlesGpu" : "Particles";
	string materialName = string(pComponent->GetEffect()->GetName()) + pComponent->GetName();
	m_pMaterial = gEnv->p3DEngine->GetMaterialManager()->CreateMaterial(materialName);
	if (gEnv->pRenderer)
	{
		const uint32 textureLoadFlags = FT_DONT_STREAM;
		const int textureId = gEnv->pRenderer->EF_LoadTexture(m_diffuseMap.c_str(), textureLoadFlags)->GetTextureID();
		if (textureId <= 0)
			CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "Particle effect texture %s not found", m_diffuseMap.c_str());

		SInputShaderResourcesPtr pResources = gEnv->pRenderer->EF_CreateInputShaderResource();
		pResources->m_Textures[EFTT_DIFFUSE].m_Name = m_diffuseMap;
		uint32 mask = eGpuParticlesVertexShaderFlags_None;
		if (params.facingMode == gpu_pfx2::EFacingMode::Velocity)
			mask |= eGpuParticlesVertexShaderFlags_FacingVelocity;
		SShaderItem shaderItem = gEnv->pRenderer->EF_LoadShaderItem(shaderName, false, 0, pResources, mask);
		m_pMaterial->AssignShaderItem(shaderItem);

		if (textureId > 0)
			gEnv->pRenderer->RemoveTexture(textureId);
	}
	Vec3 white = Vec3(1.0f, 1.0f, 1.0f);
	float defaultOpacity = 1.0f;
	m_pMaterial->SetGetMaterialParamVec3("diffuse", white, false);
	m_pMaterial->SetGetMaterialParamFloat("opacity", defaultOpacity, false);
	m_pMaterial->RequestTexturesLoading(0.0f);
}

//////////////////////////////////////////////////////////////////////////
// CParticleComponent

CParticleComponent::CParticleComponent()
	: m_dirty(true)
	, m_pEffect(0)
	, m_componentId(gInvalidId)
	, m_componentParams(*this)
	, m_nodePosition(-1.0f, -1.0f)
{
	m_useParticleData.fill(false);
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

void CParticleComponent::AddFeature(uint placeIdx, const SParticleFeatureParams& featureParams)
{
	IParticleFeature* pNewFeature = (featureParams.m_pFactory)();
	m_features.insert(
	  m_features.begin() + placeIdx,
	  static_cast<CParticleFeature*>(pNewFeature));
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

void CParticleComponent::AddToUpdateList(EUpdateList list, CParticleFeature* pFeature)
{
	if (std::find(m_updateLists[list].begin(), m_updateLists[list].end(), pFeature) != m_updateLists[list].end())
		return;
	m_updateLists[list].push_back(pFeature);
	if (pFeature->GetGpuInterface())
	{
		if (std::find(m_gpuUpdateLists[list].begin(), m_gpuUpdateLists[list].end(), pFeature->GetGpuInterface()) != m_gpuUpdateLists[list].end())
			return;
		m_gpuUpdateLists[list].push_back(pFeature->GetGpuInterface());
	}
	SetChanged();
}

TInstanceDataOffset CParticleComponent::AddInstanceData(size_t size)
{
	CRY_PFX2_ASSERT(size > 0);        // instance data of 0 bytes makes no sense
	SetChanged();
	TInstanceDataOffset offset = TInstanceDataOffset(m_componentParams.m_instanceDataStride);
	m_componentParams.m_instanceDataStride += size;
	return offset;
}

void CParticleComponent::AddParticleData(EParticleDataType type)
{
	SetChanged();
	uint dim = type.info().dimension;
	for (uint i = type; i < type + dim; ++i)
		m_useParticleData[i] = true;
}

bool CParticleComponent::SetParentComponent(CParticleComponent* pParentComponent, bool delayed)
{
	if (m_parent)
		return false;
	m_parent = pParentComponent;
	if (delayed)
		m_componentParams.m_emitterLifeTime.end = gInfinity;
	auto& children = pParentComponent->m_children;
	auto it = std::find(children.begin(), children.end(), this);
	if (it == children.end())
		children.push_back(this);
	return true;
}

float CParticleComponent::GetEquilibriumTime(Range parentLife) const
{
	Range compLife(
		parentLife.start + m_componentParams.m_emitterLifeTime.start,
		min(parentLife.end, parentLife.start + m_componentParams.m_emitterLifeTime.end) + m_componentParams.m_maxParticleLifeTime);
	float eqTime = std::isfinite(compLife.end) ? 
		compLife.end : 
		compLife.start + (std::isfinite(m_componentParams.m_maxParticleLifeTime) ? m_componentParams.m_maxParticleLifeTime : 0.0f);
	for (const auto& child : m_children)
	{
		if (child->IsEnabled())
		{
			float childEqTime = child->GetEquilibriumTime(compLife);
			eqTime = max(eqTime, childEqTime);
		}
	}
	return eqTime;
}


void CParticleComponent::PrepareRenderObjects(CParticleEmitter* pEmitter)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	for (auto& it : GetUpdateList(EUL_Render))
		it->PrepareRenderObjects(pEmitter, this);
}

void CParticleComponent::ResetRenderObjects(CParticleEmitter* pEmitter)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	for (auto& it : GetUpdateList(EUL_Render))
		it->ResetRenderObjects(pEmitter, this);
}

void CParticleComponent::Render(CParticleEmitter* pEmitter, IParticleComponentRuntime* pRuntime, const SRenderContext& renderContext)
{
	if (IsVisible())
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		const bool isGpuParticles = (pRuntime->GetGpuRuntime() != nullptr);

		for (auto& it : GetUpdateList(EUL_Render))
			it->Render(pEmitter, pRuntime, this, renderContext);		
		
		if (!GetUpdateList(EUL_RenderDeferred).empty() && !isGpuParticles)
		{
			CParticleJobManager& jobManager = GetPSystem()->GetJobManager();
			CParticleComponentRuntime* pCpuRuntime = static_cast<CParticleComponentRuntime*>(pRuntime);
			jobManager.AddDeferredRender(pCpuRuntime, renderContext);
		}
	}
}

void CParticleComponent::RenderDeferred(CParticleEmitter* pEmitter, IParticleComponentRuntime* pRuntime, const SRenderContext& renderContext)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	for (auto& it : GetUpdateList(EUL_RenderDeferred))
		it->Render(pEmitter, pRuntime, this, renderContext);
}

bool CParticleComponent::CanMakeRuntime(CParticleEmitter* pEmitter) const
{
	for (auto& pFeature : m_features)
	{
		if (pFeature->IsEnabled() && !pFeature->CanMakeRuntime(pEmitter))
			return false;
	}
	return true;
}

gpu_pfx2::TFeatures CParticleComponent::GetGpuUpdateList(EUpdateList list) const
{
	return gpu_pfx2::TFeatures(&m_gpuUpdateLists[list][0], (uint)m_gpuUpdateLists[list].size());
}

void CParticleComponent::GetParentData(IParticleEmitter* pEmitter, Array<TParticleId, uint> parentParticleIds, gpu_pfx2::SInitialData data[]) const
{
	CParticleEmitter* pCEmitter = static_cast<CParticleEmitter*>(pEmitter);

	const CParticleContainer& parentContainer = GetParentComponent() ?
		pCEmitter->GetRuntimes()[GetParentComponent()->GetComponentId()].pRuntime->GetCpuRuntime()->GetContainer()
	    : pCEmitter->GetParentContainer();

	IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
	IVec3Stream parentVelocities = parentContainer.GetIVec3Stream(EPVF_Velocity);

	for (uint i = 0; i < parentParticleIds.size(); ++i)
	{
		data[i].position = parentPositions.Load(parentParticleIds[i]);
		data[i].velocity = parentVelocities.Load(parentParticleIds[i]);
	}
}


void CParticleComponent::PreCompile()
{
	if (!m_dirty)
		return;

	m_componentParams.Reset();

	m_parent = nullptr;
	m_children.clear();

	for (size_t i = 0; i < EUL_Count; ++i)
		m_updateLists[i].clear();
	for (size_t i = 0; i < EUL_Count; ++i)
		m_gpuUpdateLists[i].clear();

	for (auto& it : m_features)
	{
		if (it && !it->VersionValidate(this))
			it = nullptr;
	}

	// eliminates features that point to null
	auto it = std::remove_if(
	  m_features.begin(), m_features.end(),
	  [](decltype(*m_features.begin()) pFeature)
		{
			return !pFeature;
	  });
	m_features.erase(it, m_features.end());

	// add default particle data
	m_useParticleData.fill(false);
	AddParticleData(EPDT_ParentId);
	AddParticleData(EPVF_Position);
	AddParticleData(EPVF_Velocity);
	AddParticleData(EPDT_NormalAge);
	AddParticleData(EPDT_InvLifeTime);
	AddParticleData(EPDT_LifeTime);
	AddParticleData(EPDT_State);
}

void CParticleComponent::ResolveDependencies()
{
	if (!m_dirty)
		return;

	m_GPUComponentParams = gpu_pfx2::SComponentParams();

	for (auto& it : m_features)
	{
		if (it->IsEnabled())
			it->ResolveDependency(this);
	}
}

void CParticleComponent::Compile()
{
	if (!m_dirty)
		return;

	uint featureMask = 0;
	for (auto& it : m_features)
	{
		if (it->IsEnabled())
		{
			// Validate feature requirements and exclusivity.
			EFeatureType type = it->GetFeatureType();
			if (type & (EFT_Life | EFT_Motion | EFT_Render))
				if (featureMask & type)
				{
					it->SetEnabled(false);
					continue;
				}
			featureMask |= type;
			it->SetGpuInterfaceNeeded(m_GPUComponentParams.usesGpuImplementation);
			it->AddToComponent(this, &m_componentParams);
		}
	}

	// add default features
	for (uint b = 1; b < EFT_END; b <<= 1)
		if (!(featureMask & b))
			if (auto* params = GetPSystem()->GetDefaultFeatureParam(EFeatureType(b)))
				if (auto* feature = params->m_pFactory())
					static_cast<CParticleFeature*>(feature)->AddToComponent(this, &m_componentParams);
}

void CParticleComponent::FinalizeCompile()
{
	m_componentParams.MakeMaterial(this);
	m_dirty = false;
}

void CParticleComponent::Serialize(Serialization::IArchive& ar)
{
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
	m_componentParams.m_pComponent = this;
	ar(m_componentParams, "Stats", "Component Statistics");
	ar(m_nodePosition, "nodePos", "Node Position");
	ar(m_features, "Features", "^");
	if (ar.isInput())
		SetChanged();
}

void CParticleComponent::SetName(const char* name)
{
	if (!m_pEffect)
	{
		m_name = name;
		return;
	}

	string oldName = m_name;

	m_name = m_pEffect->MakeUniqueName(this, name);

	if (oldName != m_name)
	{
		const uint numComponents = m_pEffect->GetNumComponents();
		for (uint componentIdx = 0; componentIdx < numComponents; ++componentIdx)
		{
			IParticleComponent* pComponent = m_pEffect->GetComponent(componentIdx);
			if (!pComponent)
				continue;
			const uint numFeatures = pComponent->GetNumFeatures();
			for (uint featureIdx = 0; featureIdx < numFeatures; ++featureIdx)
			{
				IParticleFeature* pFeature = pComponent->GetFeature(featureIdx);
				const uint numConnectors = pFeature->GetNumConnectors();
				for (uint connectorIdx = 0; connectorIdx < numConnectors; ++connectorIdx)
				{
					if (oldName == pFeature->GetConnectorName(connectorIdx))
					{
						pFeature->DisconnectFrom(oldName);
						pFeature->ConnectTo(m_name);
					}
				}
			}
		}
	}
}

SERIALIZATION_CLASS_NAME(CParticleComponent, CParticleComponent, "Component", "Component");

}
