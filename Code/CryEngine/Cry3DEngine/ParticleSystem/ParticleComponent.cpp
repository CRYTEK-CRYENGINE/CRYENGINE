// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     29/01/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleComponent.h"
#include "ParticleSystem.h"
#include "ParticleComponentRuntime.h"
#include "ParticleEmitter.h"
#include "Features/FeatureMotion.h"
#include <CrySerialization/STL.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/SmartPtr.h>
#include <CrySerialization/Math.h>

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
{
	m_usesGPU              = false;
	m_renderObjectFlags    = 0;
	m_instanceDataStride   = 0;
	m_maxParticlesBurst    = 0;
	m_maxParticleSpawnRate = 0;
	m_scaleParticleCount   = 1.0f;
	m_maxParticleLifeTime  = 0.0f;
	m_renderStateFlags     = OS_ALPHA_BLEND;
	m_requiredShaderType   = eST_All;
	m_maxParticleSize      = 0.0f;
	m_meshCentered         = false;
	m_diffuseMap           = "%ENGINE%/EngineAssets/Textures/white.dds";
	m_particleObjFlags     = 0;
	m_renderObjectSortBias = 0.0f;
}

void SComponentParams::Serialize(Serialization::IArchive& ar)
{
	auto* pComponent = static_cast<CParticleComponent*>(ar.context<IParticleComponent>());
	if (!pComponent || !ar.isEdit() || !ar.isOutput())
		return;
	char buffer[1024];

	bool first = true;
	buffer[0] = 0;
	uint32 bytesPerParticle = 0;
	for (auto type : EParticleDataType::values())
	{
		if (pComponent->UseParticleData(type))
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

void SComponentParams::GetMaxParticleCounts(int& total, int& perFrame, float minFPS, float maxFPS) const
{
	if (m_emitterLifeTime.end == 0.0f)
		total = m_maxParticlesBurst;
	else
	{
		float duration = min(m_emitterLifeTime.Length(), m_maxParticleLifeTime);
		float rate = m_maxParticleSpawnRate + m_maxParticlesBurst * maxFPS;
		float count = rate * duration;
		total = int_ceil(count);
	}
	perFrame = max((int)m_maxParticlesBurst, int_ceil(m_maxParticleSpawnRate / minFPS));
	total = int_ceil(total * m_scaleParticleCount * 1.1f);
	perFrame = int_ceil(perFrame * m_scaleParticleCount * 1.1f);
}

//////////////////////////////////////////////////////////////////////////
// CParticleComponent

CParticleComponent::CParticleComponent()
	: m_dirty(true)
	, m_pEffect(0)
	, m_componentId(gInvalidId)
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

IParticleFeature* CParticleComponent::AddFeature(uint placeIdx, const SParticleFeatureParams& featureParams)
{
	IParticleFeature* pNewFeature = (featureParams.m_pFactory)();
	m_features.insert(
	  m_features.begin() + placeIdx,
	  static_cast<CParticleFeature*>(pNewFeature));
	SetChanged();
	return pNewFeature;
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

void CParticleComponent::SetParentComponent(CParticleComponent* pParentComponent, bool delayed)
{
	if (m_parent == pParentComponent)
		return;
	m_parent = pParentComponent;
	if (delayed)
		m_componentParams.m_emitterLifeTime.end = gInfinity;
	auto& children = pParentComponent->m_children;
	stl::push_back_unique(children, this);
}

void CParticleComponent::GetMaxParticleCounts(int& total, int& perFrame, float minFPS, float maxFPS) const
{
 	m_componentParams.GetMaxParticleCounts(total, perFrame, minFPS, maxFPS);
	if (m_parent)
	{
		int totalParent, perFrameParent;
 		m_parent->GetMaxParticleCounts(totalParent, perFrameParent, minFPS, maxFPS);
		total *= totalParent;
		perFrame *= totalParent;
	}
}

float CParticleComponent::GetEquilibriumTime(Range parentLife) const
{
	if (UsesGPU())
		return 0.0f;  // PFX2_TODO

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


void CParticleComponent::RenderAll(CParticleEmitter* pEmitter, CParticleComponentRuntime* pRuntime, const SRenderContext& renderContext)
{
	if (IsVisible())
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		Render(pEmitter, pRuntime, this, renderContext);
		
		if (RenderDeferred.size())
		{
			CParticleJobManager& jobManager = GetPSystem()->GetJobManager();
			CParticleComponentRuntime* pCpuRuntime = static_cast<CParticleComponentRuntime*>(pRuntime);
			jobManager.AddDeferredRender(pCpuRuntime, renderContext);
		}
		if (UsesGPU())
			pRuntime->GetParticleStats().rendered++;
	}
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
	if (!m_dirty)
		return;

	m_componentParams = {};
	m_GPUComponentParams = {};

	m_parent = nullptr;
	m_children.clear();

	static_cast<SFeatureDispatchers&>(*this) = {};
	m_gpuFeatures.clear();

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

	for (auto& it : m_features)
	{
		if (it && it->IsEnabled() && !it->ResolveDependency(this))
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
	GetMaxParticleCounts(m_GPUComponentParams.maxParticles, m_GPUComponentParams.maxNewBorns);
	MakeMaterial();
	m_dirty = false;
}

IMaterial* CParticleComponent::MakeMaterial()
{
	enum EGpuParticlesVertexShaderFlags
	{
		eGpuParticlesVertexShaderFlags_None           = 0x0,
		eGpuParticlesVertexShaderFlags_FacingVelocity = 0x2000
	};

	IMaterial* pMaterial = m_componentParams.m_pMaterial;
	if (m_componentParams.m_requiredShaderType != eST_All)
	{
		if (pMaterial)
		{
			IShader* pShader = pMaterial->GetShaderItem().m_pShader;
			if (!pShader || (pShader->GetFlags() & EF_LOADED && pShader->GetShaderType() != m_componentParams.m_requiredShaderType))
				pMaterial = nullptr;
		}
	}

	if (pMaterial)
		return pMaterial;

	string materialName = string(GetEffect()->GetName()) + ":" + GetName();
	pMaterial = gEnv->p3DEngine->GetMaterialManager()->CreateMaterial(materialName);
	if (gEnv->pRenderer)
	{
		const char* shaderName = UsesGPU() ? "Particles.ParticlesGpu" : "Particles";
		const string& diffuseMap = m_componentParams.m_diffuseMap;
		const uint32 textureLoadFlags = FT_DONT_STREAM;
		const int textureId = gEnv->pRenderer->EF_LoadTexture(diffuseMap.c_str(), textureLoadFlags)->GetTextureID();
		if (textureId <= 0)
			CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "Particle effect texture %s not found", diffuseMap.c_str());

		SInputShaderResourcesPtr pResources = gEnv->pRenderer->EF_CreateInputShaderResource();
		pResources->m_Textures[EFTT_DIFFUSE].m_Name = diffuseMap;
		uint32 mask = eGpuParticlesVertexShaderFlags_None;
		if (GPUComponentParams().facingMode == gpu_pfx2::EFacingMode::Velocity)
			mask |= eGpuParticlesVertexShaderFlags_FacingVelocity;
		SShaderItem shaderItem = gEnv->pRenderer->EF_LoadShaderItem(shaderName, false, 0, pResources, mask);
		pMaterial->AssignShaderItem(shaderItem);

		if (textureId > 0)
			gEnv->pRenderer->RemoveTexture(textureId);
	}
	Vec3 white = Vec3(1.0f, 1.0f, 1.0f);
	float defaultOpacity = 1.0f;
	pMaterial->SetGetMaterialParamVec3("diffuse", white, false);
	pMaterial->SetGetMaterialParamFloat("opacity", defaultOpacity, false);
	pMaterial->RequestTexturesLoading(0.0f);

	return m_componentParams.m_pMaterial = pMaterial;
}

void CParticleComponent::Serialize(Serialization::IArchive& ar)
{
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

	Serialization::SContext context(ar, static_cast<IParticleComponent*>(this));
	ar(m_componentParams, "Stats", "Component Statistics");
	ar(m_nodePosition, "nodePos", "Node Position");
	ar(m_features, "Features", "^");
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

SERIALIZATION_CLASS_NAME(CParticleComponent, CParticleComponent, "Component", "Component");

}
