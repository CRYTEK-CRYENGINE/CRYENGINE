// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "TimeSource.h"

CRY_PFX2_DBG

namespace pfx2
{


bool Serialize(Serialization::IArchive& ar, ETimeSourceField& value, cstr name, cstr label)
{
	// Populate enum on first serialization call.
	if (!ETimeSourceField::count())
	{
		for (auto type : EParticleDataType::values())
			if (type.info().isType<float>(1) && type != EPDT_NormalAge && type != EPDT_SpawnFraction && type != EPDT_InvLifeTime)
				ETimeSourceField::container().add(type, type.name(), type.label());
	}

	return value.container().Serialize(ar, reinterpret_cast<ETimeSourceField::Value&>(value), name, label);
}

void CTimeSource::SerializeInplace(Serialization::IArchive& ar)
{
	const auto& context = GetContext(ar);

	bool patchedTimeSource = false;
	if (ar.isInput() && GetVersion(ar) < 7)
	{
		string field;
		const bool hasField = ar(field, "Field");
		if (hasField && field == "Age")
		{
			m_timeSource = ETimeSource::Age;
			string source;
			ar(source, "Source");
			if (source == "Field")
				m_sourceOwner = ETimeSourceOwner::Self;
			else if (source == "ParentField")
				m_sourceOwner = ETimeSourceOwner::Parent;
			patchedTimeSource = true;
		}
	}
	if (!patchedTimeSource)
		ar(m_timeSource, "TimeSource", "^>120>");

	// Read or set related parameters
	switch (m_timeSource)
	{
	case ETimeSource::Field:
		ar(m_fieldSource, "Field", "Field");
	// continue
	case ETimeSource::Age:
	case ETimeSource::SpawnFraction:
	case ETimeSource::Speed:
		if (m_sourceOwner == ETimeSourceOwner::_None)
			m_sourceOwner = ETimeSourceOwner::Self;
		if (context.GetDomain() == EMD_PerParticle)
			ar(m_sourceOwner, "Owner", "Owner");
		break;
	case ETimeSource::Attribute:
		ar(m_attributeName, "AttributeName", "Attribute Name");
		m_sourceOwner = ETimeSourceOwner::_None;
		break;
	case ETimeSource::LevelTime:
		m_sourceOwner = ETimeSourceOwner::_None;
		break;
	case ETimeSource::_ParentTime:
		m_timeSource = ETimeSource::Age;
		m_sourceOwner = ETimeSourceOwner::Parent;
		break;
	case ETimeSource::_ParentOrder:
		m_timeSource = ETimeSource::SpawnFraction;
		m_sourceOwner = ETimeSourceOwner::Parent;
		break;
	case ETimeSource::_ParentSpeed:
		m_timeSource = ETimeSource::Speed;
		m_sourceOwner = ETimeSourceOwner::Parent;
		break;
	case ETimeSource::_ParentField:
		m_timeSource = ETimeSource::Field;
		m_sourceOwner = ETimeSourceOwner::Parent;
		ar(m_fieldSource, "Field", "Field");
		break;
	}

	if (ar.isInput() && GetVersion(ar) <= 8)
	{
		ar(m_timeScale, "Scale", "Scale");
		ar(m_timeBias, "Bias", "Bias");
	}
	else
	{
		ar(m_timeScale, "TimeScale", "Time Scale");
		ar(m_timeBias, "TimeBias", "Time Bias");
	}

	if (!context.HasUpdate() || m_timeSource == ETimeSource::Random)
		m_spawnOnly = true;
	else if (context.GetDomain() == EMD_PerParticle && m_timeSource == ETimeSource::Age && m_sourceOwner == ETimeSourceOwner::Self || m_timeSource == ETimeSource::ViewAngle || m_timeSource == ETimeSource::CameraDistance)
		m_spawnOnly = false;
	else
		ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
}

string CTimeSource::GetSourceDescription() const
{
	string desc;
	if (m_sourceOwner == ETimeSourceOwner::Parent)
		desc = "Parent ";

	if (m_timeSource == ETimeSource::Attribute)
		desc += "Attribute: ", desc += m_attributeName;
	else if (m_timeSource == ETimeSource::Field)
		desc += Serialization::getEnumLabel(m_fieldSource);
	else
		desc += Serialization::getEnumLabel(m_timeSource);

	return desc;
}


}
