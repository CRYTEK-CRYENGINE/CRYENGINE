// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectProperties.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySerialization/Decorators/ActionButton.h>
#include <CrySerialization/Color.h>
#include <CrySchematyc/Utils/Any.h>

namespace Schematyc
{

CObjectProperties::SComponent::SComponent() {}

CObjectProperties::SComponent::SComponent(const char* _szName, const CClassProperties& _properties, EOverridePolicy _overridePolicy)
	: name(_szName)
	, properties(_properties)
{
	properties.SetOverridePolicy(_overridePolicy);
}

CObjectProperties::SComponent::SComponent(const SComponent& rhs)
	: name(rhs.name)
	, properties(rhs.properties)
{}

void CObjectProperties::SComponent::Serialize(Serialization::IArchive& archive)
{
	if (archive.isOutput())
	{
		// Only save per instance object properties when override policy is not default
		if (properties.GetOverridePolicy() != EOverridePolicy::Default)
		{
			archive(properties, "properties");
		}
	}
	else
	{
		archive(properties, "properties");
	}
	if (!archive.isEdit())
	{
		EOverridePolicy ovr = properties.GetOverridePolicy();
		archive(ovr, "overridePolicy");
		properties.SetOverridePolicy(ovr);
	}
}

CObjectProperties::SVariable::SVariable() {}

CObjectProperties::SVariable::SVariable(const char* _szName, const CAnyValuePtr& _pValue, EOverridePolicy _overridePolicy)
	: name(_szName)
	, pValue(_pValue)
	, overridePolicy(_overridePolicy)
{}

CObjectProperties::SVariable::SVariable(const SVariable& rhs)
	: name(rhs.name)
	, pValue(CAnyValue::CloneShared(*rhs.pValue))
	, overridePolicy(rhs.overridePolicy)
{}

void CObjectProperties::SVariable::Serialize(Serialization::IArchive& archive)
{
	if (archive.isEdit())
	{
		if (overridePolicy == EOverridePolicy::Default)
		{
			archive(Serialization::ActionButton(functor(*this, &CObjectProperties::SVariable::Edit)), "edit", "^Edit");
			archive(*pValue, "value", "!^Value");
		}
		else
		{
			archive(Serialization::ActionButton(functor(*this, &CObjectProperties::SVariable::Revert)), "revert", "^Revert");
			archive(*pValue, "value", "^Value");
		}
	}
	else
	{
		archive(*pValue, "value");
		archive(overridePolicy, "overridePolicy");
	}
}

void CObjectProperties::SVariable::Edit()
{
	overridePolicy = EOverridePolicy::Override;
}

void CObjectProperties::SVariable::Revert()
{
	overridePolicy = EOverridePolicy::Default;
}

CObjectProperties::CObjectProperties() {}

CObjectProperties::CObjectProperties(const CObjectProperties& rhs)
	: m_components(rhs.m_components)
	, m_variables(rhs.m_variables)
{}

IObjectPropertiesPtr CObjectProperties::Clone() const
{
	return std::make_shared<CObjectProperties>(*this);
}

void CObjectProperties::Serialize(Serialization::IArchive& archive)
{
	if (archive.isEdit())
	{
		if (!m_components.empty())
		{
			archive.openBlock("components", "Components");

			ComponentsByName componentsByName;
			for (Components::value_type& component : m_components)
			{
				componentsByName.insert(ComponentsByName::value_type(component.second.name.c_str(), component.second));
			}
			for (ComponentsByName::value_type& component : componentsByName)
			{
				archive(component.second, component.first, component.first);
			}

			archive.closeBlock();
		}
	}
	else
	{
		archive(m_components, "components");
	}
	SerializeVariables(archive);
}

void  CObjectProperties::SerializeVariables(Serialization::IArchive& archive)
{
	if (archive.isEdit())
	{
		if (!m_variables.empty())
		{
			VariablesByName variablesByName;
			for (Variables::value_type& variable : m_variables)
			{
				variablesByName.insert(VariablesByName::value_type(variable.second.name.c_str(), variable.second));
			}
			for (VariablesByName::value_type& variable : variablesByName)
			{
				archive(variable.second, variable.first, variable.first);
			}
		}
	}
	else
	{
		archive(m_variables, "variables");
	}
}

const CClassProperties* CObjectProperties::GetComponentProperties(const CryGUID& guid) const
{
	Components::const_iterator itComponent = m_components.find(guid);
	return itComponent != m_components.end() ? &itComponent->second.properties : nullptr;
}

CClassProperties* CObjectProperties::GetComponentProperties(const CryGUID& guid)
{
	auto itComponent = m_components.find(guid);
	return itComponent != m_components.end() ? &itComponent->second.properties : nullptr;
}

bool CObjectProperties::ReadVariable(const CAnyRef& value, const CryGUID& guid) const
{
	Variables::const_iterator itVariable = m_variables.find(guid);
	if ((itVariable != m_variables.end()))
	{
		if (itVariable->second.overridePolicy == EOverridePolicy::Override)
		{
			return Any::CopyAssign(value, *itVariable->second.pValue);
		}
		return true;
	}
	return false;
}

void CObjectProperties::AddComponent(const CryGUID& guid, const char* szName, const CClassProperties& properties)
{
	m_components.insert(Components::value_type(guid, SComponent(szName, properties, EOverridePolicy::Default)));
}

void CObjectProperties::AddVariable(const CryGUID& guid, const char* szName, const CAnyConstRef& value)
{
	m_variables.insert(Variables::value_type(guid, SVariable(szName, CAnyValue::CloneShared(value), EOverridePolicy::Default)));
}

} // Schematyc
