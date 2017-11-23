// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "NavigationSystem/NavigationSystem.h"
#include <CryAISystem/Serialization/NavigationSerialize.h>

// Reflection of types defined elsewhere

//////////////////////////////////////////////////////////////////////////

inline void ReflectType(Schematyc::CTypeDesc<NavigationVolumeID>& desc)
{
	desc.SetGUID("21A0EEF4-4EA7-4445-BF9E-E5A5BB66757E"_cry_guid);
	desc.SetDefaultValue(NavigationVolumeID());
}

//////////////////////////////////////////////////////////////////////////

inline void NavigationAgentTypeIDToString(Schematyc::IString& output, const NavigationAgentTypeID& input)
{
	output.assign(gAIEnv.pNavigationSystem->GetAgentTypeName(input));
}

inline void ReflectType(Schematyc::CTypeDesc<NavigationAgentTypeID>& desc)
{
	desc.SetGUID("52fb5a3e-b615-496b-a7b2-e9dfe483f755"_cry_guid);
	desc.SetLabel("NavAgentType");
	desc.SetDescription("NavigationAgentTypeId");
	desc.SetDefaultValue(NavigationAgentTypeID());
	desc.SetToStringOperator<& NavigationAgentTypeIDToString>();
}

//////////////////////////////////////////////////////////////////////////
inline void NavigationAreaTypeIDToString(Schematyc::IString& output, const NavigationAreaTypeID& input)
{
	output.assign(gAIEnv.pNavigationSystem->GetAnnotations().GetAreaTypeName(input));
}

inline void ReflectType(Schematyc::CTypeDesc<NavigationAreaTypeID>& desc)
{
	desc.SetGUID("B96DEA46-DBB8-43BE-923B-F483596D4DD1"_cry_guid);
	desc.SetLabel("NavAreaType");
	desc.SetDescription("NavigationAreaTypeId");
	desc.SetDefaultValue(NavigationAreaTypeID());
	desc.SetToStringOperator<& NavigationAreaTypeIDToString>();
}

//////////////////////////////////////////////////////////////////////////
static void ReflectType(Schematyc::CTypeDesc<NavigationAreaFlagID>& desc)
{
	desc.SetGUID("C4B6D015-4484-4C9A-A740-83CC5742128B"_cry_guid);
	desc.SetLabel("Navigation Area Flag");
	desc.SetDescription("Navigation Area Flag");
}

//////////////////////////////////////////////////////////////////////////
inline void ReflectType(Schematyc::CTypeDesc<SNavMeshQueryFilterDefault>& desc)
{
	desc.SetGUID("BC7B6732-BA06-4C76-98AA-219E7775D989"_cry_guid);
	desc.SetLabel("Navigation Query Filter");
	desc.SetDescription("Navigation Query Filter");
	desc.SetDefaultValue(SNavMeshQueryFilterDefault());
}

//////////////////////////////////////////////////////////////////////////
namespace NavigationComponentHelpers
{
struct SAgentTypesMask
{
	uint32      mask;
	SAgentTypesMask() : mask(-1) {}
	void        Serialize(Serialization::IArchive& ar);
	bool        operator==(const SAgentTypesMask& other) const { return other.mask == mask; }

	static void ReflectType(Schematyc::CTypeDesc<SAgentTypesMask>& desc)
	{
		desc.SetGUID("746F16BE-44CA-4231-8A01-EF7CA66265C9"_cry_guid);
		desc.SetLabel("Navigation Agent Types");
		desc.SetDescription("Navigation Agent Types");
	}
};

struct SAnnotationFlagsMask
{
	MNM::AreaAnnotation::value_type mask;

	SAnnotationFlagsMask() : mask(-1) {}
	bool        operator==(const SAnnotationFlagsMask& other) const { return other.mask == mask; }

	static void ReflectType(Schematyc::CTypeDesc<SAnnotationFlagsMask>& desc)
	{
		desc.SetGUID("449D0789-4DAE-4702-A2CB-068665C6149B"_cry_guid);
		desc.SetLabel("Navigation Area Flags");
		desc.SetDescription("Navigation Area Flags");
	}
};
bool Serialize(Serialization::IArchive& archive, SAnnotationFlagsMask& value, const char* szName, const char* szLabel);
}

//////////////////////////////////////////////////////////////////////////
namespace NavigationSystemSchematyc
{
void Register(Schematyc::IEnvRegistrar& registrar, Schematyc::CEnvRegistrationScope& parentScope);

} //NavigationSystemSchematyc
