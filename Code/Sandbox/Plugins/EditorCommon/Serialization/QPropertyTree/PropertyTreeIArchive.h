// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "PropertyTreeModel.h"
#include <CrySerialization/yasli/Archive.h>

namespace PropertyTree2
{
class PropertyTreeIArchive : public yasli::Archive
{
public:
	PropertyTreeIArchive(const CRowModel& root);
	virtual ~PropertyTreeIArchive();

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
	bool operator()(yasli::PointerInterface& pointerInterface, const char* name, const char* label) override;
	bool operator()(yasli::ContainerInterface& container, const char* name, const char* label) override;
	bool operator()(yasli::CallbackInterface& callback, const char* name, const char* label) override;
	bool operator()(yasli::Object& obj, const char* name, const char* label) override;
	using yasli::Archive::operator();
	//!This checks if a row model with "name" exists, if it does it is used as the scope for the following serialization ops
	//!Note that you are not supposed to create a new block with an input archive (you are basically serializing invalid data), new blocks are created when serializing Object -> UI with an OArchive
	bool openBlock(const char* name, const char* label, const char* icon = 0) override;
	void closeBlock() override;

private:

	template<typename DataType>
	const CRowModel* FindRowInScope(const char* name)
	{
		return FindRowInScope(name, yasli::TypeID::get<DataType>());
	}

	//If type is invalid, only name will be matched
	const CRowModel* FindRowInScope(const char* name, const yasli::TypeID& type);

	template<typename ValueType>
	bool ProcessSimpleRow(const char* name, ValueType& value);

	const CRowModel* m_pCurrentScope;
	int              m_lastVisitedIndex; //assuming rows will be accessed sequentially, this helps with optimization
};
}
