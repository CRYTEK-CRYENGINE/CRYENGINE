// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Serialization/PropertyTree/IDrawContext.h>
#include <Serialization/PropertyTree/PropertyRowField.h>
#include "Serialization/QPropertyTree/QPropertyTree.h"
#include "Serialization/PropertyTree/PropertyTreeModel.h"
#include "Serialization.h"
#include <CrySerialization/Decorators/ResourceFolderPath.h>
#include <CrySerialization/Decorators/ResourceFolderPathImpl.h>

using Serialization::ResourceFolderPath;

class PropertyRowResourceFolderPath : public PropertyRowField
{
public:
	PropertyRowResourceFolderPath() : handle_() {}
	bool                  isLeaf() const override   { return true; }
	bool                  isStatic() const override { return false; }
	bool                  onActivate(const PropertyActivationEvent& e) override;
	void                  setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override;
	bool                  assignTo(const Serialization::SStruct& ser) const override;
	string                valueAsString() const;
	bool                  onContextMenu(IMenu& menu, PropertyTree* tree);

	int                   buttonCount() const override     { return 1; }
	property_tree::Icon   buttonIcon(const PropertyTree* tree, int index) const override;
	bool                  usePathEllipsis() const override { return true; }
	void                  serializeValue(Serialization::IArchive& ar) override;
	const void*           searchHandle() const override    { return handle_; }
	Serialization::TypeID typeId() const override          { return Serialization::TypeID::get<string>(); }
	void                  clear();
private:
	string      path_;
	string      startFolder_;
	const void* handle_;
};

struct ResourceFolderPathMenuHandler : PropertyRowMenuHandler
{
	PropertyTree*                  tree;
	PropertyRowResourceFolderPath* self;

	ResourceFolderPathMenuHandler(PropertyTree* tree, PropertyRowResourceFolderPath* container);

	void onMenuClear();
};

