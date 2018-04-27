// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Serialization/PropertyTree/IDrawContext.h>
#include <Serialization/PropertyTree/PropertyRowField.h>
#include <Serialization/PropertyTree/PropertyRowString.h>
#include <Serialization/PropertyTree/PropertyTree.h>
#include <Serialization/PropertyTree/PropertyTreeModel.h>
#include "Serialization.h"
#include <CrySerialization/Decorators/ResourceFilePath.h>
#include <CrySerialization/Decorators/ResourceFilePathImpl.h>
#include <Serialization/Decorators/IconXPM.h>

using Serialization::ResourceFilePath;

class PropertyRowResourceFilePath : public PropertyRowString
{
public:
	void                  clear();

	bool                  isLeaf() const override   { return true; }
	bool                  isStatic() const override { return false; }

	void                  setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override;
	bool                  assignTo(const Serialization::SStruct& ser) const override;
	bool                  onActivateButton(int buttonIndex, const PropertyActivationEvent& e) override;

	int                   buttonCount() const override     { return 1; }
	property_tree::Icon   buttonIcon(const PropertyTree* tree, int index) const override;
	virtual bool          usePathEllipsis() const override { return true; }
	void                  serializeValue(Serialization::IArchive& ar);
	const void*           searchHandle() const override { return handle_; }
	Serialization::TypeID typeId() const override       { return Serialization::TypeID::get<string>(); }

	bool                  onContextMenu(property_tree::IMenu& menu, PropertyTree* tree) override;
private:
	string      filter_;
	string      startFolder_;
	int         flags_;
	const void* handle_;
};

struct ResourceFilePathMenuHandler : PropertyRowMenuHandler
{
	PropertyTree*                tree;
	PropertyRowResourceFilePath* self;

	ResourceFilePathMenuHandler(PropertyTree* tree, PropertyRowResourceFilePath* container);

	void onMenuClear();
};

