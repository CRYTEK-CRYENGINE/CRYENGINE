// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Containers/CryArray.h>

#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Utils/Scratchpad.h>

namespace Schematyc
{

class CClassProperties
{
private:

	typedef DynArray<uint32> Properties;

public:

	inline CClassProperties() {}

	inline CClassProperties(const CClassProperties& rhs)
		: m_pDesc(rhs.m_pDesc)
		, m_properties(rhs.m_properties)
		, m_scratchpad(rhs.m_scratchpad)
		, m_overridePolicy(rhs.m_overridePolicy)
	{}

	inline bool IsEmpty() const
	{
		return m_properties.empty();
	}

	const CClassDesc* GetTypeDesc() const { return m_pDesc; };

	inline bool Set(const CClassDesc& desc)
	{
		Clear();

		m_pDesc = &desc;

		const CClassMemberDescArray& memberDescs = desc.GetMembers();
		m_properties.reserve(memberDescs.size());
		for (const CClassMemberDesc& memberDesc : memberDescs)
		{
			const void* pDefaultValue = memberDesc.GetDefaultValue();
			if (!pDefaultValue)
			{
				Clear();
				return false;
			}

			const uint32 pos = m_scratchpad.Add(CAnyConstRef(memberDesc.GetTypeDesc(), pDefaultValue));
			m_properties.emplace_back(pos);
		}

		return true;
	}

	inline void Clear()
	{
		m_properties.clear();
		m_scratchpad.Clear();
		m_pDesc = nullptr;
	}

	inline bool Apply(const CClassDesc& desc, void* pValue) const
	{
		if (!m_pDesc || (desc != *m_pDesc))
		{
			return false;
		}

		if (m_overridePolicy == EOverridePolicy::Default)
		{
			return false;
		}

		const CClassMemberDescArray& memberDescs = desc.GetMembers();
		for (uint32 propertyIdx = 0, propertyCount = m_properties.size(); propertyIdx < propertyCount; ++propertyIdx)
		{
			const CClassMemberDesc& memberDesc = memberDescs[propertyIdx];
			Any::CopyAssign(CAnyRef(memberDesc.GetTypeDesc(), static_cast<uint8*>(pValue) + memberDesc.GetOffset()), *m_scratchpad.Get(m_properties[propertyIdx]));
		}
		return true;
	}

	//! Read current values from class
	inline bool Read(const CClassDesc& desc, void* pValue)
	{
		if (m_pDesc != &desc)
		{ 
			Set(desc);
		}

		const CClassMemberDescArray& memberDescs = desc.GetMembers();
		for (uint32 propertyIdx = 0, propertyCount = m_properties.size(); propertyIdx < propertyCount; ++propertyIdx)
		{
			const CClassMemberDesc& memberDesc = memberDescs[propertyIdx];
			Any::CopyAssign(*m_scratchpad.Get(m_properties[propertyIdx]),CAnyRef(memberDesc.GetTypeDesc(), static_cast<uint8*>(pValue) + memberDesc.GetOffset()));
		}
		return true;
	}

	inline void Serialize(Serialization::IArchive& archive)
	{
		if (m_pDesc)
		{
			const CClassMemberDescArray& memberDescs = m_pDesc->GetMembers();
			for (uint32 propertyIdx = 0, propertyCount = m_properties.size(); propertyIdx < propertyCount; ++propertyIdx)
			{
				const CClassMemberDesc& memberDesc = memberDescs[propertyIdx];
				archive(*m_scratchpad.Get(m_properties[propertyIdx]), memberDesc.GetName(), memberDesc.GetLabel());
				const char* szDescription = memberDesc.GetDescription();
				if (szDescription && (szDescription[0] != '\0'))
				{
					archive.doc(szDescription);
				}
			}
		}
	}

	inline bool Compare( const CClassProperties &rhs ) const
	{
		if (!m_pDesc || (m_pDesc != rhs.m_pDesc))
			return false;

		if (m_properties.size() != rhs.m_properties.size())
			return false;

		const CClassMemberDescArray& memberDescs = m_pDesc->GetMembers();
		for (uint32 propertyIdx = 0, propertyCount = m_properties.size(); propertyIdx < propertyCount; ++propertyIdx)
		{
			const CClassMemberDesc& memberDesc = memberDescs[propertyIdx];
			if (!Any::Equals(*m_scratchpad.Get(m_properties[propertyIdx]), *rhs.m_scratchpad.Get(rhs.m_properties[propertyIdx])))
			{
				return false;
			}
		}
		return true;
	}

	inline bool Compare(const CClassDesc& desc, void* pValue) const
	{
		if (!m_pDesc || (m_pDesc != &desc))
			return false;

		const CClassMemberDescArray& memberDescs = m_pDesc->GetMembers();
		for (uint32 propertyIdx = 0, propertyCount = m_properties.size(); propertyIdx < propertyCount; ++propertyIdx)
		{
			const CClassMemberDesc& memberDesc = memberDescs[propertyIdx];
			if (!Any::Equals(
					*m_scratchpad.Get(m_properties[propertyIdx]), 
					CAnyRef(memberDesc.GetTypeDesc(), static_cast<uint8*>(pValue) + memberDesc.GetOffset()))
				)
			{
				return false;
			}
		}
		return true;
	}

	void SetOverridePolicy(EOverridePolicy policy)
	{
		m_overridePolicy = policy;
	}

	EOverridePolicy GetOverridePolicy() const
	{
		return m_overridePolicy;
	}

private:
	const CClassDesc* m_pDesc = nullptr;
	HeapScratchpad    m_scratchpad;
	Properties        m_properties;
	EOverridePolicy   m_overridePolicy = EOverridePolicy::Override;
};

} // Schematyc
