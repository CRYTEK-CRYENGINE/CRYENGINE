/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once
#include <vector>
#include <CrySerialization/yasli/Pointers.h>

namespace yasli{ class Archive; }

class PropertyRow;

struct TreePathLeaf
{
	int index;

	TreePathLeaf(int _index = -1)
		: index(_index)
	{
	}
	bool operator==(const TreePathLeaf& rhs) const{
		return index == rhs.index;
	}
	bool operator!=(const TreePathLeaf& rhs) const{
		return index != rhs.index;
	}
};
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, TreePathLeaf& value, const char* name, const char* label);

typedef std::vector<TreePathLeaf> TreePath;
typedef std::vector<TreePath> TreePathes;

class PropertyTreeOperator
{
public:
    enum Type{
        NONE,
        REPLACE,
        ADD,
        REMOVE
    };

    PropertyTreeOperator();
    ~PropertyTreeOperator();
    PropertyTreeOperator(const TreePath& path, PropertyRow* row);
    void YASLI_SERIALIZE_METHOD(yasli::Archive& ar);
private:
    Type type_;
    TreePath path_;
    yasli::SharedPtr<PropertyRow> row_;
    int index_;
    friend class PropertyTreeModel;
};


