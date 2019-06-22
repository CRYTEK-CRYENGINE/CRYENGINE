// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Serialization/PropertyTreeLegacy/IDrawContext.h>
#include <Serialization/PropertyTreeLegacy/PropertyRowField.h>
#include <Serialization/PropertyTreeLegacy/PropertyRowString.h>
#include <Serialization/PropertyTreeLegacy/PropertyTreeLegacy.h>
#include <Serialization/PropertyTreeLegacy/PropertyTreeModel.h>
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
	property_tree::Icon   buttonIcon(const PropertyTreeLegacy* tree, int index) const override;
	virtual bool          usePathEllipsis() const override { return true; }
	void                  serializeValue(Serialization::IArchive& ar);
	const void*           searchHandle() const override { return handle_; }
	Serialization::TypeID typeId() const override       { return Serialization::TypeID::get<string>(); }

	bool                  onContextMenu(property_tree::IMenu& menu, PropertyTreeLegacy* tree) override;
private:
	string      filter_;
	string      startFolder_;
	int         flags_;
	const void* handle_;
};

struct ResourceFilePathMenuHandler : PropertyRowMenuHandler
{
	PropertyTreeLegacy*                tree;
	PropertyRowResourceFilePath* self;

	ResourceFilePathMenuHandler(PropertyTreeLegacy* tree, PropertyRowResourceFilePath* container);

	void onMenuClear();
};
