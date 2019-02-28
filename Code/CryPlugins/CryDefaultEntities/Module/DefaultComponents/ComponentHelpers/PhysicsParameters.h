// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>

namespace Cry
{
namespace DefaultComponents
{
struct SPhysicsParameters
{
	static constexpr float s_weightTypeEpsilon = 0.0001f;

	enum class EWeightType : uint8
	{
		Mass,
		Density,
		Immovable
	};

	inline bool operator==(const SPhysicsParameters& rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

	void        Serialize(Serialization::IArchive& archive)
	{
		if (!archive.isEdit())
		{
			const bool wasLegacyType = !archive(m_weightType, "WeightType", "Weight Type");
			archive(m_density, "Density", "Density");
			archive(m_mass, "Mass", "Mass");

			if (archive.isInput())
			{
				if (wasLegacyType)
				{
					if (m_mass <= 0 && m_density <= 0)
					{
						m_weightType = EWeightType::Immovable;
					}
					else
					{
						m_weightType = m_density > m_mass ? EWeightType::Density : EWeightType::Mass;
					}
				}
			}
			return;
		}

		archive(m_weightType, "WeightType", "Weight Type");
		archive.doc("Determines whether to use mass, density or make the object immovable when physicalizing the mesh");
		
		switch (m_weightType)
		{
		case EWeightType::Mass:
			{
				archive(m_mass, "Mass", "Mass");
				archive.doc("Mass of the object in kg.");
			}
			break;
		case EWeightType::Density:
			{
				archive(m_density, "Density", "Density");
				archive.doc("Density of the object. Results in the mass being calculated based on the volume of the object.");
			}
			break;
		}
	}

	Schematyc::Range<0, 100000> m_mass = 10.f;
	Schematyc::Range<0, 100000> m_density = s_weightTypeEpsilon;

	EWeightType                 m_weightType = EWeightType::Mass;
};

static void ReflectType(Schematyc::CTypeDesc<SPhysicsParameters::EWeightType>& desc)
{
	desc.SetGUID("{981D050F-37C0-41BE-BF14-716E09772FC3}"_cry_guid);
	desc.SetLabel("Weight Type");
	desc.SetDescription("Determines which value to use to physicalize the mesh");
	desc.SetDefaultValue(SPhysicsParameters::EWeightType::Mass);
	desc.AddConstant(SPhysicsParameters::EWeightType::Mass, "Mass", "Mass");
	desc.AddConstant(SPhysicsParameters::EWeightType::Density, "Density", "Density");
	desc.AddConstant(SPhysicsParameters::EWeightType::Immovable, "Immovable", "Immovable");
}

static void ReflectType(Schematyc::CTypeDesc<SPhysicsParameters>& desc)
{
	desc.SetGUID("{6CE17866-A74D-437D-98DB-F72E7AD7234E}"_cry_guid);
}
}
}