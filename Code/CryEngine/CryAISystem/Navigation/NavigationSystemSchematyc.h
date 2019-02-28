// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "NavigationSystem/NavigationSystem.h"
#include <CryAISystem/Serialization/NavigationSerialize.h>
#include <CryAISystem/Components/IEntityNavigationComponent.h>

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
inline void ReflectType(Schematyc::CTypeDesc<IEntityNavigationComponent::SMovementProperties>& desc)
{
	desc.SetGUID("44888426-aafa-472e-81ac-58bb1be18475"_cry_guid);

	desc.SetLabel("Movement Properties");
	desc.SetDescription("Movement Properties");
	desc.SetDefaultValue(IEntityNavigationComponent::SMovementProperties());

	desc.AddMember(&IEntityNavigationComponent::SMovementProperties::normalSpeed, 'nrsp', "normalSpeed", "Normal Speed", "Normal speed of the agent", 4.0f);
	desc.AddMember(&IEntityNavigationComponent::SMovementProperties::minSpeed, 'mnsp', "minSpeed", "Min Speed", "Minimal speed of the agent", 0.0f);
	desc.AddMember(&IEntityNavigationComponent::SMovementProperties::maxSpeed, 'mxsp', "maxSpeed", "Max Speed", "Maximal speed of the agent", 6.0f);

	desc.AddMember(&IEntityNavigationComponent::SMovementProperties::maxAcceleration, 'mxac', "maxAcceleration", "Max Acceleration", "Maximal acceleration", 6.0f);
	desc.AddMember(&IEntityNavigationComponent::SMovementProperties::maxDeceleration, 'mxdc', "maxDeceleration", "Max Deceleration", "Maximal deceleration", 10.0f);

	desc.AddMember(&IEntityNavigationComponent::SMovementProperties::lookAheadDistance, 'lad', "lookAheadDistance", "Look Ahead Distance", "How far is point on path that the agent is following", 0.5f);
	desc.AddMember(&IEntityNavigationComponent::SMovementProperties::bStopAtEnd, 'sae', "stopAtEnd", "Stop At End", "Should the agent stop when reaching end of the path", true);
}

//////////////////////////////////////////////////////////////////////////
inline void ReflectType(Schematyc::CTypeDesc<IEntityNavigationComponent::SCollisionAvoidanceProperties::EType>& desc)
{
	desc.SetGUID("ff0a97c4-eb3f-44cb-b23a-1626fc073e4c"_cry_guid);

	desc.AddConstant(IEntityNavigationComponent::SCollisionAvoidanceProperties::EType::None, "None", "None");
	desc.AddConstant(IEntityNavigationComponent::SCollisionAvoidanceProperties::EType::Passive, "Passive", "Passive");
	desc.AddConstant(IEntityNavigationComponent::SCollisionAvoidanceProperties::EType::Active, "Active", "Active");
}

inline void ReflectType(Schematyc::CTypeDesc<IEntityNavigationComponent::SCollisionAvoidanceProperties>& desc)
{
	desc.SetGUID("4086c4b2-2300-41ea-aa95-4f119fa4281b"_cry_guid);

	desc.AddMember(&IEntityNavigationComponent::SCollisionAvoidanceProperties::type, 'type', "type", "Type", "How the agent is going to behave in collision avoidance system", IEntityNavigationComponent::SCollisionAvoidanceProperties::EType::Active);
	desc.AddMember(&IEntityNavigationComponent::SCollisionAvoidanceProperties::radius, 'rad', "radius", "Radius", "Radius of the agent used in collision avoidance calculations", 0.3f);
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
