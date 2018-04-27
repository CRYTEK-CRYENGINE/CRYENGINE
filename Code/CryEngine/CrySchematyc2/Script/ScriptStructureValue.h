// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//#include <CrySchematyc2/AggregateTypeId.h>
//#include <CrySchematyc2/IEnvRegistry.h>
//#include <CrySchematyc2/IEnvTypeDesc.h>
//#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Script/IScriptFile.h>

namespace Schematyc2
{
	// Script structure value.
	// #SchematycTODO : Does we really need to derive from IAny or should we create a CScriptAny utility class?
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CScriptStructureValue : public IAny
	{
	public:

		CScriptStructureValue(const IScriptStructure* pStructure);
		CScriptStructureValue(const CScriptStructureValue& rhs);

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

		void Serialize(Serialization::IArchive& archive);

	private:

		typedef std::map<string, IAnyPtr> FieldMap; // #SchematycTODO : Replace map with vector to preserve order!

		void Refresh();

		const IScriptStructure* m_pStructure; // #SchematycTODO : Wouldn't it be safer to reference by GUID?
		FieldMap                m_fields;
	};
}
