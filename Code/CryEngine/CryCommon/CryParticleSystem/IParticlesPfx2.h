// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IParticles.h"
#include "CryExtension/ICryUnknown.h"
#include "CryExtension/CryCreateClassInstance.h"

namespace pfx2
{

template<typename T> using TVarArray   = Array<T, uint>;
template<typename T> using TConstArray = Array<const T, uint>;

struct IParticleFeature;

typedef Serialization::IArchive IArchive;

template<typename F>
struct TParticleStats
	: INumberVector<F, 17, TParticleStats<F>>
{
	TElementCounts<F> emitters;
	TElementCounts<F> components;

	struct ParticleStats
		: INumberVector<F, 5, ParticleStats>
		, TElementCountsBase<F>
	{
		F culled = 0;
	} particles;
	TElementCounts<F> pixels;
};

typedef TParticleStats<uint> SParticleStats;

struct SParticleFeatureParams
{
	const char*       m_fullName;
	const char*       m_groupName;
	const char*       m_featureName;
	ColorB            m_color;
	IParticleFeature* (*m_pFactory)();
	bool              m_hasComponentConnector;
	uint              m_defaultForType;
};

typedef uint32 TParticleId;

struct IParticleFeature : public _i_reference_target_t
{
	virtual bool                          IsEnabled() const = 0;
	virtual void                          SetEnabled(bool enabled) = 0;
	virtual void                          Serialize(IArchive& ar) = 0;
	virtual const SParticleFeatureParams& GetFeatureParams() const = 0;
	virtual uint                          GetNumConnectors() const = 0;
	virtual const char*                   GetConnectorName(uint connectorId) const = 0;
	virtual void                          ConnectTo(const char* pOtherName) = 0;
	virtual void                          DisconnectFrom(const char* pOtherName) = 0;
	virtual uint                          GetNumResources() const = 0;
	virtual const char*                   GetResourceName(uint resourceId) const = 0;
};

struct TParticleFeatures: std::vector<_smart_ptr<IParticleFeature>>
{
	uint m_editVersion = 0;
};


struct IParticleComponent : public _i_reference_target_t
{
	virtual void                SetChanged() = 0;
	virtual bool                IsEnabled() const = 0;
	virtual void                SetEnabled(bool enabled) = 0;
	virtual bool                IsVisible() const = 0;
	virtual void                SetVisible(bool visible) = 0;
	virtual void                Serialize(IArchive& ar) = 0;
	virtual void                SetName(const char* name) = 0;
	virtual const char*         GetName() const = 0;
	virtual uint                GetNumFeatures() const = 0;
	virtual IParticleFeature*   GetFeature(uint featureIdx) const = 0;
	virtual IParticleFeature*   AddFeature(uint placeIdx, const SParticleFeatureParams& featureParams) = 0;
	virtual void                RemoveFeature(uint featureIdx) = 0;
	virtual void                SwapFeatures(const uint* swapIds, uint numSwapIds) = 0;
	virtual IParticleComponent* GetParent() const = 0;
	virtual void                SetParent(IParticleComponent* parent) = 0;
	virtual Vec2                GetNodePosition() const = 0;
	virtual void                SetNodePosition(Vec2 position) = 0;
};

struct SSpawnEntry
{
	uint32 m_count;
	uint32 m_parentId;
	float  m_ageBegin;
	float  m_ageIncrement;
	float  m_fractionBegin;
	float  m_fractionIncrement;
};

struct SParentData
{
	Vec3 position;
	Vec3 velocity;
};

struct IParticleEffectPfx2 : public IParticleEffect
{
	virtual void                   SetChanged() = 0;
	virtual void                   Update() = 0;
	virtual uint                   GetNumComponents() const = 0;
	virtual IParticleComponent*    GetComponent(uint componentIdx) const = 0;
	virtual IParticleComponent*    AddComponent() = 0;
	virtual void                   RemoveComponent(uint componentIdx, bool all = false) = 0;
	virtual Serialization::SStruct GetEffectOptionsSerializer() const = 0;
	virtual TParticleAttributesPtr CreateAttributesInstance() const = 0;
	virtual bool                   IsSubstitutedPfx1() const = 0;
	virtual void                   SetSubstitutedPfx1(bool b) = 0;
};

using PParticleEffect     = _smart_ptr<IParticleEffectPfx2>;
using PParticleEmitter    = _smart_ptr<IParticleEmitter>;

struct IParticleSystem : public ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(IParticleSystem, "cc3aa21d-44a2-4ded-9aa1-daded21d570a"_cry_guid);

	virtual PParticleEffect         CreateEffect() = 0;
	virtual PParticleEffect         ConvertEffect(const IParticleEffect* pOldEffect, bool bReplace = false) = 0;
	virtual void                    RenameEffect(PParticleEffect pEffect, cstr name) = 0;
	virtual PParticleEffect         FindEffect(cstr name, bool bAllowLoad = true) = 0;
	virtual PParticleEmitter        CreateEmitter(PParticleEffect pEffect) = 0;
	virtual uint                    GetNumFeatureParams() const = 0;
	virtual SParticleFeatureParams& GetFeatureParam(uint featureIdx) const = 0;
	virtual TParticleAttributesPtr  CreateParticleAttributes() const = 0;

	virtual void                    OnFrameStart() = 0;
	virtual void                    Update() = 0;
	virtual void                    FinishRenderTasks(const SRenderingPassInfo& passInfo) = 0;
	virtual void                    Reset() = 0;
	virtual void                    ClearRenderResources() = 0;

	virtual void                    Serialize(TSerialize ser) = 0;
	virtual bool                    SerializeFeatures(IArchive& ar, TParticleFeatures& features, cstr name, cstr label) const = 0;

	virtual void                    GetStats(SParticleStats& stats) = 0;
	virtual void                    DisplayStats(Vec2& location, float lineHeight) = 0;
	virtual void                    GetMemoryUsage(ICrySizer* pSizer) const = 0;
};

static std::shared_ptr<IParticleSystem> GetIParticleSystem()
{
	static std::shared_ptr<IParticleSystem> pParticleSystem;
	static bool created = CryCreateClassInstanceForInterface(cryiidof<IParticleSystem>(), pParticleSystem);
	return pParticleSystem;
}

}
