// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
// SModuleParams

SComponentParams::SComponentParams()
{
	m_usesGPU              = false;
	m_renderObjectFlags    = 0;
	m_instanceDataStride   = 0;
	m_maxParticlesBurst    = 0;
	m_maxParticlesPerFrame = 0;
	m_maxParticleRate      = 0.0f;
	m_scaleParticleCount   = 1.0f;
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
			bytesPerParticle += type.info().typeSize * type.info().step();
		}
	}
	ar(string(buffer), "", "!Fields used:");

	cry_sprintf(buffer, "%d", bytesPerParticle);
	ar(string(buffer), "", "!Bytes per Particle:");
}

void SComponentParams::GetMaxParticleCounts(int& total, int& perFrame, float minFPS, float maxFPS) const
{
	total = m_maxParticlesBurst;
	const float rate = m_maxParticleRate + m_maxParticlesPerFrame * maxFPS;
	const float extendedLife = m_maxParticleLife + rcp(minFPS); // Particles stay 1 frame after death
	if (rate > 0.0f && std::isfinite(extendedLife))
		total += int_ceil(rate * extendedLife);
	perFrame = int(m_maxParticlesBurst + m_maxParticlesPerFrame) + int_ceil(m_maxParticleRate / minFPS);
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

uint CParticleComponent::AddInstanceData(uint size)
{
	CRY_PFX2_ASSERT(size > 0);        // instance data of 0 bytes makes no sense
	SetChanged();
	uint offset = m_Params.m_instanceDataStride;
	m_Params.m_instanceDataStride += size;
	return offset;
}

void CParticleComponent::AddParticleData(EParticleDataType type)
{
	SetChanged();
	uint dim = type.info().dimension;
	for (uint i = type; i < type + dim; ++i)
		m_useParticleData[i] = true;
}

void CParticleComponent::SetParent(IParticleComponent* pParentComponent)
{
	if (m_parent == pParentComponent)
		return;
	if (m_parent)
		stl::find_and_erase(m_parent->m_children, this);
	m_parent = static_cast<CParticleComponent*>(pParentComponent);
	if (m_parent)
		stl::push_back_unique(m_parent->m_children, this);
}

void CParticleComponent::GetMaxParticleCounts(int& total, int& perFrame, float minFPS, float maxFPS) const
{
 	m_Params.GetMaxParticleCounts(total, perFrame, minFPS, maxFPS);
	if (m_parent)
	{
		int totalParent, perFrameParent;
 		m_parent->GetMaxParticleCounts(totalParent, perFrameParent, minFPS, maxFPS);
		total *= totalParent;
		perFrame *= totalParent;
	}
}

void CParticleComponent::UpdateTimings()
{
	// Adjust parent lifetimes to include child lifetimes
	STimingParams params {};
	float maxChildEq = 0.0f, maxChildLife = 0.0f;
	for (auto& pChild : m_children)
	{
		pChild->UpdateTimings();
		const STimingParams& timingsChild = pChild->ComponentParams();
		SetMax(maxChildEq, timingsChild.m_equilibriumTime);
		SetMax(maxChildLife, timingsChild.m_maxTotalLIfe);
	}

	const float moreEq = maxChildEq - FiniteOr(m_Params.m_maxParticleLife, 0.0f);
	if (moreEq > 0.0f)
	{
		m_Params.m_stableTime      += moreEq ;
		m_Params.m_equilibriumTime += moreEq ;
	}
	const float moreLife = maxChildLife - FiniteOr(m_Params.m_maxParticleLife, 0.0f);
	if (moreLife > 0.0f)
	{
		m_Params.m_maxTotalLIfe += moreLife;
	}
}

void CParticleComponent::RenderAll(CParticleEmitter* pEmitter, CParticleComponentRuntime* pRuntime, const SRenderContext& renderContext)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (auto* pGPURuntime = pRuntime->GetGpuRuntime())
	{
		SParticleStats stats;
		pGPURuntime->AccumStats(stats);
		auto& statsGPU = GetPSystem()->GetThreadData().statsGPU;
		statsGPU.components.rendered += stats.components.rendered;
		statsGPU.particles.rendered += stats.particles.rendered;
	}

	Render(pEmitter, pRuntime, this, renderContext);
		
	if (RenderDeferred.size())
	{
		CParticleJobManager& jobManager = GetPSystem()->GetJobManager();
		CParticleComponentRuntime* pCpuRuntime = static_cast<CParticleComponentRuntime*>(pRuntime);
		jobManager.AddDeferredRender(pCpuRuntime, renderContext);
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

	m_Params = {};
	m_GPUParams = {};

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
			it->AddToComponent(this, &m_Params);
		}
	}

	// add default features
	for (uint b = 1; b < EFT_END; b <<= 1)
	{
		if (!(featureMask & b))
		{
			if (EFeatureType(b) != EFT_Child || m_parent)
			{
				if (auto* params = GetPSystem()->GetDefaultFeatureParam(EFeatureType(b)))
				{
					if (auto* feature = params->m_pFactory())
					{
						m_defaultFeatures.push_back(pfx2::TParticleFeaturePtr(static_cast<CParticleFeature*>(feature)));
						static_cast<CParticleFeature*>(feature)->AddToComponent(this, &m_Params);
					}
				}
			}
		}
	}
}

void CParticleComponent::FinalizeCompile()
{
	GetMaxParticleCounts(m_GPUParams.maxParticles, m_GPUParams.maxNewBorns);
	m_GPUParams.maxParticles += m_GPUParams.maxParticles >> 3;
	m_GPUParams.maxNewBorns  += m_GPUParams.maxNewBorns  >> 3;
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

	IMaterial* pMaterial = m_Params.m_pMaterial;
	if (m_Params.m_requiredShaderType != eST_All)
	{
		if (pMaterial)
		{
			IShader* pShader = pMaterial->GetShaderItem().m_pShader;
			if (!pShader || (pShader->GetFlags() & EF_LOADED && pShader->GetShaderType() != m_Params.m_requiredShaderType))
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
		const string& diffuseMap = m_Params.m_diffuseMap;
		static uint32 textureLoadFlags = 0;//FT_DONT_STREAM;
		ITexture* pTexture = gEnv->pRenderer->EF_GetTextureByName(diffuseMap.c_str(), textureLoadFlags);
		if (!pTexture)
		{
			GetPSystem()->CheckFileAccess(diffuseMap.c_str());
			pTexture = gEnv->pRenderer->EF_LoadTexture(diffuseMap.c_str(), textureLoadFlags);
		}
		if (pTexture->GetTextureID() <= 0)
			CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "Particle effect texture %s not found", diffuseMap.c_str());

		SInputShaderResourcesPtr pResources = gEnv->pRenderer->EF_CreateInputShaderResource();
		pResources->m_Textures[EFTT_DIFFUSE].m_Name = diffuseMap;
		uint32 mask = eGpuParticlesVertexShaderFlags_None;
		if (GPUComponentParams().facingMode == gpu_pfx2::EFacingMode::Velocity)
			mask |= eGpuParticlesVertexShaderFlags_FacingVelocity;
		SShaderItem shaderItem = gEnv->pRenderer->EF_LoadShaderItem(shaderName, false, 0, pResources, mask);
		pMaterial->AssignShaderItem(shaderItem);
	}
	Vec3 white = Vec3(1.0f, 1.0f, 1.0f);
	float defaultOpacity = 1.0f;
	pMaterial->SetGetMaterialParamVec3("diffuse", white, false);
	pMaterial->SetGetMaterialParamFloat("opacity", defaultOpacity, false);

	return m_Params.m_pMaterial = pMaterial;
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
	ar(m_Params, "Stats", "Component Statistics");
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
