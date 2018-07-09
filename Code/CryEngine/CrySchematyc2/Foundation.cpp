// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Foundation.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CFoundation::CFoundation(const SGUID& guid, const char* name)
		: m_guid(guid)
		, m_name(name)
	{}

	//////////////////////////////////////////////////////////////////////////
	SGUID CFoundation::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFoundation::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CFoundation::SetDescription(const char* szDescription)
	{
		m_description = szDescription;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFoundation::GetDescription() const
	{
		return m_description.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CFoundation::SetProperties(const IPropertiesPtr& pProperties)
	{
		m_pProperties = pProperties;
	}

	//////////////////////////////////////////////////////////////////////////
	IPropertiesConstPtr CFoundation::GetProperties() const
	{
		return m_pProperties;
	}

	//////////////////////////////////////////////////////////////////////////
	void CFoundation::UseNamespace(const char* szNamespace)
	{
		CRY_ASSERT(szNamespace && szNamespace[0]);
		if(szNamespace && szNamespace[0])
		{
			m_namespaces.push_back(szNamespace);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CFoundation::GetNamespaceCount() const
	{
		return m_namespaces.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFoundation::GetNamespace(size_t namespaceIdx) const
	{
		return namespaceIdx < m_namespaces.size() ? m_namespaces[namespaceIdx] : "";
	}

	//////////////////////////////////////////////////////////////////////////
	void CFoundation::AddAbstractInterface(const SGUID& guid)
	{
		m_abstractInterfaces.push_back(guid);
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CFoundation::GetAbstractInterfaceCount() const
	{
		return m_abstractInterfaces.size();
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CFoundation::GetAbstractInterfaceGUID(size_t iAbstractInterface) const
	{
		return iAbstractInterface < m_abstractInterfaces.size() ? m_abstractInterfaces[iAbstractInterface] : SGUID();
	}

	//////////////////////////////////////////////////////////////////////////
	void CFoundation::AddComponent(const SGUID& guid)
	{
		m_components.push_back(guid);
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CFoundation::GetComponentCount() const
	{
		return m_components.size();
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CFoundation::GetComponentGUID(size_t iComponent) const
	{
		return iComponent < m_components.size() ? m_components[iComponent] : SGUID();
	}

	//////////////////////////////////////////////////////////////////////////
	bool CFoundation::AddExtension_Protected(const EnvTypeId& typeId, const IFoundationExtensionPtr& pExtension)
	{
		if(QueryExtension_Protected(typeId) == NULL)
		{
			m_extensions.insert(TExtensionMap::value_type(typeId, pExtension));
			return true;
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	IFoundationExtensionPtr CFoundation::QueryExtension_Protected(const EnvTypeId& typeId) const
	{
		TExtensionMap::const_iterator	iExtension = m_extensions.find(typeId);
		return iExtension != m_extensions.end() ? iExtension->second : IFoundationExtensionPtr();
	}
}
