// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "ParticleSystem/ParticleCommon.h"
#include "ParticleSystem/ParticleAttributes.h"
#include "ParticleSystem/ParticleEmitter.h"
#include "ParamMod.h"
#include "FeatureColor.h"

namespace pfx2
{

SERIALIZATION_ENUM_DECLARE(ETimeSource, ,
                           Age,
                           SpawnFraction,
                           Speed,
                           Field,
                           Attribute,
                           LevelTime,
                           ViewAngle,
	                       CameraDistance,
                           Random,

                           // old version
                           _ParentTime,
                           _ParentOrder,
                           _ParentSpeed,
                           _ParentField,

                           _SelfTime = Age,
                           _SelfSpeed = Speed
                           )

SERIALIZATION_ENUM_DECLARE(ETimeSourceOwner, ,
                           _None,
                           Self,
                           Parent
                           )

typedef DynamicEnum<struct STimeSourceField> ETimeSourceField;
bool Serialize(Serialization::IArchive& ar, ETimeSourceField& value, cstr name, cstr label);


class CTimeSource
{
public:
	CTimeSource();

	template<typename TParam, typename TMod>
	void              AddToParam(CParticleComponent* pComponent, TParam* pParam, TMod* pModifier);
	template<typename TBase, typename TStream>
	void              Dispatch(const SUpdateContext& context, const SUpdateRange& range, TStream stream, EModDomain domain) const;

	EModDomain        GetDomain() const;
	EParticleDataType GetDataType() const;
	string            GetSourceDescription() const;
	float             Adjust(float sample) const { return sample * m_timeScale + m_timeBias; }
	void              SerializeInplace(Serialization::IArchive& ar);

protected:
	float m_timeScale;
	float m_timeBias;

private:
	IParamModContext& GetContext(Serialization::IArchive& ar) const;

	string           m_attributeName;
	ETimeSource      m_timeSource;
	ETimeSourceField m_fieldSource;
	ETimeSourceOwner m_sourceOwner;
	bool             m_spawnOnly;
};

}

#include "TimeSourceImpl.h"
