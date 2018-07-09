/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include <CrySerialization/yasli/Config.h>
#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/Pointers.h>

class PropertyRow;
class PropertyTreeModel;
class ValidatorBlock;
using yasli::SharedPtr;

class PropertyOArchive : public yasli::Archive{
public:
	PropertyOArchive(PropertyTreeModel* model, PropertyRow* rootNode, ValidatorBlock* validator);
	~PropertyOArchive();

	void setOutlineMode(bool outlineMode);
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
	bool operator()(yasli::ContainerInterface& ser, const char *name, const char *label) override;
	bool operator()(yasli::CallbackInterface& callback, const char* name, const char* label) override;
	bool operator()(yasli::Object& obj, const char *name, const char *label) override;
	using yasli::Archive::operator();

	bool openBlock(const char* name, const char* label, const char* icon=0) override;
	void closeBlock() override;
	void validatorMessage(bool error, const void* handle, const yasli::TypeID& type, const char* message) override;
	void documentLastField(const char* docString) override;

protected:
	PropertyOArchive(PropertyTreeModel* model, bool forDefaultType);

private:
	struct Level {
		std::vector<SharedPtr<PropertyRow> > oldRows;
		int rowIndex;
		Level() : rowIndex(0) {}
	};
	std::vector<Level> stack_;

	template<class RowType, class ValueType>
	PropertyRow* updateRowPrimitive(const char* name, const char* label, const char* typeName, const ValueType& value, const void* handle, const yasli::TypeID& typeId);

	template<class RowType, class ValueType>
	RowType* updateRow(const char* name, const char* label, const char* typeName, const ValueType& value);

	void enterNode(PropertyRow* row); // sets currentNode
	void closeStruct(const char* name);
	PropertyRow* defaultValueRootNode();

	bool updateMode_;
	bool defaultValueCreationMode_;
	PropertyTreeModel* model_;
	ValidatorBlock* validator_;
	SharedPtr<PropertyRow> currentNode_;
	SharedPtr<PropertyRow> lastNode_;

	// for defaultArchive
	SharedPtr<PropertyRow> rootNode_;
	yasli::string typeName_;
	const char* derivedTypeName_;
	yasli::string derivedTypeNameAlt_;
	bool outlineMode_;
};

// vim:ts=4 sw=4:

