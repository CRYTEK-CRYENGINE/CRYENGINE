// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Serialization/PropertyTreeLegacy/IDrawContext.h>
#include <Serialization/PropertyTreeLegacy/PropertyRowField.h>
#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>
#include <Serialization/PropertyTreeLegacy/PropertyTreeModel.h>
#include <Serialization/PropertyTreeLegacy/Imenu.h>
#include "Serialization.h"

//TODO : this file is in editor package, breaking intended package dependencies
#include "Serialization/Decorators/EntityLink.h"

class PropertyRowEntityLink : public PropertyRowField
{
public:
	~PropertyRowEntityLink();

	void                        pick(PropertyTreeLegacy* tree);
	void                        select();
	void                        clear();

	bool                        isLeaf() const override   { return true; }
	bool                        isStatic() const override { return false; }

	bool                        onActivate(const PropertyActivationEvent& e) override;

	int                         buttonCount() const override     { return 1; }
	virtual property_tree::Icon buttonIcon(const PropertyTreeLegacy* tree, int index) const override;
	virtual bool                usePathEllipsis() const override { return true; }
	string                      valueAsString() const;
	void                        serializeValue(Serialization::IArchive& ar);
	const void*                 searchHandle() const override { return handle_; }
	Serialization::TypeID       typeId() const override       { return type_; }
	bool                        isPicking() const;

	bool                        onContextMenu(IMenu& menu, PropertyTreeLegacy* tree) override;

protected:
	struct Picker;

	CryGUID                 guid_;
	string                  name_;
	std::unique_ptr<Picker> picker_;

	Serialization::TypeID   type_;
	const void*             handle_;
};

struct EntityLinkMenuHandler : PropertyRowMenuHandler
{
public:
	PropertyTreeLegacy*          tree;
	PropertyRowEntityLink* self;

	EntityLinkMenuHandler(PropertyTreeLegacy* tree, PropertyRowEntityLink* container);

	void onMenuPick();
	void onMenuSelect();
	void onMenuDelete();
};
