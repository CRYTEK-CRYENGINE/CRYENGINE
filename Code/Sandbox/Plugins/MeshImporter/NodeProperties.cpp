// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "NodeProperties.h"
#include <CrySerialization/Enum.h>

SERIALIZATION_ENUM_BEGIN_NESTED(CNodeProperties, EPrimitiveType, "Primitive Type")
SERIALIZATION_ENUM(CNodeProperties::ePrimitiveType_Default, "default", "Default")
SERIALIZATION_ENUM(CNodeProperties::ePrimitiveType_Box, "box", "Box")
SERIALIZATION_ENUM(CNodeProperties::ePrimitiveType_Cylinder, "cylinder", "Cylinder")
SERIALIZATION_ENUM(CNodeProperties::ePrimitiveType_Capsule, "capsule", "Capsule")
SERIALIZATION_ENUM(CNodeProperties::ePrimitiveType_Sphere, "sphere", "Sphere")
SERIALIZATION_ENUM(CNodeProperties::ePrimitiveType_NotAPrimitive, "notaprim", "Not a Primitive")
SERIALIZATION_ENUM_END()

namespace
{
enum EGroup
{
	eGroup_Default,
	eGroup_Proxy,
	eGroup_Joint,
	eGroup_Deformable,
	eGroup_COUNT
};

// TODO : Move to a shared location
template<typename T>
const char* EnumToString(T value)
{
	auto& desc = yasli::getEnumDescription<T>();
	return desc.label(static_cast<int>(value));
}

template<typename InputIt>
string GetCollectionAsString(InputIt first, InputIt last, const char* delimeter = "\n")
{
	string result;
	for (; first != last; ++first)
	{
		result = result.empty() ? (*first).GetAsString() : result + delimeter + (*first).GetAsString();
	}

	return result;
}
}

CNodeProperties::CNodeProperties()
	: m_nodeType(ENodeType::eDefault)
{
	m_groups.resize(eGroup_COUNT);

	m_groups[eGroup_Default].compatibleWith = [](ENodeType nodeType)
	{
		return nodeType == ENodeType::eMesh || nodeType == ENodeType::eProxy;
	};
	m_groups[eGroup_Default].bIsActive = true;

	m_groups[eGroup_Default].properties.emplace_back(
	  EValueType::eFloat,
	  "mass",
	  "Mass",
	  "Mass defines the weight of an object based on real world physics in kg.\n"
	  "Mass=0 sets the object to \"unmovable\".");

	m_groups[eGroup_Default].properties.emplace_back(
	  EValueType::eFloat,
	  "density",
	  "Density",
	  "The engine automatically calculates the mass for an object.\n"
	  "based on the density and the bounding box of an object.\n"
	  "Can be used alternatively to mass.");

	m_groups[eGroup_Default].properties.emplace_back(
	  EValueType::eBool,
	  "no_hit_refinement",
	  "No Hit Refinement",
	  "Do not perform hit-refinement at the rendermesh level.");

	m_groups[eGroup_Default].properties.emplace_back(
	  EValueType::eBool,
	  "dynamic",
	  "Dynamically Breakable", "Flags the object as \"dynamically breakable\".\n"
	                           "However this string is not required on Glass, Trees, or Cloth,\n"
	                           "as these are already flagged automatically by the engine\n"
	                           "(through surface-type system).");

	m_groups[eGroup_Default].properties.emplace_back(
	  EValueType::eBool,
	  "entity",
	  "Spawn As Entity",
	  "If the render geometry properties include \"entity\",\n"
	  "the object will not fade out after being disconnected from the main object.");

	m_groups[eGroup_Default].properties.emplace_back(
	  EValueType::eString,
	  "pieces",
	  "Pieces",
	  "Instead of disconnecting the piece when the joint is broken,\n"
	  "it will instantly disappear spawning a particle effect\n"
	  "depending on the surfacetype of the proxy.");

	// Proxy

	m_groups[eGroup_Proxy].compatibleWith = [](ENodeType nodeType)
	{
		return nodeType == ENodeType::eProxy;
	};
	m_groups[eGroup_Proxy].bIsActive = true;

	m_groups[eGroup_Proxy].properties.emplace_back(
	  EValueType::ePrimitiveType,
	  "primitive",
	  "Primitive",
	  "Force this proxy to be a primitive in the engine.");

	m_groups[eGroup_Proxy].properties.emplace_back(
	  EValueType::eBool,
	  "no_explosion_occlusion",
	  "No Explosion Occlusion",
	  "Will allow the force / damage of an explosion\n"
	  "to penetrate through the phys proxy.");

	// Deformable mesh

	m_groups[eGroup_Deformable].compatibleWith = [](ENodeType nodeType)
	{
		return nodeType == ENodeType::eMesh;
	};
	m_groups[eGroup_Deformable].name = "Deformable";
	m_groups[eGroup_Deformable].bIsActive = false; // Deformable properties are hidden by default.

	m_groups[eGroup_Deformable].properties.emplace_back(
	  EValueType::eFloat,
	  "stiffness",
	  "Stiffness",
	  "Resilience to bending and shearing.");

	m_groups[eGroup_Deformable].properties.emplace_back(
	  EValueType::eFloat,
	  "hardness",
	  "Hardness",
	  "Resilience to stretching.");

	m_groups[eGroup_Deformable].properties.emplace_back(
	  EValueType::eFloat,
	  "max_stretch",
	  "Max Stretch",
	  "If any edge is stretched more than that, it's length is re-enforced.\n"
	  "Max Stretch = 0.3 means stretched to 130% of its original length\n"
	  "(or by 30% wrt to its original length). Default 0.01.");

	m_groups[eGroup_Deformable].properties.emplace_back(
	  EValueType::eFloat,
	  "max_impulse",
	  "Max Impulse",
	  "Upper limit on all applied impulses.\n"
	  "Default skeleton's mass*100");

	m_groups[eGroup_Deformable].properties.emplace_back(
	  EValueType::eFloat,
	  "skin_dist",
	  "Skin Distance",
	  "Sphere radius in skinning assignment.\n"
	  "Default is the minimum of the main mesh's bounding box's dimensions.");

	m_groups[eGroup_Deformable].properties.emplace_back(
	  EValueType::eFloat,
	  "thickness",
	  "Thickness",
	  "Sets the collision thickness for the skeleton.");

	m_groups[eGroup_Deformable].properties.emplace_back(
	  EValueType::eFloat,
	  "explosion_scale",
	  "Explosion Scale",
	  "Used to scale down the effect of explosions on the deformable.");

	// Dummy - may be a joint node.

	m_groups[eGroup_Joint].compatibleWith = [](ENodeType nodeType)
	{
		return nodeType == ENodeType::eDummy;
	};
	m_groups[eGroup_Joint].name = "Jointed Breakable";
	m_groups[eGroup_Joint].bIsActive = false;

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eBool,
	  "gameplay_critical",
	  "Gameplay Critical",
	  "Joints in the entire entity will break,\n"
	  "even if jointed breaking is disabled overall.");

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eBool,
	  "player_can_break",
	  "Player Can Break",
	  "Joints in the entire breakable entity can be broken\n"
	  "by the player bumping into them.");

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eFloat,
	  "limit",
	  "Limit",
	  "Limit is a general value for several different kind of forces\n"
	  "applied to the joint.");

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eFloat,
	  "bend",
	  "Bend",
	  "Maximum torque around an axis perpendicular to the normal.");

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eFloat,
	  "twist",
	  "Twist",
	  "Maximum torque around the normal.");

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eFloat,
	  "pull",
	  "Pull",
	  "Maximum force applied to the joint's 1st object against the joint normal.");

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eFloat,
	  "push",
	  "Push",
	  "Maximum force applied to the joint's 1st object.");

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eFloat,
	  "shift",
	  "Shift",
	  "Maximum force in the direction perpendicular to normal.");

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eFloat,
	  "constraint_limit",
	  "Constraint Limit",
	  "Force limit for the newly created constraint,\n"
	  "is checked when the constraint hits a rotation limit\n"
	  "and is being pushed further.");

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eFloat,
	  "constraint_minang",
	  "Constraint Min Ang",
	  "Rotation limits for the constraint.\n"
	  "The default is no limits.");

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eFloat,
	  "consrtaint_maxang",
	  "Consrtaint Max Ang",
	  "Rotation limits for the constraint.\n"
	  "The default is no limits.");

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eFloat,
	  "constraint_damping",
	  "Constraint Damping",
	  "Angular damping. The default is 0.");

	m_groups[eGroup_Joint].properties.emplace_back(
	  EValueType::eBool,
	  "constraint_collides",
	  "Constraint Collides",
	  "Whether the constraint parts will ignore\n"
	  "collisions with each other.");
}

string CNodeProperties::GetAsString() const
{
	return GetCollectionAsString(m_groups.begin(), m_groups.end());
}

// CNodeProperties::SProperty

string CNodeProperties::SProperty::GetAsString() const
{
	switch (valueType)
	{
	case EValueType::eString:
		return string().Format("%s=%s", name, stringValue.c_str());
		break;
	case EValueType::eFloat:
		return string().Format("%s=%g", name, floatValue);
		break;
	case EValueType::eBool:
		return boolValue ? name : "";
		break;
	case EValueType::ePrimitiveType:
		return string().Format("%s=%s", name, EnumToString(primitiveValue));
		break;
	default:
		assert(0);
		break;
	}
	return "";
}

// CNodeProperties::SGroup

void CNodeProperties::SGroup::Serialize(Serialization::IArchive& ar)
{
	// Do not write to persistent storage if disabled.
	if (!ar.isEdit() && ar.isOutput() && !bIsActive)
	{
		return;
	}

	for (auto& property : properties)
	{
		// Do not write default values to persistent storage.
		if (!ar.isEdit() && ar.isOutput() && property.IsDefaultValue())
		{
			continue;
		}

		switch (property.valueType)
		{
		case EValueType::eString:
			ar(property.stringValue, property.name, property.label);
			break;
		case EValueType::eFloat:
			ar(property.floatValue, property.name, property.label);
			break;
		case EValueType::eBool:
			ar(property.boolValue, property.name, property.label);
			break;
		case EValueType::ePrimitiveType:
			ar(property.primitiveValue, property.name, property.label);
			break;
		}

		if (ar.isEdit())
		{
			ar.doc(property.tooltip);
		}
	}

	// Restore the bIsActive state after deserialisation from a persistent storage.
	if (!bIsActive && !ar.isEdit() && ar.isInput())
	{
		bIsActive = std::find_if(properties.begin(), properties.end(), [](const SProperty& property) { return !property.IsDefaultValue(); }) != properties.end();
	}
}

string CNodeProperties::SGroup::GetAsString() const
{
	return GetCollectionAsString(properties.begin(), properties.end());
}

