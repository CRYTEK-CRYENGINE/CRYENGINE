// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Domain.h"
#include "../ParticleSystem.h"
#include "../ParticleComponentRuntime.h"
#include "TimeOfDay.h"

namespace pfx2
{


bool Serialize(Serialization::IArchive& ar, EDomainField& value, cstr name, cstr label)
{
	// Populate enum on first serialization call.
	if (!EDomainField::count())
	{
		for (auto type : EParticleDataType::values())
			if (type.info().isType<float>(1) && type != EPDT_NormalAge && type != EPDT_SpawnFraction && type != EPDT_InvLifeTime)
				EDomainField::container().add(type, type.name(), type.label());
	}

	return value.container().Serialize(ar, reinterpret_cast<EDomainField::Value&>(value), name, label);
}

void CDomain::SerializeInplace(Serialization::IArchive& ar)
{
	const EDataDomain domain = *ar.context<EDataDomain>();
	const uint version = GetVersion(ar);

	bool patchedDomain = false;
	if (ar.isInput() && version < 7)
	{
		string field;
		const bool hasField = ar(field, "Field");
		if (hasField && field == "Age")
		{
			m_domain = EDomain::Age;
			string source;
			ar(source, "Source");
			if (source == "Field")
				m_sourceOwner = EDomainOwner::Self;
			else if (source == "ParentField")
				m_sourceOwner = EDomainOwner::Parent;
			patchedDomain = true;
		}
	}
	if (!patchedDomain)
		ar(m_domain, "TimeSource", "^>120>");

	// Read or set related parameters
	switch (m_domain)
	{
	case EDomain::Field:
		ar(m_fieldSource, "Field", "Field");
	// continue
	case EDomain::Age:
	case EDomain::SpawnFraction:
	case EDomain::Speed:
		if (m_sourceOwner == EDomainOwner::_None)
			m_sourceOwner = EDomainOwner::Self;
		if (domain & EDD_PerParticle)
			ar(m_sourceOwner, "Owner", "Owner");
		break;
	case EDomain::Attribute:
		ar(m_attribute, "AttributeName", "Attribute Name");
		m_sourceOwner = EDomainOwner::_None;
		break;
	case EDomain::Global:
		ar(m_sourceGlobal, "SourceGlobal", "Source");
		break;

	case EDomain::_ParentTime:
		m_domain = EDomain::Age;
		m_sourceOwner = EDomainOwner::Parent;
		break;
	case EDomain::_ParentOrder:
		m_domain = EDomain::SpawnFraction;
		m_sourceOwner = EDomainOwner::Parent;
		break;
	case EDomain::_ParentSpeed:
		m_domain = EDomain::Speed;
		m_sourceOwner = EDomainOwner::Parent;
		break;
	case EDomain::_ParentField:
		m_domain = EDomain::Field;
		m_sourceOwner = EDomainOwner::Parent;
		ar(m_fieldSource, "Field", "Field");
		break;
	}
		
	if (ar.isInput() && version < 9)
	{
		ar(m_domainScale, "Scale");
		ar(m_domainBias, "Bias");
	}
	else if (ar.isInput() && version < 10)
	{
		ar(m_domainScale, "TimeScale");
		ar(m_domainBias, "TimeBias");
	}
	else
	{
		ar(m_domainScale, "DomainScale", "Domain Scale");
		ar(m_domainBias, "DomainBias", "Domain Bias");
	}

	if (!(domain & EDD_HasUpdate) || m_domain == EDomain::Random)
		m_spawnOnly = true;
	else if (domain & EDD_PerParticle && m_domain == EDomain::Age && m_sourceOwner == EDomainOwner::Self || m_domain == EDomain::ViewAngle || m_domain == EDomain::CameraDistance)
		m_spawnOnly = false;
	else
		ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
}

string CDomain::GetSourceDescription() const
{
	string desc;
	if (m_sourceOwner == EDomainOwner::Parent)
		desc = "Parent ";

	if (m_domain == EDomain::Attribute)
		desc += "Attribute: ", desc += m_attribute.Name().c_str();
	else if (m_domain == EDomain::Field)
		desc += Serialization::getEnumLabel(m_fieldSource);
	else
		desc += Serialization::getEnumLabel(m_domain);

	return desc;
}

float CDomain::GetGlobalValue(EDomainGlobal source) const
{
	C3DEngine* p3DEngine((C3DEngine*)gEnv->p3DEngine);
	switch (source)
	{
	case EDomainGlobal::TimeOfDay:
		return gEnv->p3DEngine->GetTimeOfDay()->GetTime() / 24.0f;
	case EDomainGlobal::ExposureValue:
	{
		Vec3 exposure;
		p3DEngine->GetGlobalParameter(E3DPARAM_HDR_EYEADAPTATION_PARAMS, exposure);
		const float minEV = exposure.x;
		const float maxEV = exposure.y;
		const float evCompensation = exposure.z;
		return Lerp(minEV, maxEV, 1.0f - std::pow(0.5f, evCompensation));
	}
	}
	return 0.0f;
}

namespace detail
{
	CAttributeSampler::CAttributeSampler(CParticleComponentRuntime& runtime, const CAttributeReference& attr)
	{
		const CAttributeInstance& attributes = runtime.GetEmitter()->GetAttributeInstance();
		m_attributeValue = ToFloatv(attr.GetValueAs(attributes, 1.0f));
	}
}

}
