/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include <CrySerialization/yasli/Archive.h>

namespace yasli{
	class EnumDescription;
	class Object;
}

class PropertyRow;
class PropertyTreeModel;

class PropertyIArchive : public yasli::Archive{
public:
	PropertyIArchive(PropertyTreeModel* model, PropertyRow* root);

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
	bool operator()(yasli::PointerInterface& ser, const char* name, const char* label) override;
	bool operator()(yasli::ContainerInterface& ser, const char* name, const char* label) override;
	bool operator()(yasli::Object& obj, const char* name, const char* label) override;
	bool operator()(yasli::CallbackInterface& callback, const char* name, const char* label) override;
	using yasli::Archive::operator();

	bool openBlock(const char* name, const char* label, const char* icon=0) override;
	void closeBlock() override;

protected:
	bool needDefaultArchive(const char* baseName) const { return false; }
private:
	bool openRow(const char* name, const char* label, const char* typeName);
	void closeRow(const char* name);

	struct Level {
		int rowIndex;
		Level() : rowIndex(0) {}
	};

	vector<Level> stack_;

	PropertyTreeModel* model_;
	PropertyRow* currentNode_;
	PropertyRow* lastNode_;
	PropertyRow* root_;
};


