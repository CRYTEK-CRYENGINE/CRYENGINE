// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "PropertyTreeModel.h"
#include <CrySerialization/yasli/Archive.h>

namespace PropertyTree2
{
	class PropertyTreeOArchive : public yasli::Archive
	{
	public:
		PropertyTreeOArchive(CRowModel& root);
		virtual ~PropertyTreeOArchive();

	protected:
		bool operator()(yasli::StringInterface& value, const char* name, const char* label) override;
		bool operator()(yasli::WStringInterface& value, const char* name, const char* label) override;
		bool operator()(bool& value, const char* name, const char* label) override;
		bool operator()(char& value, const char* name, const char* label) override;

		bool operator()(yasli::i8& value, const char* name, const char* label) override;
		bool operator()(yasli::i16& value, const char* name, const char* label) override;
		bool operator()(yasli::i32& value, const char* name, const char* label) override;
		bool operator()(yasli::i64& value, const char* name, const char* label) override;

		bool operator()(yasli::u8& value, const char* name, const char* label) override;
		bool operator()(yasli::u16& value, const char* name, const char* label) override;
		bool operator()(yasli::u32& value, const char* name, const char* label) override;
		bool operator()(yasli::u64& value, const char* name, const char* label) override;

		bool operator()(float& value, const char* name, const char* label) override;
		bool operator()(double& value, const char* name, const char* label) override;

		bool operator()(const yasli::Serializer& ser, const char* name, const char* label) override;
		bool operator()(yasli::PointerInterface& ptr, const char *name, const char *label) override;
		bool operator()(yasli::ContainerInterface& container, const char *name, const char *label) override;
		bool operator()(yasli::CallbackInterface& callback, const char* name, const char* label) override;
		bool operator()(yasli::Object& obj, const char *name, const char *label) override;
		using yasli::Archive::operator();

		bool openBlock(const char* name, const char* label, const char* icon = 0) override;
		void closeBlock() override;

	private:
		//Requires the VALIDATION flag
		//void validatorMessage(bool error, const void* handle, const yasli::TypeID& type, const char* message) override;
		void documentLastField(const char* docString) override;

	private:

		void EnterScope(CRowModel* scope);
		void ExitScope();

		template<typename DataType>
		CRowModel* FindOrCreateRowInScope(const char* name, const char* label)
		{
			return FindOrCreateRowInScope(name, label, yasli::TypeID::get<DataType>());
		}

		//If type is invalid, only name will be matched
		CRowModel* FindOrCreateRowInScope(const char* name, const char* label, const yasli::TypeID& type);
		CRowModel* FindRowInScope(const char* name);
		CRowModel* GetLastVisitedRow();

		template<typename ValueType>
		void ProcessSimpleRow(const ValueType& value, const char* name, const char* label);

		struct Scope
		{
			Scope(CRowModel* row) 
				: m_scopeRow(row)
				, m_lastVisitedIndex(-1) 
			{}

			CRowModel* m_scopeRow;
			int m_lastVisitedIndex;
		};

		std::vector<Scope> m_scopeStack;
		Scope m_currentScope;
		bool m_firstPopulate;
	};
}
