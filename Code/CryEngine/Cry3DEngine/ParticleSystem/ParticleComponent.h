// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleCommon.h"
#include "ParticleFeature.h"
#include <CryRenderer/IGpuParticles.h>

namespace pfx2
{

class CParticleComponentRuntime;

SERIALIZATION_ENUM_DECLARE(EAnimationCycle, : uint8,
	Once,
	Loop,
	Mirror
)


struct STextureAnimation
{
	UFloat                              m_frameRate;     //!< Anim framerate; 0 = 1 cycle / particle life.
	TValue<uint16, THardLimits<1, 256>> m_frameCount;    //!< Number of tiles (frames) of animation
	EAnimationCycle                     m_cycleMode;     //!< How animation cycles.
	bool                                m_frameBlending; //!< Blend textures between frames.

	// To do: Add random and curve modifiers

	STextureAnimation()
		: m_cycleMode(EAnimationCycle::Once), m_frameBlending(true), m_ageScale(1.0f), m_animPosScale(1.0f)
	{
	}

	bool IsAnimating() const
	{
		return m_frameCount > 1;
	}
	bool HasAbsoluteFrameRate() const
	{
		return m_frameRate > 0.0f;
	}
	float GetAnimPosAbsolute(float age) const
	{
		// Select anim frame based on particle age.
		float animPos = age * m_ageScale;
		switch (m_cycleMode)
		{
		case EAnimationCycle::Once:
			animPos = min(animPos, 1.f);
			break;
		case EAnimationCycle::Loop:
			animPos = mod(animPos, 1.f);
			break;
		case EAnimationCycle::Mirror:
			animPos = 1.f - abs(mod(animPos, 2.f) - 1.f);
			break;
		}
		return animPos * m_animPosScale;
	}
	float GetAnimPosRelative(float relAge) const
	{
		return relAge * m_animPosScale;
	}

	void Serialize(Serialization::IArchive& ar);

private:
	float m_ageScale;
	float m_animPosScale;

	void  Update();
};

SERIALIZATION_ENUM_DEFINE(EIndoorVisibility, ,
	IndoorOnly,
	OutdoorOnly,
	Both
	)

SERIALIZATION_ENUM_DEFINE(EWaterVisibility, ,
	AboveWaterOnly,
	BelowWaterOnly,
	Both
	)

struct SVisibilityParams
{
	UFloat            m_viewDistanceMultiple = 1; // Multiply standard view distance calculated from max particle size and e_ParticlesMinDrawPixels
	UFloat            m_minCameraDistance;
	UInfFloat         m_maxCameraDistance;
	UInfFloat         m_maxScreenSize;            // Override cvar e_ParticlesMaxDrawScreen, fade out near camera
	EIndoorVisibility m_indoorVisibility;
	EWaterVisibility  m_waterVisibility;

	SVisibilityParams()
		: m_indoorVisibility(EIndoorVisibility::Both)
		, m_waterVisibility(EWaterVisibility::Both)
	{}
	void Combine(const SVisibilityParams& o);  // Combination from multiple features chooses most restrictive values
};

struct STimingParams
{
	float m_maxParticleLife = 0;  // Max time a particle can live
	float m_maxTotalLIfe    = 0;  // Max time an emitter can live
	float m_stableTime      = 0;  // Max time for particles, including children, to die
	float m_equilibriumTime = 0;  // Time for emitter to reach equilibrium after activation

	bool  IsImmortal() const { return !std::isfinite(m_maxTotalLIfe); }
};

struct SComponentParams: STimingParams
{
	SComponentParams();

	void  Serialize(Serialization::IArchive& ar);

	bool                      m_usesGPU;
	SParticleShaderData       m_shaderData;
	_smart_ptr<IMaterial>     m_pMaterial;
	_smart_ptr<IMeshObj>      m_pMesh;
	EShaderType               m_requiredShaderType;
	string                    m_diffuseMap;
	uint64                    m_renderObjectFlags;
	size_t                    m_instanceDataStride;
	STextureAnimation         m_textureAnimation;
	uint32                    m_maxParticlesBurst;
	uint32                    m_maxParticlesPerFrame;
	float                     m_maxParticleRate;
	float                     m_scaleParticleCount;
	float                     m_maxParticleSize;
	float                     m_renderObjectSortBias;
	SVisibilityParams         m_visibility;
	int                       m_renderStateFlags;
	uint8                     m_particleObjFlags;
	bool                      m_meshCentered;

	void GetMaxParticleCounts(int& total, int& perFrame, float minFPS = 4.0f, float maxFPS = 120.0f) const;
};

template<typename T> struct TDataOffset
{
	TDataOffset(uint offset = 0) : m_offset(offset) {}
	operator uint() const { return m_offset; }

private:
	uint m_offset;
};

class CParticleComponent : public IParticleComponent, public SFeatureDispatchers
{
public:
	typedef _smart_ptr<CParticleComponent> TComponentPtr;
	typedef std::vector<TComponentPtr>     TComponents;

	CParticleComponent();

	// IParticleComponent
	virtual void                SetChanged() override;
	virtual void                SetEnabled(bool enabled) override                         { SetChanged(); m_enabled.Set(enabled); }
	virtual bool                IsEnabled() const override                                { return m_enabled; }
	virtual bool                IsVisible() const override                                { return m_visible; }
	virtual void                SetVisible(bool visible) override                         { m_visible.Set(visible); }
	virtual void                Serialize(Serialization::IArchive& ar) override;
	virtual void                SetName(cstr name) override;
	virtual cstr                GetName() const override                                  { return m_name; }
	virtual uint                GetNumFeatures() const override                           { return m_features.size(); }
	virtual IParticleFeature*   GetFeature(uint featureIdx) const override;
	virtual IParticleFeature*   AddFeature(uint placeIdx, const SParticleFeatureParams& featureParams) override;
	virtual void                RemoveFeature(uint featureIdx) override;
	virtual void                SwapFeatures(const uint* swapIds, uint numSwapIds) override;
	virtual IParticleComponent* GetParent() const override                                { return m_parent; }
	virtual void                SetParent(IParticleComponent* pParentComponent) override;
	virtual Vec2                GetNodePosition() const override;
	virtual void                SetNodePosition(Vec2 position) override;
	// ~IParticleComponent

	void                                  ClearFeatures()                       { m_features.clear(); }
	void                                  AddFeature(CParticleFeature* pFeature);
	void                                  PreCompile();
	void                                  ResolveDependencies();
	void                                  Compile();
	void                                  FinalizeCompile();
	IMaterial*                            MakeMaterial();

	uint                                  GetComponentId() const                { return m_componentId; }
	CParticleEffect*                      GetEffect() const                     { return m_pEffect; }
	void                                  SetEffect(CParticleEffect* pEffect)   { m_pEffect = pEffect; }

	template<typename T> TDataOffset<T>   AddInstanceData()                     { return AddInstanceData(sizeof(T)); }
	void                                  AddParticleData(EParticleDataType type);

	bool                                  UsesGPU() const                       { return m_Params.m_usesGPU; }
	gpu_pfx2::SComponentParams&           GPUComponentParams()                  { return m_GPUParams; };
	void                                  AddGPUFeature(gpu_pfx2::IParticleFeature* gpuInterface) { if (gpuInterface) m_gpuFeatures.push_back(gpuInterface); }
	TConstArray<gpu_pfx2::IParticleFeature*> GetGpuFeatures() const             { return { &*m_gpuFeatures.begin(), &*m_gpuFeatures.end() }; }

	const SComponentParams& GetComponentParams() const                          { return m_Params; }
	SComponentParams&       ComponentParams()                                   { return m_Params; }
	bool                    UseParticleData(EParticleDataType type) const       { return m_useParticleData[type]; }

	CParticleComponent*     GetParentComponent() const                          { return m_parent; }
	const TComponents&      GetChildComponents() const                          { return m_children; }
	void                    ClearChildren()                                     { m_children.resize(0); }

	void                    GetMaxParticleCounts(int& total, int& perFrame, float minFPS = 4.0f, float maxFPS = 120.0f) const;
	void                    UpdateTimings();

	void                    RenderAll(CParticleEmitter* pEmitter, CParticleComponentRuntime* pRuntime, const SRenderContext& renderContext);
	bool                    CanMakeRuntime(CParticleEmitter* pEmitter) const;

private:
	uint AddInstanceData(uint size);

	friend class CParticleEffect;

	string                                   m_name;
	CParticleEffect*                         m_pEffect;
	uint                                     m_componentId;
	CParticleComponent*                      m_parent;
	TComponents                              m_children;
	Vec2                                     m_nodePosition;
	SComponentParams                         m_Params;
	std::vector<TParticleFeaturePtr>         m_features;
	std::vector<TParticleFeaturePtr>         m_defaultFeatures;
	StaticEnumArray<bool, EParticleDataType> m_useParticleData;
	SEnable                                  m_enabled;
	SEnable                                  m_visible;
	bool                                     m_dirty;

	gpu_pfx2::SComponentParams               m_GPUParams;
	std::vector<gpu_pfx2::IParticleFeature*> m_gpuFeatures;
};

typedef _smart_ptr<CParticleComponent> TComponentPtr;
typedef std::vector<TComponentPtr>     TComponents;

}

