// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>
#include <CryString/CryString.h>

class CNodeProperties
{
public:

	enum class ENodeType
	{
		eDefault,
		eProxy,
		eMesh,
		eDummy,
	};

	enum EPrimitiveType
	{
		ePrimitiveType_Default,
		ePrimitiveType_Box, // Force this proxy to be a Box primitive in the engine.
		ePrimitiveType_Cylinder,
		ePrimitiveType_Capsule,
		ePrimitiveType_Sphere,
		ePrimitiveType_NotAPrimitive, // Force the engine NOT to convert this proxy to a primitive.
	};

private:
	enum class EValueType
	{
		eString,
		eFloat,
		eBool,
		ePrimitiveType
	};

	struct SProperty
	{
		EValueType     valueType;

		float          floatValue;
		bool           boolValue;
		EPrimitiveType primitiveValue;
		string         stringValue;

		const char*    name;
		const char*    label;
		const char*    tooltip;

		// Warning: name, label and tooltip must be string literals.
		SProperty(EValueType type, const char* name, const char* label, const char* tooltip = nullptr)
			: valueType(type)
			, floatValue(0)
			, boolValue(false)
			, primitiveValue(EPrimitiveType::ePrimitiveType_Default)
			, name(name)
			, label(label)
			, tooltip(tooltip)
		{
			switch (valueType)
			{
			case EValueType::eFloat:
				floatValue = 0;
				break;
			case EValueType::eBool:
				boolValue = false;
				break;
			case EValueType::ePrimitiveType:
				primitiveValue = EPrimitiveType::ePrimitiveType_Default;
				break;
			case EValueType::eString:
				break;
			default:
				assert(0);
				break;
			}
		}

		bool operator==(const SProperty& other) const
		{
			if (valueType != other.valueType || name != other.name)
			{
				return false;
			}

			switch (valueType)
			{
			case EValueType::eBool:
				return boolValue == other.boolValue;
			case EValueType::eFloat:
				return floatValue == other.floatValue;
			case EValueType::ePrimitiveType:
				return primitiveValue == other.primitiveValue;
			case EValueType::eString:
				return stringValue == other.stringValue;
			default:
				assert(0);
				return false;
			}
		}

		bool   IsDefaultValue() const;

		string GetAsString() const;
	};

	struct SGroup
	{
		std::vector<SProperty> properties;
		string                 name;
		bool                   bIsActive;

		// An unary predicate which returns true if the group is applicable for the nodeType.
		std::function<bool(ENodeType nodeType)> compatibleWith;

		SGroup() : bIsActive(false) {}

		bool operator==(const SGroup& other) const
		{
			return properties == other.properties;
		}

		void   Serialize(Serialization::IArchive& ar);

		string GetAsString() const;
	};

public:
	CNodeProperties();

	void Serialize(Serialization::IArchive& ar)
	{
		for (auto& group : m_groups)
		{
			// UI logic
			if (ar.isEdit())
			{
				// Filter groups by the node type
				if (group.compatibleWith && !group.compatibleWith(m_nodeType))
				{
					continue;
				}

				// Show the group check box.
				if (!group.name.empty())
				{
					ar(group.bIsActive, group.name, group.name);

					if (!group.bIsActive)
					{
						continue;
					}
				}
			}

			// TODO: If true will show the group as a subtree.
			// Can be runtime flag.
			// Can be global or group specific.
			const bool bShowAsSubtree = false;

			if (bShowAsSubtree && ar.isEdit() && !group.name.empty())
			{
				ar(group, group.name, group.name);
			}
			else // Plain list
			{
				group.Serialize(ar);
			}
		}
	}

	bool operator==(const CNodeProperties& other) const
	{
		return m_groups == other.m_groups;
	}

	bool operator!=(const CNodeProperties& other) const
	{
		return !(*this == other);
	}

	void SetNodeType(ENodeType nodeType)
	{
		m_nodeType = nodeType;
	}

	string GetAsString() const;

private:
	std::vector<SGroup> m_groups;
	ENodeType           m_nodeType;
};

inline bool CNodeProperties::SProperty::IsDefaultValue() const
{
	switch (valueType)
	{
	case EValueType::eString:
		return stringValue.empty();
	case EValueType::eFloat:
		return floatValue == 0;
	case EValueType::eBool:
		return !boolValue;
	case EValueType::ePrimitiveType:
		return primitiveValue == EPrimitiveType::ePrimitiveType_Default;
	default:
		return false;
	}
}

