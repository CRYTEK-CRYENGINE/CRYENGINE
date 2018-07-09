// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//#include <CrySchematyc2/AggregateTypeId.h>
//#include <CrySchematyc2/IEnvRegistry.h>
//#include <CrySchematyc2/IEnvTypeDesc.h>
//#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Script/IScriptFile.h>

namespace Schematyc2
{
	// Script enumeration value.
	// #SchematycTODO : Does we really need to derive from IAny or should we create a CScriptAny utility class?
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CScriptEnumerationValue : public IAny
	{
	public:

		CScriptEnumerationValue(const IScriptEnumeration* pEnumeration);
		CScriptEnumerationValue(const CScriptEnumerationValue& rhs);

		// IAny
		virtual CTypeInfo GetTypeInfo() const override;
		virtual uint32 GetSize() const override;
		virtual bool Copy(const IAny& rhs) override;
		virtual IAny* Clone(void* pPlacement) const override;
		virtual IAnyPtr Clone() const override;
		virtual bool ToString(const CharArrayView& str) const override;
		virtual GameSerialization::IContextPtr BindSerializationContext(Serialization::IArchive& archive) const override;
		virtual bool Serialize(Serialization::IArchive& archive, const char* szName, const char* szLabel) override;
		virtual IAnyExtension* QueryExtension(EAnyExtension extension) override;
		virtual const IAnyExtension* QueryExtension(EAnyExtension extension) const override;
		virtual void* ToVoidPtr() override;
		virtual const void* ToVoidPtr() const override;
		// ~IAny

	private:

		const IScriptEnumeration* m_pEnumeration; // #SchematycTODO : Wouldn't it be safer to reference by GUID?
		size_t                    m_value; // #SchematycTODO : Would it be safer to store a string?
	};
}
