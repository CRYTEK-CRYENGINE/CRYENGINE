// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Factions/FactionSystem.h"

#include <CryAISystem/VisionMapTypes.h>
#include <CrySerialization/Decorators/BitFlags.h>

namespace Perception
{
namespace ComponentHelpers
{

struct SLocation
{
	static void ReflectType(Schematyc::CTypeDesc<SLocation>& typeInfo)
	{
		typeInfo.SetGUID("80827c81-dd8f-4020-b033-163f6a130ede"_cry_guid);
	}
	void Serialize(Serialization::IArchive& archive)
	{
		archive(type, "type", "Location Type");
		archive.doc("How the location is defined.");

		switch (type)
		{
		case SLocation::EType::Bone:
			archive(boneName, "boneName", "Bone Name");
			archive.doc("The name of the bone/joint from which to take the location.");
			if (boneName.empty())
			{
				archive.error(boneName, "Bone name is empty!");
			}
			break;
		case SLocation::EType::Pivot:
			// Nothing special to serialize
			break;
		default:
			CRY_ASSERT(false);     // We should never get here!
			break;
		}
		static_assert(int(SLocation::EType::Count) == 2, "Unexpected enum count");

		archive(Serialization::SPosition(offset), "offset", "Offset");
		archive.doc("A fixed offset from position.");
	}

	enum class EType
	{
		Pivot,
		Bone,
		Count,
	};

	bool operator==(const SLocation& other) const
	{
		return type == other.type && offset == other.offset && boneName == other.boneName;
	}

	EType  type = EType::Pivot;
	Vec3   offset = ZERO;
	string boneName;

	bool Validate(IEntity* pEntity, const char* szComponentName) const;
	void GetPositionAndOrientation(IEntity* pEntity, Vec3* pOutPosition, Vec3* pOutOrientation) const;

private:
	bool GetTransformFromPivot(IEntity* pEntity, Vec3* pOutPosition, Vec3* pOutOrientation) const;
	bool GetTransformFromBone(IEntity* pEntity, Vec3* pOutPosition, Vec3* pOutOrientation) const;
};

struct SLocationsArray
{
	static void ReflectType(Schematyc::CTypeDesc<SLocationsArray>& typeInfo)
	{
		typeInfo.SetGUID("3542865D-DE8F-4496-97A2-4FA59D9C9440"_cry_guid);
	}
	bool operator==(const SLocationsArray& other) const { return locations == other.locations; }

	std::vector<SLocation> locations;
};

inline bool Serialize(Serialization::IArchive& archive, SLocationsArray& value, const char* szName, const char* szLabel)
{
	return archive(value.locations, szName, szLabel);
}

//////////////////////////////////////////////////////////////////////////

struct SVisionMapType
{
	static void ReflectType(Schematyc::CTypeDesc<SVisionMapType>& typeInfo)
	{
		typeInfo.SetGUID("5dc7e1c7-fc02-42ae-988e-606e6f876785"_cry_guid);
	}
	bool operator==(const SVisionMapType& other) const { return mask == other.mask; }
	uint32 mask = 0;
};

inline bool Serialize(Serialization::IArchive& archive, SVisionMapType& value, const char* szName, const char* szLabel)
{
	return archive(Serialization::BitFlags<VisionMapTypes>(value.mask), szName, szLabel);
}

}   //! ComponentHelpers

} //! Perception
